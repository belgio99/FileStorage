//
//  api.c
//  Client
//
//  Created on 14/06/21.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/un.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/un.h>
#include <ctype.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <string.h>


#include "../header/utils.h"
#include "../header/api.h"

#define MINUS_ONE_RETURN(X,err)													\
if ((X)==-1) {																			\
errno = err;																			\
return -1;																				\
}
#define MINUS_ONE_RETURN_WITHFREE(X,err,elemtofree)						\
if ((X)==-1) {																			\
errno = err;																			\
free(elemtofree);																		\
return -1;																				\
}
#define MINUS_ONE_RETURN_WITHMSG(X,err,msg)									\
if ((X)==-1) {																			\
errno = err;																			\
 msg; 																				\
return -1;																				\
}
#define MINUS_ONE_RETURN_WITHFREE_WITHMSG(X,err,elemtofree,msg)		\
if ((X)==-1) {																			\
errno = err;																			\
free(elemtofree);																		\
 msg; 																				\
return -1;																				\
}


static _Bool connected = FALSE;
static struct sockaddr_un sa;
static int clientFD;

char* readLocalFile(const char* pathname, size_t* filesize)
{
	int fd;
	if ((fd=open(pathname,O_RDONLY))==-1)
		return NULL;
	struct stat fdbuffer;
	fstat(fd,&fdbuffer);
	*filesize=fdbuffer.st_size;
	char* file;
	if((file = malloc(fdbuffer.st_size+1))==NULL || readn(fd, file, fdbuffer.st_size+1)==-1)
		return NULL;
	close(fd);
	return file;
}
int saveLocalFile(const char* file, const char* pathname, size_t filesize)
{
	if (pathname==NULL)
		return 0;
	int fd;
	if((fd = open(pathname, O_RDWR|O_CREAT , 0666)) == -1 || writen(fd, (void*)file, filesize) == -1)
		return -1;
	close(fd);
	return 0;
}
int fileReplacement(int clientFD, const char* dirname)
{
	int serverAnswer;
	size_t pathlength;
	char* pathname=malloc(sizeof(char));
	size_t filesize;
	char* filecontents=malloc(sizeof(char));
	char* savepathname=malloc(sizeof(char));
	while (1)
	{
		MINUS_ONE_RETURN(read(clientFD, &serverAnswer, sizeof(int)),EFAULT);
		if (serverAnswer==SUCCESS || serverAnswer==ERROR)
			break;
		MINUS_ONE_RETURN(read(clientFD, &pathlength, sizeof(pathlength)),EFAULT);
		MINUS_ONE_RETURN(read(clientFD, pathname, pathlength),EFAULT);
		pathname=realloc(pathname,sizeof(char)*pathlength);
		MINUS_ONE_RETURN(read(clientFD, &filesize, sizeof(filesize)),EFAULT);
		MINUS_ONE_RETURN(read(clientFD, filecontents, filesize),EFAULT);
		savepathname=realloc(savepathname, sizeof(char)*(pathlength+100+3));
		if (dirname!=NULL)
		{
		strcpy(savepathname,dirname);
		savepathname=strcat(savepathname, "/");
		savepathname=strncat(savepathname, pathname,pathlength);
		 printf("Salvo il file in \"%s\".\n",dirname);
		if (saveLocalFile(filecontents, savepathname, filesize)==-1)
			 printf("Salvataggio fallito.\n");
		}
	}
	free(pathname);
	free(filecontents);
	free(savepathname);
	return serverAnswer;
}

