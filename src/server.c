//
//  main.c
//  Server
//
//  Created on 09/06/21.
//

#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/select.h>
#include <signal.h>

#include "../header/shoco.h"
#include "../header/icl_hash.h"
#include "../header/utils.h"
#include "../header/linkedlist.h"

#define MAXFILES 100
#define BUF_LEN 100

#define MINUS_ONE_EXIT(X,err)														\
if ((X)==-1) {																			\
perror(#err);																			\
exit(EXIT_FAILURE);																	\
}
#define MINUS_ONE_RETURN(X,err,n)													\
if ((X)==-1) {																			\
errno = err;																			\
triggerError();																			\
return n;																				\
}
#define NULL_EXIT(X,err)													\
if ((X)==NULL) {																		\
perror(#err);																			\
triggerError();																		\
exit(EXIT_FAILURE);																	\
}
#define NULL_RETURN(X,n)													\
if ((X)==NULL)		{																	\
	return n;																				\
}
#define NOT_ZERO_ERROR(X,err)														\
if ((X)!=0) {																			\
perror(#err);																			\
exit(EXIT_FAILURE);																	\
}

#define LOCK(l)      if (pthread_mutex_lock(l)!=0)     { \
if (pthread_mutex_lock(l)!=0){ ;		\
perror("Errore fatale durante l'esecuzione del LOCK\n");		    \
pthread_exit((void*)EXIT_FAILURE);			    \
}}
#define UNLOCK(l)    if (pthread_mutex_unlock(l)!=0)     { \
if (pthread_mutex_lock(l)!=0){		\
perror("Errore fatale durante l'esecuzione dell'UNLOCK\n");		    \
pthread_exit((void*)EXIT_FAILURE);			    \
}}

#define WAIT(c,l)    if (pthread_cond_wait(c,l)!=0)       { \
if (pthread_cond_wait(c,l)!=0)       {	\
perror("Errore fatale durante l'esecuzione del WAIT\n");		    \
pthread_exit((void*)EXIT_FAILURE);				    \
}}

#define SIGNAL(c)    if (pthread_cond_signal(c)!=0)       {	\
if (pthread_cond_signal(c)!=0)       {\
perror("Errore fatale durante l'esecuzione del SIGNAL\n");			\
pthread_exit((void*)EXIT_FAILURE);					\
}}

#define MUTEXOPERATION(X,ht)														\
LOCK (&ht);																					\
X;																								\
UNLOCK (&ht);																				\


typedef struct {
	int threadnumber;
	long memorysizebytes;
	int maxfilenumber;
	char* socketname;
	char* logfilepath;
} Settings;

typedef struct {
	long highestfilenum;
	long higheststorageused;
	int filesreplacednum;
} Stats;

typedef struct {
	pthread_t threadid;
	_Bool error;
} threaderrorstruct;

typedef struct fifoqueue {
	serverFile* fileInServer;
	struct fifoqueue* next;
} Fifo;
volatile sig_atomic_t quit = 0;
volatile sig_atomic_t stop = 0;
Stats serverstats = {0};

Settings serversettings = {1,100*1000*1000,1000,NULL};

FILE* fp;

threaderrorstruct threaderrortable[33];
int runningthreads=0;

void sigterm(int signo) {
	switch(signo){
		case SIGINT:
			fprintf(fp,"Ricevuto un SIGINT.\n");
			quit = 1;
			break;
		case SIGQUIT:
			fprintf(fp,"Ricevuto un SIGQUIT.\n");
			quit = 1;
			break;
		case SIGHUP:
			fprintf(fp,"Ricevuto un SIGHUP.\n");
			stop = 1;
			break;
	}
}
static pthread_cond_t	condbuf = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t	bufmtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	servermtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	serverstatsmtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	fifoqueuemtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	requestcountermtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	logfilemtx = PTHREAD_MUTEX_INITIALIZER;

list* clientrequests = NULL;
list* lastqueueelem = NULL;
int requestcounter = 0;

Fifo *fifoqueue = NULL;
Fifo *fifoqueueLastElem = NULL;

int storedFiles = 0;
static long storedBytes = 0;

static icl_hash_t* hashTableFileStorage;
static int pipefd[2];
static fd_set set;

void triggerError(void) {
	int i=1;
	while (threaderrortable[i].threadid==pthread_self())
		i++;
	threaderrortable[i].error=TRUE;
	return;
}

char* serverGetPathname(int connectionfd)
{
	size_t pathlength = PATH_MAX+1;
	char* pathname;
	MINUS_ONE_RETURN(readn(connectionfd, &pathlength, sizeof(pathlength)),EFAULT,NULL);
	if (pathlength>PATH_MAX) {
		triggerError();
		return NULL;
	}
	NULL_RETURN(pathname=malloc(sizeof(char)*pathlength+1),NULL);
	memset(pathname,'\0',(int)pathlength+1);
	MINUS_ONE_RETURN(readn(connectionfd, pathname, pathlength),EFAULT,NULL);
	MUTEXOPERATION(fprintf(fp,"Pathname ricevuto: %s\n",pathname),logfilemtx);
	return pathname;
}

