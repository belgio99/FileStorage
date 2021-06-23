//
//  main.c
//  Server
//
//  Created on 09/06/21.
//

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
#define MINUS_ONE_RETURN(X,err)													\
if ((X)==-1) {																			\
perror(#err);																			\
return -1;																				\
}
#define CHECK_MALLOC(X)																\
if ((X)==NULL) {																		\
perror("Malloc fallita!");															\
return -1;																				\
}
#define NEGATIVE_RETURN(X,err)													\
if ((X)<=0) {																			\
perror(#err);																			\
return NULL;																			\
}
#define NULL_RETURN(X,err)															\
if ((X)==NULL) {																		\
perror(#err);																			\
return NULL;																			\
}
#define NULL_EXIT_ERROR(X,err)													\
if ((X)==NULL) {																		\
perror(#err);																			\
exit(EXIT_FAILURE);																	\
}
#define NOT_ZERO_ERROR(X,err)														\
if ((X)!=0) {																			\
perror(#err);																			\
exit(EXIT_FAILURE);																	\
}
#define NOT_ZERO_ERROR_UNLOCK(X,err,tounlock)								\
if ((X)!=0) {																			\
UNLOCK(tounlock);																		\
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

typedef struct {
	int threadnumber;
	long memorysizebytes;
	int maxfilenumber;
	char* socketname;
} Settings;

typedef struct {
	int filestorednum;
	int currentstoragesize;
	long highestfilenum;
	long higheststorageused;
	int filesreplacednum;
} Stats;

typedef struct fifoqueue {
	serverFile fileInServer;
	struct fifoqueue* next;
} Fifo;
volatile sig_atomic_t quit = 0;
volatile sig_atomic_t stop = 0;
Stats serverstats = {0};

Settings serversettings = {1,100*1000*1000,1000,NULL};

listfiles *filesopened = NULL;

void sigterm(int signo) {
	switch(signo){
		case SIGINT:
			quit = 1;
			break;
		case SIGQUIT:
			quit = 1;
			break;
		case SIGHUP:
			stop = 1;
			break;
	}
}
static pthread_cond_t	olockcond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t	condbuf = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t	bufmtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	servermtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	serverstatsmtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	filesopenedmtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	fifoqueuemtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	requestcountermtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	olockmutex = PTHREAD_MUTEX_INITIALIZER;

list* clientrequests = NULL;
list* lastqueueelem = NULL;
int requestcounter = 0;

Fifo *fifoqueue = NULL;
Fifo *fifoqueueLast = NULL;

static long storedFiles = 0;
static long storedBytes = 0;

static icl_hash_t* hashTableFileStorage;
static icl_hash_t* filesOpened;
static int pipefd[2];
static fd_set set;

serverFile* serverOpenFile(int openFlags, int connectionfd)
{
	size_t pathsize;
	char* pathname = NULL;
	serverFile* tmp = NULL;
	int serverAnswer = 0;
	int request = 0;
	MINUS_ONE_EXIT(readn(connectionfd, &request, sizeof(int)), "Impossibile ricevere la richiesta dal client");
	NEGATIVE_RETURN(readn(connectionfd, &pathsize, sizeof(pathsize)),"errore: questo dato non può essere negativo");
	NULL_RETURN(pathname=malloc(sizeof(char)*pathsize),"Malloc fallita");
	memset(pathname,'\0',pathsize);
	NEGATIVE_RETURN(readn(connectionfd, pathname, pathsize),"errore: questo dato non può essere negativo");
	UNLOCK(&servermtx);
	switch(openFlags)
		{
			case OPENFILE:
			if((tmp = icl_hash_find(hashTableFileStorage, pathname)) == NULL)
				serverAnswer = ERROR_FILENOTFOUND;
				else
					serverAnswer=SUCCESS_OPEN;
			break;
			case OPENCREATE:
			LOCK(&servermtx);
			if((tmp = icl_hash_find(hashTableFileStorage, pathname)) == NULL)
				{
				UNLOCK(&servermtx);
				//free(path);
				serverAnswer = SUCCESS_OPENCREATE;
				}
			break;
			case OPENLOCK:
			LOCK(&servermtx);
			if((tmp = icl_hash_find(hashTableFileStorage, pathname)) == NULL)
				{
				UNLOCK(&servermtx);
				//free(path);
				serverAnswer = ERROR_FILENOTFOUND;
				}
			else
				{
				LOCK(&tmp->filemtx);
				UNLOCK(&servermtx);
				if(tmp->isOpened)
					{
					serverAnswer = ERROR_FILEALREADY_OPENED;
					free(pathname);
					return NULL;
					}
				else
					{
					tmp->isLocked = TRUE;
					tmp->user = connectionfd;
					}
				UNLOCK(&tmp->filemtx);
				}
			UNLOCK(&servermtx);
			serverAnswer=SUCCESS_OPENLOCK;
			break;
			
			case OPENLOCKCREATE:
			
			LOCK(&servermtx);
			if((tmp = icl_hash_find(hashTableFileStorage, pathname)) == NULL)
				serverAnswer=SUCCESS_OPENCREATE;
			else {
				serverAnswer=ERROR_FILEALREADY_EXISTS;
				serverAnswer = -1;
				free(pathname);
				return NULL;
			}
			UNLOCK(&servermtx);
			
			break;
		}
	
	if(serverAnswer==SUCCESS_OPENCREATE || serverAnswer==SUCCESS_OPENLOCKCREATE)
		{
		tmp=malloc(sizeof(serverFile));
		tmp->path=malloc(sizeof(char)*pathsize);
		strcpy(tmp->path,pathname);
		pthread_mutex_init(&tmp->filemtx,NULL);
		LOCK(&tmp->filemtx);
		if(openFlags==OPENLOCK || openFlags==OPENLOCKCREATE)
			tmp->isLocked=TRUE;
		else
			tmp->isLocked=FALSE;
		tmp->isOpened = TRUE;
		tmp->user = connectionfd;
		LOCK(&servermtx);
		icl_hash_insert(hashTableFileStorage, tmp->path, tmp);
		UNLOCK(&servermtx);


		LOCK(&filesopenedmtx);
		insertInList(connectionfd, &tmp, &filesopened);
		icl_hash_insert(filesOpened, tmp->path, tmp);
		UNLOCK(&filesopenedmtx);
		UNLOCK(&tmp->filemtx);
		}
	if (serverAnswer==SUCCESS_OPEN && !tmp->isLocked)
	{
		tmp->isOpened=TRUE;
		tmp->user=connectionfd;
	}
	if(writen(connectionfd, &serverAnswer, sizeof(int)) == -1){
		free(pathname);
		UNLOCK(&servermtx);
		return NULL;
	}
	UNLOCK(&servermtx);
	free(pathname);
	return tmp;
}
int serverCloseFile(int connectionfd, char* key)
{
	if (key!=NULL)
		{
		serverFile* tmp=icl_hash_find(hashTableFileStorage, key);
		tmp->isOpened=FALSE;
		return icl_hash_delete(filesOpened, key, NULL, NULL);
		}
	size_t pathsize;
	char* path = NULL;
	MINUS_ONE_RETURN(readn(connectionfd, &pathsize, sizeof(pathsize)),"errore: questo dato non può essere negativo");
	CHECK_MALLOC(path = malloc(pathsize));
	memset(path,'\0',pathsize);
	MINUS_ONE_RETURN(readn(connectionfd, path, pathsize),"errore: questo dato non può essere negativo");
	int returnval = icl_hash_delete(filesOpened, path, NULL, NULL);
	free(path);
	return returnval;
}

size_t serverReadNFiles(int connectionfd){
	
	int N;
	MINUS_ONE_RETURN(readn(connectionfd, &N, sizeof(int)),"Errore durante la lettura dal client");
	size_t filesize = 0;
	LOCK(&servermtx);
	LOCK(&fifoqueuemtx);
	Fifo *tmp = fifoqueue;
	while(N != 0){
		if(tmp == NULL)
			break;
		UNLOCK(&tmp->fileInServer.filemtx);
		LOCK(&tmp->fileInServer.filemtx);
		if(tmp->fileInServer.isOpened == FALSE){
			filesize = (int)tmp->fileInServer.size;
			size_t pathlength=strlen(tmp->fileInServer.path);
			if(writen(connectionfd, &pathlength, sizeof(size_t)) == -1)
				{
				UNLOCK(&tmp->fileInServer.filemtx);
				UNLOCK(&fifoqueuemtx);
				UNLOCK(&servermtx);

				return -1;
				}
			if(writen(connectionfd, tmp->fileInServer.path, pathlength) == -1)
				{
				UNLOCK(&tmp->fileInServer.filemtx);
				UNLOCK(&fifoqueuemtx);
				UNLOCK(&servermtx);

				return -1;
				}
			if(writen(connectionfd, &filesize, sizeof(filesize)) == -1)
				{
				UNLOCK(&tmp->fileInServer.filemtx);
				UNLOCK(&fifoqueuemtx);
				UNLOCK(&servermtx);

				return -1;
				}
			
			if(writen(connectionfd, tmp->fileInServer.filecontent, filesize) == -1)
				{
				UNLOCK(&tmp->fileInServer.filemtx);
				UNLOCK(&fifoqueuemtx);
				UNLOCK(&servermtx);

				return -1;
				}
			N--;
		}
		UNLOCK(&tmp->fileInServer.filemtx);
		tmp=tmp->next;
	}
	
	UNLOCK(&fifoqueuemtx);
	UNLOCK(&servermtx);
	
	size_t zeroexit = 0;
	writen(connectionfd, &zeroexit, sizeof(size_t));
	if (N==0 || tmp==NULL)
		return 0;
	return -1;

}


int fifo_insert(serverFile *serverFileoInsert){
	LOCK(&fifoqueuemtx);
	Fifo *new = malloc(sizeof(Fifo));
	if(!new){
		perror("malloc");
		return -1;
	}
	new->fileInServer = *serverFileoInsert;
	new->next = NULL;
	if(fifoqueueLast == NULL){
		fifoqueueLast = new;
		fifoqueue = new;
		UNLOCK(&fifoqueuemtx);
		return 0;
	}
	fifoqueueLast->next = new;
	fifoqueueLast = new;
	UNLOCK(&fifoqueuemtx);
	return 0;
}

size_t replacementFIFO(int connectionfd)
{
	serverstats.filesreplacednum++;
	size_t filesize = 0;
	Fifo *tmp = NULL;
	Fifo *curr = fifoqueue;
	size_t pathlength;
	LOCK(&fifoqueuemtx);
	while(curr != NULL){
		if(curr->fileInServer.isLocked == 0 && curr->fileInServer.isOpened == 0)
		{
			LOCK(&curr->fileInServer.filemtx);
			filesize = curr->fileInServer.size;
			if(tmp != NULL)
				tmp->next = curr->next;
			else
				fifoqueue = fifoqueue->next;
			if(fifoqueue == NULL)
				fifoqueueLast = NULL;
			pathlength=strlen(curr->fileInServer.path);
			if(writen(connectionfd, &pathlength, sizeof(size_t))==-1)
			{
				UNLOCK(&curr->fileInServer.filemtx);
				UNLOCK(&fifoqueuemtx);
				return -1;
			}
			if(writen(connectionfd, curr->fileInServer.path, pathlength) == -1)
			{
				UNLOCK(&curr->fileInServer.filemtx);
				UNLOCK(&fifoqueuemtx);
				return -1;
			}
			if(writen(connectionfd, &filesize, sizeof(size_t)) == -1)
			{
				UNLOCK(&curr->fileInServer.filemtx);
				UNLOCK(&fifoqueuemtx);
				return -1;
			}
			if(writen(connectionfd, curr->fileInServer.filecontent, filesize) == -1)
			{
				UNLOCK(&curr->fileInServer.filemtx);
				UNLOCK(&fifoqueuemtx);
				return -1;
			}
			if(icl_hash_delete(hashTableFileStorage, curr->fileInServer.path, NULL, NULL) == -1)
			{
				UNLOCK(&curr->fileInServer.filemtx);
				UNLOCK(&fifoqueuemtx);
				return -1;
			}
			free(curr);
			break;
		}
		
		tmp = curr;
		curr = curr->next;
	}
	UNLOCK(&fifoqueuemtx);
	return filesize;
}

int replaceServerFiles(int connectionfd, size_t filesize)
{
	LOCK(&serverstatsmtx);
	int serverAnswer=-1;
	if (filesize>serversettings.memorysizebytes)
	{
		UNLOCK(&serverstatsmtx);
		UNLOCK(&servermtx);
		return ERROR;
	}
	while (1)
	{
		if ((serversettings.memorysizebytes-storedBytes)<filesize)
			serverAnswer=ERROR_NOSPACELEFT;
		else
		{
			serverAnswer=SUCCESS;
			break;
		}
		MINUS_ONE_RETURN(writen(connectionfd, &serverAnswer, sizeof(int)),EFAULT);
		size_t freedBytes=replacementFIFO(connectionfd);
		if (freedBytes<=0)
		{
			serverAnswer=ERROR;
			break;
		}
		storedBytes-=freedBytes;
	}
	MINUS_ONE_RETURN(writen(connectionfd, &serverAnswer, sizeof(int)),EFAULT);
	UNLOCK(&serverstatsmtx);
	return serverAnswer;
}

int serverAppend(int connectionfd){
	int serverAnswer = 0;
	size_t answer2 = 0;
	serverFile *tmp;
	char *contenuto;
	size_t size;
	
	if ((tmp=serverOpenFile(OPENFILE,connectionfd))==NULL)
	{
		MINUS_ONE_RETURN(writen(connectionfd, &serverAnswer, sizeof(int)),EFAULT);
		return -1;
	}
	UNLOCK(&servermtx)
	LOCK(&servermtx);
	LOCK(&tmp->filemtx);
	UNLOCK(&servermtx);
	tmp->isLocked=TRUE;
	
	if(readn(connectionfd, &size, sizeof(size_t)) == -1){			//leggo la dimensione dell'append
		MINUS_ONE_RETURN(writen(connectionfd, &serverAnswer, sizeof(int)),EFAULT);
		UNLOCK(&tmp->filemtx);
		return -1;
	}
	
	if((contenuto = malloc(sizeof(char) * size)) == NULL){	//alloco la dimensione dell'append
		MINUS_ONE_RETURN(writen(connectionfd, &serverAnswer, sizeof(int)),EFAULT);
		UNLOCK(&tmp->filemtx);
		return -1;
	}
	
	memset(contenuto,'\0',size);
	
	if(readn(connectionfd, (void*)contenuto, size)== -1){				//leggo il contenuto del dell'append
		MINUS_ONE_RETURN(writen(connectionfd, &serverAnswer, sizeof(int)),EFAULT);
		free(contenuto);
		UNLOCK(&tmp->filemtx);
		return -1;
	}
	if (serversettings.memorysizebytes<size+tmp->size)
	{
		serverAnswer=ERROR;
		MINUS_ONE_RETURN(writen(connectionfd, &serverAnswer, sizeof(int)),EFAULT);
		free(contenuto);
		UNLOCK(&tmp->filemtx);
		return -1;
	}
	if ((serversettings.memorysizebytes-storedBytes)<size+(tmp->size))
	{
		serverAnswer=ERROR_NOSPACELEFT;
		writen(connectionfd, &serverAnswer, sizeof(int));
		serverAnswer=replaceServerFiles(connectionfd, size);
	}
	else
		serverAnswer=SUCCESS;
	writen(connectionfd, &serverAnswer, sizeof(int));

	//aggiungo la filesize e il contenuto del file nel server
	if(serverAnswer != -1){
		tmp->filecontent = realloc(tmp->filecontent, tmp->size+size);
		char *memtmp = (char*)tmp->filecontent;
		memcpy((void*)&memtmp[tmp->size], contenuto, size);
		tmp->size = tmp->size + (int)size;
	}
	
	//aggiorno le statistiche del server
	if(serverAnswer != -1){
		LOCK(&serverstatsmtx);
		storedBytes = storedBytes + (long)size;
		if(storedBytes > serverstats.higheststorageused)
			serverstats.higheststorageused = storedBytes;
		UNLOCK(&serverstatsmtx);
	}
	
	
	//mando risposta indietro al client
	if(writen(connectionfd, &serverAnswer, sizeof(int)) == -1){

		free(contenuto);
		UNLOCK(&tmp->filemtx);
		return -1;
	}
	UNLOCK(&tmp->filemtx);
	
	//chiudo il file una volta che ci ho scritto
	if(serverCloseFile(connectionfd, tmp->path) == -1){

		free(contenuto);
		return -1;
	}
	
	serverAnswer = (int)answer2;
	free(contenuto);
	return serverAnswer;
}
void fatalError(char* reason)
{
	char* str=malloc(strlen("Errore: ")+strlen(reason)+2);
	strcpy(str, "Errore: ");
	perror(strcat(str, reason));
	free(str);
	exit(EXIT_FAILURE);
}

int createServerSocket(void)
{
	unlink(serversettings.socketname);
	struct sockaddr_un sa;
	sa.sun_family=AF_UNIX;
	strncpy(sa.sun_path,serversettings.socketname,UNIX_PATH_MAX);
	int sk=socket(AF_UNIX,SOCK_STREAM,0);
	struct sockaddr* test = (struct sockaddr*)&sa;
	int a = bind(sk,test, sizeof(sa));
	printf("Bind result: %d\n",a);
	if (a<0)
		exit(EXIT_FAILURE);
	listen(sk, SOMAXCONN);
	return sk;
}

void readconfig(const char* configfilepath)
{
	FILE* fPtr=fopen(configfilepath, "r");
	if (fPtr==NULL)
		fatalError("File di configurazione non trovato!\n");
	char* currentsetting = malloc(sizeof(char)*50);
	char* value = malloc(sizeof(char)*50);
	while(!feof(fPtr))
		{
		fscanf(fPtr, "%[^= ]=%s\n",currentsetting,value);
		if (currentsetting==NULL || value==NULL)
			fatalError("Il file di configurazione ha uno o più valori non validi!\n");
		else {
			if (!strcmp(currentsetting, "threadnumber"))
				{
				if (isNumeric(value))
					serversettings.threadnumber=atoi(value);
				else
					{
					free(currentsetting);
					free(value);
					fatalError("Numero di thread worker non valido!\n");
					}
				}
			else if (!strcmp(currentsetting, "memorysize"))
				{
				if (isNumeric(value) && atoi(value)<500)
					serversettings.memorysizebytes=atoi(value)*1000000;
				else
					{
					free(currentsetting);
					free(value);
					fatalError("Dimensione della memoria del server non valida!\n");
					}
				}
			else if (!strcmp(currentsetting, "maxfilenumber"))
				{
				if (isNumeric(value))
					serversettings.maxfilenumber=atoi(value);
				else
					{
					free(currentsetting);
					free(value);
					fatalError("Numero massimo dei file del server non valido!\n");
					}
				}
			else if (!strcmp(currentsetting, "socketname"))
				{

					serversettings.socketname=malloc(sizeof(char)*35);
					strcpy(serversettings.socketname, readSocketFile(value));
				}
			else
				printf("Configurazione \"%s\" del server non supportata, verrà ignorata",currentsetting);
		}
		
		}
	free(currentsetting);
	free(value);
	fclose(fPtr);
	if (!serversettings.threadnumber)
		printf("Non è stato specificato il numero di thread nel file di configurazione, verrà usato 1 come failsafe.\n");
	if (!serversettings.memorysizebytes)
		printf("Non è stata specificata la dimensione della memoria del server nel file di configurazione, verranno usati 100 MB come failsafe.\n");
	if (!serversettings.maxfilenumber)
		printf("Non è stato specificato il numero massimo di file memorizzabili nel server. Verrà usato 1000 come failsafe.\n");
	if (serversettings.socketname==NULL)
		fatalError("Non è stato specificato alcun file del socket!\n");
}

int serverWriteFile(int connectionfd){
	int serverAnswer = 0;
	size_t answer2 = 0;
	serverFile *tmp = NULL;
	char *contenuto;
	size_t filesize;
	
	tmp = serverOpenFile(OPENCREATE,connectionfd);
	LOCK(&servermtx);
	LOCK(&tmp->filemtx);
	
	if(readn(connectionfd, &filesize, sizeof(size_t)) == -1)
		{
		icl_hash_delete(hashTableFileStorage, tmp->path, NULL, NULL);
		UNLOCK(&servermtx);
		return -1;
		}
	
	if((contenuto = malloc(sizeof(char) * filesize)) == NULL){
		icl_hash_delete(hashTableFileStorage, tmp->path, NULL, NULL);
		UNLOCK(&servermtx);
		return -1;
	}
	
	memset(contenuto,'\0',filesize);
	
	if(readn(connectionfd, contenuto, filesize)== -1)
		{
		icl_hash_delete(hashTableFileStorage, tmp->path, NULL, NULL);
		free(contenuto);
		UNLOCK(&servermtx);
		return -1;
		}

	if ((serversettings.memorysizebytes-storedBytes)<filesize)
	{
		serverAnswer=ERROR_NOSPACELEFT;
		writen(connectionfd, &serverAnswer, sizeof(int));
		serverAnswer=replaceServerFiles(connectionfd, filesize);
	}
	else
		serverAnswer=SUCCESS;
		writen(connectionfd, &serverAnswer, sizeof(int));
	
	//aggiungo la size e il contenuto del file nel server
	if(serverAnswer != -1 && tmp->isOpened == 1)
		{
		tmp->size = filesize;
		tmp->filecontent = contenuto;
		}
	
	//aggiorno le stats del server
	LOCK(&serverstatsmtx);
	storedFiles ++;
	storedBytes = storedBytes + (long)filesize;
	if(storedBytes > serverstats.higheststorageused)
		serverstats.higheststorageused = storedBytes;
	if(storedFiles > serverstats.highestfilenum)
		serverstats.highestfilenum = storedFiles;
	UNLOCK(&serverstatsmtx);
	
	//mando risposta al client
	UNLOCK(&tmp->filemtx);
	
	//chiudo il file una volta che ci ho scritto
	if(serverCloseFile(connectionfd,tmp->path) == -1){

		return -1;
	}
	fifo_insert(tmp);
	serverAnswer = (int)answer2;
	return serverAnswer;
}


int serverFileRead(int connectionfd){
	char* pathname;
	size_t pathsize;
	serverFile* tmp = NULL;
	int serverAnswer = 0;
	MINUS_ONE_RETURN(readn(connectionfd, &pathsize, sizeof(size_t)),"Errore durante la lettura dal client");
	CHECK_MALLOC(pathname = malloc(sizeof(char) * pathsize));
	memset(pathname,'\0',pathsize);
	MINUS_ONE_RETURN(readn(connectionfd, pathname, pathsize),"Errore durante la lettura dal client");
	
	LOCK(&servermtx);
	if((tmp = icl_hash_find(hashTableFileStorage, pathname)) == NULL)
		{
		serverAnswer = ERROR_FILENOTFOUND;
		UNLOCK(&servermtx);
		}
	else{
		if(tmp->isOpened)
			{
			serverAnswer = ERROR_FILEALREADY_OPENED;
			UNLOCK(&servermtx);
			}
		else{
			LOCK(&tmp->filemtx);
			UNLOCK(&servermtx);
		}
	}
	
	free(pathname);
	
	size_t filesize;
	if(serverAnswer != ERROR_FILENOTFOUND)
		filesize = tmp->size;
	else
		filesize = -1;
	if(writen(connectionfd, &serverAnswer, sizeof(int)) == -1){	//mando la size del file, in caso il file non esista mando -1

		if (tmp!=NULL)
			UNLOCK(&tmp->filemtx);
		return -1;
	}

	if (tmp!=NULL)
		{
		if(writen(connectionfd, &filesize, sizeof(size_t)) == -1){	//mando la size del file, in caso il file non esista mando -1

			UNLOCK(&tmp->filemtx);
			return -1;
		}

		if(serverAnswer == -1){
			return -1;
		}

		if(writen(connectionfd, tmp->filecontent, tmp->size) == -1){	//mando il file

			UNLOCK(&tmp->filemtx);
			return -1;
		}

		UNLOCK(&tmp->filemtx);
		}
	return serverAnswer;
	
}
int serverLockFile(int connectionfd){
	char* pathname;
	size_t pathsize;
	serverFile *tmp;
	int serverAnswer = 0;
	MINUS_ONE_RETURN(readn(connectionfd, &pathsize, sizeof(size_t)),"Errore durante la lettura dal client");
	CHECK_MALLOC(pathname = malloc(sizeof(char) * pathsize));
	memset(pathname,'\0',pathsize);
	MINUS_ONE_RETURN(readn(connectionfd, pathname, pathsize),"Errore durante la lettura dal client");
	LOCK(&servermtx);
	if((tmp = icl_hash_find(hashTableFileStorage, pathname)) == NULL)
		{
		serverAnswer = -1;
		UNLOCK(&servermtx);
		}
	else{
		LOCK(&tmp->filemtx);
		UNLOCK(&servermtx);
		if (tmp->user!=connectionfd)
			while (tmp->isLocked)
				WAIT(&olockcond, &olockmutex);
		if(tmp->isLocked == FALSE && tmp->isOpened == FALSE)
			{
			tmp->isLocked = TRUE;
			tmp->user = connectionfd;
			serverAnswer=SUCCESS_LOCK;
			}
		else
			serverAnswer = -1;
		UNLOCK(&tmp->filemtx);
	}
	if(writen(connectionfd, &serverAnswer, sizeof(int)) == -1)
		{
		free(pathname);
		return -1;
		}
	free(pathname);
	return serverAnswer;
}
int serverUnlockFile(int connectionfd)
{
	char* pathname;
	size_t dim;
	serverFile *tmp;
	int serverAnswer = 0;
	MINUS_ONE_RETURN(readn(connectionfd, &dim, sizeof(size_t)),"Errore durante la lettura dal client");
	CHECK_MALLOC(pathname = malloc(sizeof(char) * dim));
	memset(pathname,'\0',dim);
	SIGNAL(&condbuf);
	MINUS_ONE_RETURN(readn(connectionfd, pathname, dim),"Errore durante la lettura dal client");
	LOCK(&servermtx);
	if((tmp = icl_hash_find(hashTableFileStorage, pathname)) == NULL)
		{
		serverAnswer = ERROR_FILENOTFOUND;
		UNLOCK(&servermtx);
		}
	else
		{
		LOCK(&tmp->filemtx);

		UNLOCK(&servermtx);
		if(tmp->isLocked == TRUE && tmp->isOpened == FALSE){
			tmp->isLocked = FALSE;
			tmp->user = connectionfd;
		}
		else if(tmp->isLocked == 0){
			if(tmp->user == connectionfd)
				serverAnswer = 1;
		}

		else
			serverAnswer = -1;
		UNLOCK(&tmp->filemtx);
		}
	if(writen(connectionfd, &serverAnswer, sizeof(int)) == -1){
		free(pathname);
		return -1;
	}
	if(serverAnswer == -1)
		return serverAnswer;
	free(pathname);
	return serverAnswer;
}
int serverRemoveFile(int connectionfd)
{
	char* pathname;
	size_t dim;
	serverFile *tmp = NULL;
	MINUS_ONE_RETURN(readn(connectionfd, &dim, sizeof(size_t)),"Errore durante la lettura dal client");
	CHECK_MALLOC(pathname = malloc(sizeof(char) * dim));	//alloco spazio per il pathname
	memset(pathname,'\0',dim);
	MINUS_ONE_RETURN(readn(connectionfd, pathname, dim),"Errore durante la lettura dal client");
	int serverAnswer=-1;
	if ((tmp=icl_hash_find(hashTableFileStorage, pathname))==NULL) {
		MINUS_ONE_RETURN(writen(connectionfd, &serverAnswer, sizeof(serverAnswer)),"Errore durante la scrittura verso il client.");
		return serverAnswer;
	}
		serverAnswer=ERROR_FILENOTFOUND;
	if (tmp->isOpened)
		serverAnswer=ERROR_FILEALREADY_OPENED;
	if (tmp->isLocked)
		serverAnswer=ERROR_FILEISLOCKED;
	if (icl_hash_delete(hashTableFileStorage, tmp->path, NULL, NULL)==0)
		 serverAnswer=SUCCESS;
	MINUS_ONE_RETURN(writen(connectionfd, &serverAnswer, sizeof(serverAnswer)),"Errore durante la scrittura verso il client.");
		 return serverAnswer;
}

int updatemax(fd_set set, int fdmax) {
	for(int i=(fdmax-1);i>=0;--i)
		if (FD_ISSET(i, &set)) return i;

	return -1;
}

void* workerThread(void* args)
{
	while (!quit)
		{
		int currentfd;
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
				UNLOCK(&requestcountermtx);
				UNLOCK(&bufmtx);
				SIGNAL(&condbuf);
				return NULL;
				}
			LOCK(&requestcountermtx);
			}
		requestcounter--;
		UNLOCK(&requestcountermtx);
		if ((currentfd=removeNode(&clientrequests, &lastqueueelem))==-1)
			{UNLOCK(&bufmtx); exit(EXIT_FAILURE);}
		UNLOCK(&bufmtx);
		SIGNAL(&condbuf);
		if(currentfd == -2){
			return NULL;
		}
		int clientcoderequest = -1;
		//printf("READN\n");
		if(readn(currentfd, &clientcoderequest, sizeof(int)) == -1) 
			clientcoderequest = -1;

		switch((int)clientcoderequest){
			case OPENFILE:
			case OPENCREATE:
			case OPENLOCK:
			case OPENLOCKCREATE:
				serverOpenFile(clientcoderequest,currentfd);
				break;
			case READ:
				serverFileRead(currentfd);
				break;

			case READN:
				serverReadNFiles(currentfd);
				break;

			case APPEND:
				serverAppend(currentfd);
				break;

			case LOCKFILE:
				serverLockFile(currentfd);
				break;

			case UNLOCKFILE:
				serverUnlockFile(currentfd);
				break;

			case CLOSEFILE:
				serverCloseFile(currentfd, NULL);
				break;

			case REMOVE:
				serverRemoveFile(currentfd);
				break;

			case WRITE:
				serverWriteFile(currentfd);
				break;

			case CLOSECONN:
				close(currentfd);
				return NULL;

			default:
				break;

		}
		if((int)clientcoderequest != CLOSECONN){

			if(write(pipefd[1], &currentfd, sizeof(int)) == -1)
				close(currentfd);
			}

		}

	return NULL;
}




int main(int argc, const char * argv[]) {
	if (argc!=2)
		fatalError("Argomenti non validi! Passare come argomento solo il percorso del file di configurazione!\n");
	struct sigaction s;
	s.sa_handler = sigterm;
	sigemptyset(&s.sa_mask);
	s.sa_flags = SA_RESTART;
	sigaction(SIGINT, &s, NULL);
	sigaction(SIGQUIT, &s, NULL);
	sigaction(SIGHUP, &s, NULL);
	sigaction(SIGPIPE, &s, NULL);
	signal(SIGINT, sigterm);
	signal(SIGQUIT, sigterm);
	signal(SIGHUP, sigterm);
	readconfig(argv[1]);
	hashTableFileStorage = icl_hash_create(100, NULL, NULL);
	filesOpened = icl_hash_create(25, NULL, NULL);
	int runningthreads=0;
	pthread_t* threadids;
	NULL_EXIT_ERROR(threadids=malloc(serversettings.threadnumber*sizeof(pthread_t)), "Impossibile creare la struttura per i thread!");
	for(int i=0; i<serversettings.threadnumber; i++)
		threadids[i] = 0;
	int listenerfd = createServerSocket();
	pipe(pipefd);
	fd_set tempset;
	FD_ZERO(&set);
	FD_ZERO(&tempset);
	FD_SET(pipefd[0],&set);
	FD_SET(listenerfd,&set);
	int maxfd = listenerfd;
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
		for (int i=0;i<maxfd+1;i++)
			{
			if (FD_ISSET(i,&tempset))
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
				else if (i==pipefd[0])
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
					connectionfd=i;
					int try=0;

					FD_CLR(connectionfd,&set);
					if (connectionfd==maxfd)
						maxfd=updatemax(set,maxfd);
					LOCK(&bufmtx);
					int result;
					while ((result = insertNode(connectionfd,&lastqueueelem,&clientrequests)) < 0 && try < 3)
						try++;
					if(result < 0){
						UNLOCK(&bufmtx);
						close(connectionfd);
					}
					else
						{
						UNLOCK(&bufmtx);
						LOCK(&requestcountermtx);
						requestcounter++;
						UNLOCK(&requestcountermtx);
						SIGNAL(&condbuf);
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
	for (int i=0;i<runningthreads;i++)
		{
		NOT_ZERO_ERROR(pthread_join(threadids[i], NULL), "Non riesco a joinare un thread.");
		}
	free(threadids);
	Fifo *tempqueue;
	while(fifoqueue!=NULL)
		{
		tempqueue=fifoqueue;
		fifoqueue=fifoqueue->next;
		free(tempqueue);
		}
	icl_hash_destroy(hashTableFileStorage, NULL, NULL);
	printf("il numero massimo di file memorizzati nel server è stato %lu \n",serverstats.highestfilenum);
	printf("il numero massimo di spazio utilizzato nel server è stato %lu \n",serverstats.higheststorageused);
	printf("l'algoritmo di rimpiazzamento è stato usato %d volte\n",serverstats.filesreplacednum);
	printf("Libero le ultime cose ed esco\n");
	free(serversettings.socketname);
	return 0;
}