int parseServerAnswer(int serverAnswer, const char* pathname)
{
	switch (serverAnswer){
		case SUCCESS:
			 printf("L'operazione sul file \"%s\" è stata completata.\n",pathname);
			return 0;
		case SUCCESS_OPEN:
			 printf("Il file \"%s\" è stato aperto.\n",pathname);
			return 0;
			break;
		case SUCCESS_CREATE:
			 printf("Il file \"%s\" è stato creato.\n",pathname);
			return 0;
			break;
		case SUCCESS_LOCK:
			 printf("Il file \"%s\" è stato lockato con successo.\n",pathname);
			return 0;
			break;
		case SUCCESS_OPENLOCK:
			 printf("Il file \"%s\" è stato aperto e lockato con successo.\n",pathname);
			return 0;
			break;
		case SUCCESS_OPENCREATE:
			 printf("Il file \"%s\" è stato creato ed aperto con successo.\n",pathname);
			return 0;
			break;
		case SUCCESS_OPENLOCKCREATE:
			 printf("Il file \"%s\" è stato creato e lockato con successo.\n",pathname);
			return 0;
			break;
		case ERROR_FILENOTFOUND:
			 printf("Il file \"%s\" non è stato trovato nel server.\n",pathname);
			return -1;
			break;
		case ERROR_FILEISLOCKED:
			 printf("Non posso accedere al file \"%s\" perché risulta lockato.\n",pathname);
			return -1;
			break;
		case ERROR_FILEALREADY_EXISTS:
			 printf("Il file \"%s\" è già esistente nel server.\n",pathname);
			return -1;
			break;
		case ERROR_FILEALREADY_OPENED:
			 printf("Il file \"%s\" è aperto.\n",pathname);
			return 0;
			break;
		case ERROR_NOSPACELEFT:
			 printf("Il server è pieno.\n");
			return -1;
			break;
		default:
			 printf("Valore di ritorno del server non riconosciuto.\n");
			return -1;
			break;
	}
}

int openConnection(const char* sockname, int msec, const struct timespec abstime)
{
	strncpy(sa.sun_path, sockname,UNIX_PATH_MAX);
	sa.sun_family=AF_UNIX;
	clientFD=socket(AF_UNIX,SOCK_STREAM,0);
	struct sockaddr* test = (struct sockaddr*)&sa;
	int connstatus=-1;
	while ((connstatus = connect(clientFD,test, sizeof(sa))) == -1 && abstime.tv_sec>time(NULL))
		usleep(msec*1000);
	errno = (connstatus==-1) ? ETIMEDOUT : EISCONN;
	connected = (connstatus==-1) ? FALSE : TRUE;
	return connstatus;
}
int closeConnection(const char* sockname)
{
	int request = CLOSECONN;
	if (sockname==NULL)
	{
		errno=EINVAL;
		return -1;
	}
	if (strcmp(sa.sun_path, sockname)==0) {
		if (writen(clientFD, &request, sizeof(int)) == -1)
			return -1;
		if(readn(clientFD, &request, sizeof(int)) == -1)
			return -1;
		return close(clientFD);
	}
	else
	{
		errno = EFAULT;
		return -1;
	}
}
int openFile(const char* pathname, int flags)
{
	if (!connected)
	{
		errno = ENOTCONN;
		return -1;
	}
	if (pathname==NULL)
	{
		errno = EINVAL;
		return -1;
	}
	int request;
	switch (flags)
	{
		case O_CREATE:
			request = OPENCREATE;
			 printf("Provo ad aprire \"%s\" in modalità di creazione nel server.\n",pathname);
			break;
		case O_LOCK:
			request = OPENLOCK;
			 printf("Provo ad aprire \"%s\" in modalità locked nel server.\n",pathname);
			break;
		case O_LOCK_CREATE:
			request = OPENLOCKCREATE;
			 printf("Provo ad aprire \"%s\" in modalità locked e di creazione nel server.\n",pathname);
			break;
		default:
			 printf("I flags non sono stati riconosciuti. Il file \"%s\" verrà aperto in modalità read-only nel server.\n",pathname);
		case 0:
			request = OPENFILE;
			 printf("Provo ad aprire \"%s\" in modalità read-only nel server.\n",pathname);
			break;
	}
	int serverAnswer;
	MINUS_ONE_RETURN(writen(clientFD, &request, sizeof(request)),EFAULT);
	size_t pathnamelenght = strlen(pathname)+1;
	MINUS_ONE_RETURN(writen(clientFD, &pathnamelenght, sizeof(size_t)),EFAULT);
	MINUS_ONE_RETURN(writen(clientFD, (void * )pathname, pathnamelenght), EFAULT);
	MINUS_ONE_RETURN(readn(clientFD, &serverAnswer, sizeof(serverAnswer)),EFAULT);
	int returnvalue = parseServerAnswer(serverAnswer,pathname);
	errno=0;
	return returnvalue;
}