serverFile* serverOpenFile(char* pathname,int connectionfd) {

	NULL_RETURN(pathname,NULL);
	MUTEXOPERATION(fprintf(fp,"Apro il file %s...\n",pathname),logfilemtx);
	serverFile* targetfile;
	MUTEXOPERATION(targetfile = icl_hash_find(hashTableFileStorage, pathname),servermtx);
	if (targetfile==NULL) {
		errno=ENOENT;
		return NULL;
	}
	if (targetfile->isLocked && targetfile->user!=connectionfd) {
		errno=EACCES;
		return NULL;
	}
	if (!targetfile->isOpened) {
		MUTEXOPERATION(fprintf(fp,"Decomprimo il file %s..., dimensione prima della decompressione: %zu bytes\n",pathname,targetfile->compressedsize),logfilemtx);
		char* uncompressedbuffer=malloc(sizeof(char)*targetfile->size);
		memset(uncompressedbuffer, '\0', targetfile->size);
		shoco_decompress(targetfile->filecontent, targetfile->compressedsize, uncompressedbuffer, targetfile->size);
		free(targetfile->filecontent);
		targetfile->filecontent=uncompressedbuffer;
		MUTEXOPERATION(fprintf(fp,"File %s decompresso, il file è tornato alla sua dimensione originale di: %zu bytes\n",pathname,targetfile->size),logfilemtx);
		targetfile->isOpened=TRUE;
	}
	MUTEXOPERATION(fprintf(fp,"File %s aperto.\n",pathname),logfilemtx);
	return targetfile;
}




char* getClientFileContent(int connectionfd, size_t *filesize) {
	MINUS_ONE_RETURN(readn(connectionfd, filesize, sizeof(filesize)), EFAULT, NULL);
	char* filecontent = malloc(sizeof(char)**filesize+1);
	memset(filecontent,'\0',*filesize+1);
	MINUS_ONE_RETURN(readn(connectionfd, filecontent, (int)*filesize), EFAULT, NULL);
	MUTEXOPERATION(fprintf(fp,"Contenuti del file appena ricevuto: %s\n",filecontent),logfilemtx);
	return filecontent;
}
void updateServerStats(size_t dataAdded)
{
	if (dataAdded>0)
		MUTEXOPERATION(fprintf(fp,"Aggiorno le statistiche del server con %zu dati in più:\n",dataAdded),logfilemtx);
	LOCK(&serverstatsmtx);
	storedBytes = storedBytes + (long)dataAdded;
	if(storedBytes > serverstats.higheststorageused)
		serverstats.higheststorageused = storedBytes;
	UNLOCK(&serverstatsmtx);
	MUTEXOPERATION(fprintf(fp,"Statistiche del server aggiornate.\n"),logfilemtx);
	return;
}


void fatalError(char* reason)
{
	MUTEXOPERATION(fprintf(fp,"Si è verificato un errore fatale! Chiusura del server...\n"),logfilemtx);
	fclose(fp);
	size_t strlenght = strlen(reason);
	char* str=malloc(strlen("Errore: ")+strlenght+2);
	strncpy(str, "Errore: ",strlenght);
	perror(strncat(str, reason,strlenght));
	free(str);
	exit(EXIT_FAILURE);
}

int createServerSocket(void)
{
	MUTEXOPERATION(fprintf(fp,"Creazione del socket...\n"),logfilemtx);
	unlink(serversettings.socketname);
	struct sockaddr_un sa;
	sa.sun_family=AF_UNIX;
	strncpy(sa.sun_path,serversettings.socketname,UNIX_PATH_MAX);
	int sk=socket(AF_UNIX,SOCK_STREAM,0);
	if (bind(sk,(struct sockaddr*)&sa, sizeof(sa))<0)
	{
		MUTEXOPERATION(fprintf(fp,"Esco per bind fallita.\n"),logfilemtx);
		exit(EXIT_FAILURE);
	}
	MUTEXOPERATION(fprintf(fp,"Bind riuscita.\n"),logfilemtx);
	listen(sk, SOMAXCONN);
	MUTEXOPERATION(fprintf(fp,"Socket creato.\n"),logfilemtx);
	return sk;
}