int writeFile(const char* pathname, const char* dirname)
{
	size_t filesize;
	char* filetosend;
	int request = WRITE;
	if ((filetosend=readLocalFile(pathname, &filesize))==NULL)
	{
		 printf("Il file \"%s\" non è stato trovato.\n",pathname);
		errno = EINVAL;
		return -1;
	}
	MINUS_ONE_RETURN(writen(clientFD, &request, sizeof(request)),EFAULT);
	
	if (openFile(pathname, O_CREATE) == -1)
	{
		 printf("Non posso creare il file\"%s\" nel server.\n",pathname);
		errno=EFAULT;
		free(filetosend);
		return -1;
	}
	MINUS_ONE_RETURN_WITHFREE(writen(clientFD, &filesize, sizeof(filesize)),EFAULT,filetosend);
	MINUS_ONE_RETURN_WITHFREE(writen(clientFD, filetosend, filesize),EFAULT,filetosend);
	 printf("File inviato al server!\n");
	 printf("Attendo la risposta dal server per eventuali file da ricevere...\n");
	free(filetosend);
	int serverAnswer;
	MINUS_ONE_RETURN(readn(clientFD, &serverAnswer, sizeof(int)),EQFULL);
	if (serverAnswer==ERROR_NOSPACELEFT)
		fileReplacement(clientFD, dirname);
	if(serverAnswer == ERROR_FAILTOSAVE || serverAnswer==ERROR){
		 printf("Scrittura del file \"%s\" fallita!\n",pathname);
		errno = EFAULT;
		return -1;
	}
		 printf("Scrittura del file \"%s\" riuscita. Sono stati scritti %zu bytes\n",pathname,filesize);

	return 0;
}
int readFile(const char* pathname, void** buf, size_t* size)
{
	int request=READ;
	 printf("Provo a leggere il file \"%s\".\n",pathname);
	MINUS_ONE_RETURN(writen(clientFD, &request, sizeof(request)),EFAULT);
	size_t pathlength = strlen(pathname)+1;
	MINUS_ONE_RETURN(writen(clientFD, &pathlength, sizeof(size_t)),EFAULT);
	MINUS_ONE_RETURN(writen(clientFD, (void * )pathname, pathlength),EFAULT);
	int serverAnswer = -1;
	MINUS_ONE_RETURN_WITHMSG(readn(clientFD, &serverAnswer, sizeof(serverAnswer)),EFAULT, printf("Lettura del file \"%s\" fallita.\n",pathname));
	MINUS_ONE_RETURN(parseServerAnswer(serverAnswer,pathname),EFAULT);
	*buf = malloc(serverAnswer);
	if(!*buf)
	{
		errno = ENOMEM;
		return -1;
	}
	size_t fileread;
	MINUS_ONE_RETURN(fileread = readn(clientFD, *buf, (int)serverAnswer),EFAULT);
	*size = serverAnswer;
	 printf("Lettura del file \"%s\" riuscita.\n",pathname);
	return 0;
}


int removeFile(const char* pathname)
{
	int request = REMOVE;
	 printf("Provo a rimuovere il file \"%s\".\n",pathname);
	MINUS_ONE_RETURN(writen(clientFD, &request, sizeof(request)),EFAULT);
	size_t pathlength = strlen(pathname)+1;
	MINUS_ONE_RETURN(writen(clientFD, &pathlength, sizeof(size_t)),EFAULT);
	MINUS_ONE_RETURN(writen(clientFD, (void * )pathname, pathlength),EFAULT);
	int serverAnswer = -1;
	MINUS_ONE_RETURN_WITHMSG(readn(clientFD, &serverAnswer, sizeof(serverAnswer)),EFAULT, printf("Rimozione del file \"%s\" fallita.\n",pathname));
	MINUS_ONE_RETURN(parseServerAnswer(serverAnswer,pathname),EFAULT);
	 printf("Rimozione del file \"%s\" riuscita.\n",pathname);
	return 0;
}


int lockFile(const char* pathname)
{
	int request = LOCKFILE;
	 printf("Provo a lockare il file \"%s\".\n",pathname);
	MINUS_ONE_RETURN(writen(clientFD, &request, sizeof(request)),EFAULT);
	size_t pathlength = strlen(pathname)+1;
	MINUS_ONE_RETURN(writen(clientFD, &pathlength, sizeof(size_t)),EFAULT);
	MINUS_ONE_RETURN(writen(clientFD, (void * )pathname, pathlength),EFAULT);
	int serverAnswer = 1;
	MINUS_ONE_RETURN_WITHMSG(readn(clientFD, &serverAnswer, sizeof(serverAnswer)),EFAULT, printf("Lock del file \"%s\" fallita.\n",pathname));
	MINUS_ONE_RETURN(parseServerAnswer(serverAnswer,pathname),EFAULT);
	 printf("Lock del file \"%s\" riuscita.\n",pathname);
	return 0;
}
int unlockFile(const char* pathname){
	int request = UNLOCKFILE;
	 printf("Provo a unlockare il file \"%s\".\n",pathname);
	MINUS_ONE_RETURN(writen(clientFD, &request, sizeof(request)),EFAULT);
	size_t pathlength = strlen(pathname)+1;
	MINUS_ONE_RETURN(writen(clientFD, &pathlength, sizeof(size_t)),EFAULT);
	MINUS_ONE_RETURN(writen(clientFD, (void * )pathname, pathlength),EFAULT);
	int serverAnswer = -1;
	MINUS_ONE_RETURN_WITHMSG(readn(clientFD, &serverAnswer, sizeof(serverAnswer)),EFAULT, printf("Unock del file \"%s\" fallita.\n",pathname));
	MINUS_ONE_RETURN(parseServerAnswer(serverAnswer,pathname),EFAULT);
	return 0;
}


int readNFiles(int N, const char* dirname){
	int request = READN;
	MINUS_ONE_RETURN(writen(clientFD, &request, sizeof(request)),EFAULT);
	MINUS_ONE_RETURN(writen(clientFD, &N, sizeof(N)),EFAULT);
	int bytesread = 0;
	char* pathname = NULL;
	char* receivedFile = NULL;
	char* savepathname=malloc(sizeof(char)*1000);
	while(1)
	{
		size_t pathsize;
		size_t filesize;
		MINUS_ONE_RETURN_WITHFREE(readn(clientFD, &pathsize, sizeof(pathsize)),EFAULT,savepathname);
		if (pathsize==0)
			break;
		pathname=malloc(sizeof(char)*pathsize);
		memset(pathname, '\0', pathsize);
		MINUS_ONE_RETURN_WITHFREE(readn(clientFD, pathname, pathsize),EFAULT,savepathname);
		MINUS_ONE_RETURN_WITHFREE(readn(clientFD, &filesize, sizeof(size_t)),EFAULT,savepathname);
		receivedFile=malloc(filesize*sizeof(char));
		MINUS_ONE_RETURN_WITHFREE(readn(clientFD, receivedFile, filesize),EFAULT,savepathname);

		 printf("Ho ricevuto un file dal server.\n");
		if (dirname!=NULL)
		{
			strcpy(savepathname,dirname);
			savepathname=strcat(savepathname, "/");
			savepathname=strncat(savepathname, pathname,pathsize);
			 printf("Salvo il file in \"%s\".\n",dirname);
			if (saveLocalFile(receivedFile, savepathname, filesize)==-1)
				 printf("Salvataggio fallito.\n");
		}
		bytesread+=filesize;
		filesize=0;
		memset(receivedFile, '\0', filesize);
		memset(savepathname, '\0', strlen(savepathname));
	}
	free(pathname);
	free(receivedFile);
	free(savepathname);
	return bytesread;
	
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname){
	int request = APPEND;
	 printf("Provo ad appendere dati al file \"%s\".\n",pathname);
	int reqopen = OPENFILE;
	MINUS_ONE_RETURN(writen(clientFD, &request, sizeof(request)),EFAULT);
	MINUS_ONE_RETURN(writen(clientFD, &reqopen, sizeof(request)),EFAULT);
	size_t pathlength = strlen(pathname)+1;
	MINUS_ONE_RETURN(writen(clientFD, &pathlength, sizeof(size_t)),EFAULT);
	MINUS_ONE_RETURN(writen(clientFD, (void * )pathname, pathlength),EFAULT);
	int serverAnswer = 0;
	MINUS_ONE_RETURN_WITHMSG(readn(clientFD, &serverAnswer, sizeof(serverAnswer)),EFAULT, printf("Non riesco ad aprire il file \"%s\".\n",pathname));
	MINUS_ONE_RETURN(writen(clientFD, &size, sizeof(size_t)),EFAULT);
	MINUS_ONE_RETURN(writen(clientFD, buf, size),EFAULT);
	MINUS_ONE_RETURN(readn(clientFD, &serverAnswer, sizeof(int)),EFAULT);
	if (serverAnswer==ERROR_NOSPACELEFT)
		fileReplacement(clientFD, dirname);
	if(serverAnswer == ERROR_FAILTOSAVE || serverAnswer==ERROR){
		 printf("L'append del file \"%s\" è fallito!\n",pathname);
		errno = EFAULT;
		return -1;

		}
	printf("L'append del file \"%s\" è riuscito.\n",pathname);
	return 0;
}