void readconfig(const char* configfilepath)
{
	FILE* fPtr=fopen(configfilepath, "r");
	if (fPtr==NULL)
		fatalError("File di configurazione non trovato!\n");
	char* currentsetting = malloc(sizeof(char)*40);
	char* value = malloc(sizeof(char)*PATH_MAX);
	while(!feof(fPtr))
	{
		memset(currentsetting, '\0', 40);
		memset(value, '\0', PATH_MAX);
		if (fscanf(fPtr, "%[^= ]=%s\n",currentsetting,value)==0) {
			free(currentsetting);
			free(value);
			fatalError("Errore nel parsing del file!");
		}
		if (currentsetting==NULL || value==NULL) {
			free(currentsetting);
			free(value);
			fatalError("Il file di configurazione ha uno o più valori non validi!\n");
		}
		else {
			if (!strcmp(currentsetting, "threadnumber"))
			{
				if (isNumeric(value) && atoi(value)<=32 && atoi(value)>0)
					serversettings.threadnumber=atoi(value);
			}
			else if (!strcmp(currentsetting, "memorysize"))
			{
				if (isNumeric(value) && atoi(value)<=500 && atoi(value)>0)
					serversettings.memorysizebytes=atoi(value)*1000000;
			}
			else if (!strcmp(currentsetting, "maxfilenumber"))
			{
				if (isNumeric(value)&& atoi(value)<=1000000 && atoi(value)>0)
					serversettings.maxfilenumber=atoi(value);
			}
			else if (!strcmp(currentsetting, "socketname"))
			{
				serversettings.socketname=malloc(sizeof(char)*PATH_MAX);
				strncpy(serversettings.socketname, value,PATH_MAX);
			}
			else if (!strcmp(currentsetting, "logfilepath"))
			{
				if (value!=NULL) {
					serversettings.logfilepath=malloc(sizeof(char)*PATH_MAX);
					strncpy(serversettings.logfilepath, value,PATH_MAX);
					fp = fopen (serversettings.logfilepath, "w");
					fprintf(fp, "Inizio file di log del server\n");
				}
			}
			else
				printf("Configurazione \"%s\" del server non supportata, verrà ignorata",currentsetting);
		}
	}
	free(currentsetting);
	free(value);
	fclose(fPtr);
	if (serversettings.socketname==NULL)
		fatalError("Non è stato specificato alcun file del socket!\n");
	if (fp==NULL)
		fp = fopen ("serverLog.log", "w");
	fprintf(fp, "Numero di thread worker impostato su: %d\n",serversettings.threadnumber);
	fprintf(fp, "Dimensione della memoria impostata su: %zu MB\n",serversettings.memorysizebytes/1000000);
	fprintf(fp, "Numero massimo di file memorizzabili impostato su: %d\n",serversettings.maxfilenumber);
	fprintf(fp, "Nome del file del socket: %s\n",serversettings.socketname);
	return;
}
int serverCloseFile(serverFile* targetfile, _Bool serverclosing)
{
	MUTEXOPERATION(fprintf(fp,"Sto per chiudere il file %s\n",targetfile->path),logfilemtx);
	if (targetfile == NULL || (targetfile->isLocked && serverclosing==FALSE))
		return -1;

	if (targetfile->isOpened==TRUE && serverclosing==FALSE) {
		MUTEXOPERATION(fprintf(fp,"Comprimo il file %s..., dimensione prima della compressione: %zu bytes\n",targetfile->path,targetfile->size),logfilemtx);
		targetfile->compressedsize=targetfile->size/2;
		char* compressedbuffer=malloc(sizeof(char)*targetfile->compressedsize);
		memset(compressedbuffer, '\0', targetfile->compressedsize);
		while(shoco_compress(targetfile->filecontent, targetfile->size, compressedbuffer, targetfile->compressedsize)==targetfile->compressedsize+1) {
			targetfile->compressedsize+=100;
			compressedbuffer=realloc(compressedbuffer,sizeof(char)*targetfile->compressedsize);
			memset(compressedbuffer, '\0', targetfile->compressedsize);
		}
		targetfile->isOpened=FALSE;
		free(targetfile->filecontent);
		targetfile->filecontent=compressedbuffer;
		MUTEXOPERATION(fprintf(fp,"File %s compresso, dimensione attuale: %zu bytes\n",targetfile->path,targetfile->compressedsize),logfilemtx);
	}
	MUTEXOPERATION(fprintf(fp,"File %s chiuso!\n",targetfile->path),logfilemtx);
	return 0;
}
int serverRemoveFile(serverFile* targetfile, _Bool serverclosing)
{
	MUTEXOPERATION(fprintf(fp,"Sto per rimuovere il file %s\n",targetfile->path),logfilemtx);
	if (targetfile==NULL || (targetfile->isLocked && serverclosing==FALSE))
		return -1;
	if (targetfile->isOpened)
		if (serverCloseFile(targetfile,serverclosing)==-1)
			return -1;
	MUTEXOPERATION(int retval = icl_hash_delete(hashTableFileStorage, targetfile->path, NULL, NULL), servermtx);
	if (retval==0) {
	MUTEXOPERATION(fprintf(fp,"File %s rimosso correttamente!\n",targetfile->path),logfilemtx);
	Fifo* prev=fifoqueue;
	Fifo* curr=fifoqueue->next;
	if (prev->fileInServer==targetfile) { //Se è il primo della lista
		fifoqueue=fifoqueue->next;
		free(targetfile->filecontent);
		free(targetfile->path);
		free(prev->fileInServer);
		free(prev);
		return retval;
	}
	while (curr->fileInServer!=targetfile) {
		prev=prev->next;
		curr=curr->next;
	}
	if (curr!=NULL)
		prev->next=curr->next;
	else
		prev->next=NULL;
	free(targetfile->filecontent);
	free(targetfile->path);
	free(curr->fileInServer);
	free(curr);
	storedFiles-=1;
	return retval;
	}
	MUTEXOPERATION(fprintf(fp,"Errore nella rimozione del file %s!\n",targetfile->path),logfilemtx);
	return retval;
}

int fifo_insert(serverFile *serverFileToInsert) {
	LOCK(&fifoqueuemtx);
	Fifo *new = malloc(sizeof(Fifo));
	if(!new) {
		perror("malloc");
		return -1;
	}
	new->fileInServer = serverFileToInsert;
	new->next = NULL;
	if(fifoqueueLastElem == NULL) { //se la lista era vuota l'ultimo elemento diventa anche il primo
		fifoqueueLastElem = new;
		fifoqueue = new;
		UNLOCK(&fifoqueuemtx);
		return 0;
	}
	fifoqueueLastElem->next=new; //altrimenti aggiungo in coda (con lastelem evito di scorrere tutta la lista per trovare l'ultimo)
	fifoqueueLastElem=new;
	UNLOCK(&fifoqueuemtx);
	return 0;
}

serverFile* serverCreateFile(char* pathname, int connectionfd)
{
	MUTEXOPERATION(fprintf(fp,"Creazione del file %s...\n",pathname),logfilemtx);
	NULL_RETURN(pathname, NULL);
	MUTEXOPERATION(serverFile* tmp = icl_hash_find(hashTableFileStorage, pathname),servermtx);
	if(tmp != NULL) {
		errno = EEXIST;
		MUTEXOPERATION(fprintf(fp,"Impossibile creare il file %s, il file è già esistente nel server!\n",pathname),logfilemtx);
		return NULL;
	}
	tmp = malloc(sizeof(serverFile));
	tmp->filemtx = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	tmp->filelockmtx = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	tmp->filelockcond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
	LOCK(&tmp->filemtx);
	size_t pathlength=strlen(pathname);
	tmp->path = malloc(sizeof(char)*pathlength+1);
	strncpy(tmp->path,pathname,pathlength+1);
	tmp->isOpened = TRUE;
	tmp->isLocked = FALSE;
	tmp->compressedsize = 0;
	tmp->user = connectionfd;
	UNLOCK(&tmp->filemtx);
	icl_entry_t* tmp2;
	MUTEXOPERATION((tmp2 = icl_hash_insert(hashTableFileStorage, tmp->path, tmp)), servermtx);
	if (tmp2==NULL) {
		free (tmp->path);
		return NULL;
	}
	fifo_insert(tmp);
	storedFiles+=1;
	if (storedFiles>serverstats.highestfilenum)
		serverstats.highestfilenum=storedFiles;
	MUTEXOPERATION(fprintf(fp,"Il file %s è stato creato correttamente.\n",pathname),logfilemtx);
	return tmp;
}


int serverSendFile(serverFile* targetfile, int connectionfd) {
	MUTEXOPERATION(fprintf(fp,"Sto per inviare il file %s al client\n",targetfile->path),logfilemtx);
	if (targetfile==NULL || targetfile->isLocked || !targetfile->isOpened)
	{
		size_t pathlength=0;
		MINUS_ONE_RETURN(writen(connectionfd, &pathlength, sizeof(pathlength)), EFAULT,-1);
		return 0;
	}
	size_t pathlength = strlen(targetfile->path);
	MINUS_ONE_RETURN(writen(connectionfd, &pathlength, sizeof(pathlength)), EFAULT, -1);
	MINUS_ONE_RETURN(writen(connectionfd, targetfile->path, (int)pathlength), EFAULT, -1);
	MINUS_ONE_RETURN(writen(connectionfd, &targetfile->size, sizeof(size_t)), EFAULT, -1);
	MINUS_ONE_RETURN(writen(connectionfd, targetfile->filecontent, (int)targetfile->size), EFAULT, -1);
	MUTEXOPERATION(fprintf(fp,"Il file %s è stato inviato correttamente.\n",targetfile->path),logfilemtx);
	return 0;
}

serverFile* serverFindFile(char* pathname) {
	MUTEXOPERATION(fprintf(fp,"Cerco il file %s...\n",pathname),logfilemtx);
	NULL_RETURN((pathname),NULL);
	serverFile* targetfile;
	MUTEXOPERATION(targetfile = icl_hash_find(hashTableFileStorage, pathname), servermtx);
	if(targetfile == NULL) {
		errno=ENOENT;
		MUTEXOPERATION(fprintf(fp,"Il file %s non è attualmente memorizzato nel server.\n",pathname),logfilemtx);
		return NULL;
	}
	MUTEXOPERATION(fprintf(fp,"Il file %s è stato trovato nel server.\n",pathname),logfilemtx);
	return targetfile;
}

int serverReadNFiles(int connectionfd) {
	int N;
	MINUS_ONE_RETURN(readn(connectionfd, &N, sizeof(int)),EFAULT, -1);
	if (N>0)
		MUTEXOPERATION(fprintf(fp,"Sto per leggere %d file.\n",N),logfilemtx);
	Fifo *tmp = fifoqueue;
	while(N != 0 || tmp != NULL) {
		if(!tmp->fileInServer->isOpened && !tmp->fileInServer->isLocked) {
			MINUS_ONE_RETURN(serverSendFile(tmp->fileInServer, connectionfd),EFAULT, -1);
		}
		N--;
		tmp=tmp->next;
	}
	MUTEXOPERATION(fprintf(fp,"Tutti i file sono stati letti.\n"),logfilemtx);
	return 0;
}



size_t replacementFIFO(int connectionfd)
{
	MUTEXOPERATION(fprintf(fp,"Avvio la procedura di rimpiazzamento file.\n"),logfilemtx);
	serverstats.filesreplacednum++;
	size_t filesize = 0;
	Fifo *curr = fifoqueue;
	size_t pathlength;
	LOCK(&fifoqueuemtx);
	while(curr != NULL && curr->fileInServer->isLocked){ //se è lockato non posso inviarlo
		curr = curr->next;
	}
	if (curr!=NULL) {
			if (!curr->fileInServer->isOpened)
				serverOpenFile(curr->fileInServer->path, connectionfd);
			LOCK(&curr->fileInServer->filemtx);
			filesize = curr->fileInServer->size;
			MUTEXOPERATION(fprintf(fp,"Invierò al client il file %s. Il suo contenuto è di %zu bytes\n",curr->fileInServer->path,curr->fileInServer->size),logfilemtx);
			if(fifoqueue == NULL)
				fifoqueueLastElem = NULL;
			pathlength=strlen(curr->fileInServer->path);
			if(writen(connectionfd, &pathlength, sizeof(size_t))==-1 || writen(connectionfd, curr->fileInServer->path, pathlength) == -1 || writen(connectionfd, &filesize, sizeof(size_t)) == -1 || writen(connectionfd, curr->fileInServer->filecontent, filesize) == -1)
				{
				triggerError();
				UNLOCK(&curr->fileInServer->filemtx);
				UNLOCK(&fifoqueuemtx);
				MUTEXOPERATION(fprintf(fp,"Errore nell'invio del file %s!\n",curr->fileInServer->path),logfilemtx);
				return -1;
			}
			MUTEXOPERATION(fprintf(fp,"File %s inviato al client.\n",curr->fileInServer->path),logfilemtx);
			UNLOCK(&curr->fileInServer->filemtx);
			if (serverRemoveFile(curr->fileInServer, FALSE) == -1) {
				return -1;
			}

	}

	UNLOCK(&fifoqueuemtx);
	return filesize;
}
int replaceServerFilesChecker(int connectionfd, size_t filesize)
{
	MUTEXOPERATION(fprintf(fp,"Controllo se c'è da utilizzare l'algoritmo di rimpiazzamento ...\n"),logfilemtx);
	LOCK(&serverstatsmtx);
	int serverAnswer=-1;
	if (filesize>serversettings.memorysizebytes) { //non sto nemmeno a provare il rimpiazzamento se tanto risulterà impossibile
		UNLOCK(&serverstatsmtx);
		return ERROR;
	}
	_Bool replacedflag = FALSE;
	while (1) {
		if ((serversettings.memorysizebytes-storedBytes)<filesize || serversettings.maxfilenumber<storedFiles) {
			MUTEXOPERATION(fprintf(fp,"Procedo con il rimpiazzamento\n"),logfilemtx);
			serverAnswer=ERROR_NOSPACELEFT; //con ERROR_NOSPACELEFT significa che provo il rimpiazzamento, con ERROR significa che ho fallito anche quello e non c'è modo di fare spazio
			replacedflag=TRUE;
		}
		else
		{
			if (replacedflag) {
				MUTEXOPERATION(fprintf(fp,"Il rimpiazzamento è andato a termine!\n"),logfilemtx);
			}
			else {
				MUTEXOPERATION(fprintf(fp,"Non c'è bisogno di usare l'algoritmo di rimpiazzamento.\n"),logfilemtx);
			}
			serverAnswer=SUCCESS;
			break;
		}
		MINUS_ONE_RETURN(writen(connectionfd, &serverAnswer, sizeof(int)),EFAULT, -1);
		size_t freedBytes=replacementFIFO(connectionfd);
		if (freedBytes<=0) {
			size_t pathlength=0;
			MINUS_ONE_RETURN(writen(connectionfd, &pathlength, sizeof(int)),EFAULT, -1);
			MUTEXOPERATION(fprintf(fp,"Non si può più fare spazio nel server!\n"),logfilemtx);
			serverAnswer=ERROR;
			errno=ENOMEM;
			break;
		}
		storedBytes-=freedBytes;
		storedFiles-=1;
	}
	MINUS_ONE_RETURN(writen(connectionfd, &serverAnswer, sizeof(int)),EFAULT, -1);
	UNLOCK(&serverstatsmtx);
	return serverAnswer;
}
int serverAppend(serverFile* targetfile, int connectionfd){
	if (targetfile==NULL) {
		errno=ENOENT;
		return -1;
	}
	if (!targetfile->isOpened || (targetfile->isLocked && targetfile->user!=connectionfd)) {
		errno=EACCES;
		return -1;
	}
	MUTEXOPERATION(fprintf(fp,"Append del file %s\n",targetfile->path),logfilemtx);
	size_t newDataLength;
	char* datatoappend = getClientFileContent(connectionfd,&newDataLength);
	if (strlen(datatoappend)<_POSIX_PIPE_BUF) {//garantisco che la scrittura sia atomica, in caso contrario non scrivo niente e fallisco
		newDataLength = (datatoappend != NULL)? strlen(datatoappend) : 0;
	MINUS_ONE_RETURN(replaceServerFilesChecker(connectionfd, targetfile->size+newDataLength),EFAULT, -1);
	LOCK(&targetfile->filemtx);
	NULL_RETURN((targetfile->filecontent = realloc(targetfile->filecontent, targetfile->size+strlen(datatoappend))),-1);
	char *memtmp = (char*)targetfile->filecontent;
	memcpy((void*)&memtmp[targetfile->size], datatoappend, newDataLength);
	targetfile->size = targetfile->size + (int)newDataLength;
	UNLOCK(&targetfile->filemtx);
		updateServerStats(newDataLength);
		free(datatoappend);
		MUTEXOPERATION(fprintf(fp,"Append del file %s terminato correttamente.\n",targetfile->path),logfilemtx);
		return 0;
	}
	return -1;
}
int serverLockFile(serverFile* targetfile, int connectionfd){
	if (targetfile==NULL) {
		errno=ENOENT;
		return -1;
	}
	MUTEXOPERATION(fprintf(fp,"Lock del file %s...\n",targetfile->path),logfilemtx);
	if (!targetfile->isOpened) {
		MUTEXOPERATION(fprintf(fp,"Impossibile eseguire l'operazione sul file %s perché risulta chiuso!\n",targetfile->path),logfilemtx);
		errno=EPERM;
		return -1;
	}
	if (targetfile->isLocked && targetfile->user!=connectionfd) {
		MUTEXOPERATION(fprintf(fp,"Il file %s risulta attualmente già lockato da un utente, mi metto in attesa...\n",targetfile->path),logfilemtx);
		WAIT(&targetfile->filelockcond,&targetfile->filelockmtx);
	}
	LOCK(&targetfile->filemtx);
	targetfile->isLocked=TRUE;
	targetfile->user=connectionfd;
	UNLOCK(&targetfile->filemtx);
	MUTEXOPERATION(fprintf(fp,"Lock del file %s riuscita!\n",targetfile->path),logfilemtx);
	return 0;
}
int serverUnlockFile(serverFile* targetfile, int connectionfd)
{
	if (targetfile==NULL) {
		errno=ENOENT;
		return -1;
	}
	MUTEXOPERATION(fprintf(fp,"Unlock del file %s...\n",targetfile->path),logfilemtx);
	if (!targetfile->isOpened) {
	MUTEXOPERATION(fprintf(fp,"Impossibile eseguire l'operazione sul file %s perché risulta chiuso!\n",targetfile->path),logfilemtx);
		errno=EPERM;
		return -1;
	}
	if (targetfile->isLocked && targetfile->user!=connectionfd) {
		MUTEXOPERATION(fprintf(fp,"Solo lo stesso utente che ha lockato il file %s può unlockarlo!\n",targetfile->path),logfilemtx);
		errno=EACCES;
		return -1;
	}
	MUTEXOPERATION(targetfile->isLocked=FALSE,targetfile->filemtx);
	MUTEXOPERATION(fprintf(fp,"Lock del file %s riuscita!\n",targetfile->path),logfilemtx);
	return 0;
}
int serverWriteFile(serverFile* targetfile, int connectionfd) {
	size_t datasize;
	char* targetdata;
	if ((targetdata = getClientFileContent(connectionfd,&datasize))==NULL) {
		free (targetdata);
		return -1;
	}
	if (targetfile==NULL) {
		free(targetdata);
		return -1;
	}
	MUTEXOPERATION(fprintf(fp,"Scrivo i contenuti nel file %s...\n",targetfile->path),logfilemtx);
	if (replaceServerFilesChecker(connectionfd, datasize)==-1) {
		free(targetdata);
		serverRemoveFile(targetfile,FALSE);
		return -1;
	}
	targetfile->size = datasize;
	targetfile->filecontent = targetdata;
	updateServerStats(targetfile->size);
	MUTEXOPERATION(fprintf(fp,"Salvataggio dei contenuti del file %s riuscito.\n",targetfile->path),logfilemtx);
	return 0;
}

int updateMax(fd_set set, int fdmax) {
	for(int i=(fdmax-1);i>=0;--i)
		if (FD_ISSET(i, &set)) return i;
	
	return -1;
}
void* workerThread(void* args)
{
	threaderrortable[runningthreads].threadid=pthread_self();
	threaderrortable[runningthreads].error=FALSE;
	while (!quit)
	{
		int currentfd = 0;
		LOCK(&bufmtx);
		if (quit)
		{
			UNLOCK(&bufmtx);
			return NULL;
		}
		LOCK(&requestcountermtx);
		while (requestcounter==0)
		{
			UNLOCK(&requestcountermtx);
			WAIT(&condbuf, &bufmtx)
			if (quit)
			{
				UNLOCK(&bufmtx);
				SIGNAL(&condbuf);
				return NULL;
			}
			LOCK(&requestcountermtx);
		}
		requestcounter--;
		UNLOCK(&requestcountermtx);
		MUTEXOPERATION(fprintf(fp,"Nuova richiesta arrivata.\n"),logfilemtx);
		if ((currentfd=removeNode(&clientrequests, &lastqueueelem))==-1)
		{UNLOCK(&bufmtx); exit(EXIT_FAILURE);}
		UNLOCK(&bufmtx);
		SIGNAL(&condbuf);
		if(currentfd == -2){
			return NULL;
		}
		int clientcoderequest = -1;
		
		if(readn(currentfd, &clientcoderequest, sizeof(int)) == -1)
			clientcoderequest = -1;
		if(clientcoderequest==CLOSECONN)
			close(currentfd);
		else {
			int serverAnswer = -1;
			char* pathname = serverGetPathname(currentfd);
			switch((int)clientcoderequest){
				case OPENFILE:
					serverAnswer = (serverOpenFile(pathname,currentfd)!=NULL)? SUCCESS : ERROR;
					break;
				case OPENCREATE:
					serverAnswer = (serverCreateFile(pathname, currentfd)!=NULL)? SUCCESS : ERROR;
					break;
				case OPENLOCK:
					serverAnswer = (serverLockFile(serverOpenFile(pathname,currentfd),currentfd)!=-1)? SUCCESS : ERROR;
					break;
				case OPENLOCKCREATE:
					serverAnswer = (serverLockFile(serverCreateFile(pathname, currentfd),currentfd)!=-1)? SUCCESS : ERROR;
					break;
				case READ:
					serverAnswer = (serverSendFile(serverFindFile(pathname), currentfd)!=-1)? SUCCESS : ERROR;
					break;

				case READN:
					serverAnswer = (serverReadNFiles(currentfd)!=-1)? SUCCESS : ERROR;
					break;

				case APPEND:
					serverAnswer = (serverAppend(serverFindFile(pathname), currentfd)!=-1)? SUCCESS: ERROR;
					break;

				case LOCKFILE:
					serverAnswer = (serverLockFile(serverFindFile(pathname), currentfd)!=-1)? SUCCESS : ERROR;
					break;

				case UNLOCKFILE:
					serverAnswer = (serverUnlockFile(serverFindFile(pathname), currentfd)!=-1)? SUCCESS : ERROR;
					break;

				case CLOSEFILE:
					serverAnswer = (serverCloseFile(serverFindFile(pathname),FALSE)!=-1)? SUCCESS : ERROR;
					break;

				case REMOVE:
					serverAnswer = (serverRemoveFile(serverFindFile(pathname),FALSE)!=-1)? SUCCESS : ERROR;
					break;

				case WRITE:
					serverAnswer = (serverWriteFile(serverCreateFile(pathname, currentfd), currentfd)!=-1)? SUCCESS : ERROR;
					break;
				default:
					break;

			}
			free(pathname);
			int i=1;
			while (threaderrortable[i].threadid==pthread_self())
				i++;
			if (threaderrortable[i].error)
				close(currentfd);
			else {
				writen(currentfd, &serverAnswer, sizeof(serverAnswer));
				if (serverAnswer==-1)
					writen(currentfd, &errno, sizeof(errno));
				if(write(pipefd[1], &currentfd, sizeof(int)) == -1)
					close(currentfd);
			}
		}



	}
	
	return NULL;
}




int main(int argc, const char * argv[]) {
	if (argc!=2)
		fatalError("Argomenti non validi! Passare come argomento solo il percorso del file di configurazione!\n");
	struct sigaction s;
	printf("Avvio del server...\n");
	s.sa_handler = sigterm;
	sigemptyset(&s.sa_mask);
	s.sa_flags = SA_RESTART;
	sigaction(SIGINT, &s, NULL);
	sigaction(SIGQUIT, &s, NULL);
	sigaction(SIGHUP, &s, NULL);
	signal(SIGINT, sigterm);
	signal(SIGQUIT, sigterm);
	signal(SIGHUP, sigterm);
	readconfig(argv[1]);
	hashTableFileStorage = icl_hash_create(100, NULL, NULL);
	pthread_t* threadids;
	NULL_EXIT(threadids=malloc(serversettings.threadnumber*sizeof(pthread_t)), "Impossibile creare la struttura per i thread!");
	for(int i=0; i<serversettings.threadnumber && i<=32; i++)
		threadids[i] = 0;
	int listenerfd = createServerSocket();
	if (pipe(pipefd)==-1) {
		free(threadids);
		fatalError("Errore nella creazione della pipe!");
	}
	fd_set tempset;
	FD_ZERO(&set);
	FD_ZERO(&tempset);
	FD_SET(pipefd[0],&set);
	FD_SET(listenerfd,&set);
	int maxfd = listenerfd;
	printf("Server pronto\n");
	while (!stop && !quit)
	{
		tempset=set;
		if (!quit){
			if (!stop) {
				if (select(maxfd+1, &tempset, NULL, NULL, NULL)== -1) //metto fdmax+1 perché non voglio l'indice massimo degli FD, ma il numero di FD attivi
				{
					if (errno==EINTR)
						continue;
					else
					{
						perror("Select fallita!");
						free(threadids);
						return -1;
					}
				}
			}
		}
		if (quit)
			break;
		if (stop)
			break;
		for (int i=0;i<maxfd+1;i++) //cerco chi è che ha fatto una richiesta
		{
			if (FD_ISSET(i,&tempset)) //guardo se è una nuova richiesta di connessione
			{
				int connectionfd;
				if (i==listenerfd)
				{
					MINUS_ONE_EXIT(connectionfd=accept(listenerfd, NULL, NULL), "Errore durante l'accept");
					FD_SET(connectionfd,&set);
					if (runningthreads<serversettings.threadnumber)
					{
						NOT_ZERO_ERROR(pthread_create(&threadids[runningthreads], NULL, workerThread, NULL), "Impossibile creare thread");
						runningthreads++;
					}
					if (connectionfd>maxfd)
						maxfd=connectionfd;

				}
				else if (i==pipefd[0]) //magari non era una richiesta, ma una scrittura sulla pipe
				{
					int pipereaderfd;
					MINUS_ONE_EXIT(read(pipefd[0], &pipereaderfd, sizeof(int)), "Errore nella lettura dalla pipe");
					FD_SET(pipereaderfd,&set);
					if (pipereaderfd>maxfd)
						maxfd=pipereaderfd;
					continue;
				}
				else
				{
					connectionfd=i; // se si tratta di una nuova richiesta da parte di un client che era già connesso
					int try=0;
					
					FD_CLR(connectionfd,&set);
					if (connectionfd==maxfd)
						maxfd=updateMax(set,maxfd);
					LOCK(&bufmtx);
					int result;
					while ((result = insertNode(connectionfd,&lastqueueelem,&clientrequests)) < 0 && try < 3)
						try++;
					if(result < 0) {
						UNLOCK(&bufmtx);
						close(connectionfd);
					}
					else
					{
						UNLOCK(&bufmtx);
						MUTEXOPERATION(requestcounter++, requestcountermtx);
						SIGNAL(&condbuf); //invio il segnale che sveglia thread fermi nella WAIT
					}
				}
			}
			
		}
	}
	if (stop)
	{
		LOCK(&bufmtx);
		for (int i=0;i<runningthreads;i++)
		{
			MINUS_ONE_EXIT((insertNode(-2, &lastqueueelem, &clientrequests)),"errore");
			requestcounter++;
		}
		UNLOCK(&bufmtx);
	}
	SIGNAL(&condbuf);
	for (int i=0;i<runningthreads;i++) {
		NOT_ZERO_ERROR(pthread_join(threadids[i], NULL), "Non riesco a joinare un thread.");
	}
	free(threadids);
	printf("il numero massimo di file memorizzati nel server è stato %lu \n",serverstats.highestfilenum);
	printf("il numero massimo di spazio utilizzato nel server è stato %lu \n",serverstats.higheststorageused);
	printf("l'algoritmo di rimpiazzamento è stato usato %d volte\n",serverstats.filesreplacednum);
	printf("Lista dei file memorizzati nel server al momento della chiusura:\n");
	while(fifoqueue!=NULL) {
		printf("%s\n",fifoqueue->fileInServer->path);
		serverRemoveFile(fifoqueue->fileInServer,TRUE);
	}

	icl_hash_destroy(hashTableFileStorage, NULL, NULL);
	free(serversettings.socketname);
	free(serversettings.logfilepath);
	printf("Chiusura server\n");
	fprintf(fp,"Chiusura server");
	fclose(fp);
	return 0;
}
