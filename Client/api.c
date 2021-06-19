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

#include "../utils.h"

#include "api.h"

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
VBON msg; 																				\
return -1;																				\
}
#define MINUS_ONE_RETURN_WITHFREE_WITHMSG(X,err,elemtofree,msg)		\
if ((X)==-1) {																			\
errno = err;																			\
free(elemtofree);																		\
VBON msg; 																				\
return -1;																				\
}

#define VBON if(VERBOSEMODE)

static _Bool connected = FALSE;
static struct sockaddr_un sa;
static int clientFD;


int openConnection(const char* sockname, int msec, const struct timespec abstime)
{
	strncpy(sa.sun_path, sockname,UNIX_PATH_MAX);
	sa.sun_family=AF_UNIX;
	int sk=socket(AF_UNIX,SOCK_STREAM,0);
	struct sockaddr* test = (struct sockaddr*)&sa;
	int connstatus;
	while ((connstatus = connect(sk,test, sizeof(sa)) == -1 && abstime.tv_sec>time(NULL)))
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
			VBON printf("Provo ad aprire \"%s\" in modalità di creazione.\n",pathname);
			break;
		case O_LOCK:
			request = OPENLOCK;
			VBON printf("Provo ad aprire \"%s\" in modalità locked.\n",pathname);
			break;
		case O_LOCK_CREATE:
			request = OPENLOCKCREATE;
			VBON printf("Provo ad aprire \"%s\" in modalità locked e di creazione.\n",pathname);
			break;
		default:
			VBON printf("I flags non sono stati riconosciuti. Il file \"%s\" verrà aperto in modalità read-only.\n",pathname);
		case 0:
			request = OPENFILE;
			VBON printf("Provo ad aprire \"%s\" in modalità read-only.\n",pathname);
			break;
	}
	int serveranswer;
	MINUS_ONE_RETURN(writen(clientFD, &request, sizeof(request)),EFAULT);
	size_t pathnamelenght = strlen(pathname)+1;
	MINUS_ONE_RETURN(writen(clientFD, &pathnamelenght, sizeof(size_t)),EFAULT);
	MINUS_ONE_RETURN(writen(clientFD, (void * )pathname, pathnamelenght), EFAULT);
	MINUS_ONE_RETURN(readn(clientFD, &serveranswer, sizeof(serveranswer)),EFAULT);
	switch (serveranswer){
		case SUCCESS_OPEN:
			VBON printf("Il file \"%s\" è stato aperto.\n",pathname);
			break;
		case SUCCESS_CREATE:
			VBON printf("Il file \"%s\" è stato creato.\n",pathname);
			break;
		case SUCCESS_LOCK:
			VBON printf("Il file \"%s\" è stato lockato con successo.\n",pathname);
			break;
		case SUCCESS_CREATELOCK:
			VBON printf("Il file \"%s\" è stato creato e lockato con successo.\n",pathname);
			break;
		case ERROR_FILENOTFOUND:
			VBON printf("Il file \"%s\" non è stato trovato nel server.\n",pathname);
			return -1;
			break;
		case ERROR_FILEISLOCKED:
			VBON printf("Non posso accedere al file \"%s\" perché risulta lockato.\n",pathname);
			return -1;
			break;
		case ERROR_FILEALREADY_EXISTS:
			VBON printf("Il file \"%s\" è già esistente nel server.\n",pathname);
			return -1;
		case ERROR_FILEALREADY_OPENED:
			VBON printf("Il file \"%s\" è aperto.\n",pathname);
			return -1;
		case ERROR_NOSPACELEFT:
			VBON printf("Il server è pieno.\n");
			return -1;
		default: break;
	}
	errno=0;
	return 0;
}
int writeFile(const char* pathname, const char* dirname)
{
	int openreturnvalue;
	size_t filesize;
	char* filetosend;
	int request = WRITE;
	if ((filetosend=readLocalFile(pathname, &filesize))==NULL)
	{
		VBON printf("Il file \"%s\" non è stato trovato.\n",pathname);
		errno = EINVAL;
		return -1;
	}
	MINUS_ONE_RETURN(writen(clientFD, &request, sizeof(request)),EFAULT);
	
	if ((openreturnvalue=openFile(pathname, O_LOCK_CREATE) == -1))
	{
		VBON printf("Non posso creare e lockare il file\"%s\" nel server.\n",pathname);
		errno=EFAULT;
		free(filetosend);
		return -1;
	}
	MINUS_ONE_RETURN_WITHFREE(writen(clientFD, &filesize, sizeof(filesize)),EFAULT,filetosend);
	MINUS_ONE_RETURN_WITHFREE(writen(clientFD, filetosend, filesize),EFAULT,filetosend);
	VBON printf("File inviato al server!\n");
	VBON printf("Attendo la risposta dal server per eventuali file da ricevere...\n");
	free(filetosend);
	int serveranswer;
	MINUS_ONE_RETURN(readn(clientFD, &serveranswer, sizeof(size_t)),EQFULL);
	if (serveranswer==ERROR_NOSPACELEFT) {
		size_t bytestoreceive;
		MINUS_ONE_RETURN((readn(clientFD, &bytestoreceive, sizeof(bytestoreceive))),EFAULT);
		while (bytestoreceive>0) {
			char* receivedFile = malloc(bytestoreceive);
			if (receivedFile==NULL)
				return -1;
			if (saveLocalFile(receivedFile, dirname, bytestoreceive)==-1)
				VBON printf("Impossibile salvare il file \"%s\" nella cartella \"%s\".\n",pathname,dirname);
			free(receivedFile);
		}
	}
	if(serveranswer == ERROR_FAILTOSAVE){
		VBON printf("Scrittura del file \"%s\" fallita!\n",pathname);
		errno = EFAULT;
		return -1;
	}
	VBON printf("Scrittura del file \"%s\" riuscita. Sono stati scritti %zu bytes\n",pathname,filesize);
	return 0;
}
int readFile(const char* pathname, void** buf, size_t* size)
{
	int request=READ;
	VBON printf("Provo a leggere il file \"%s\".\n",pathname);
	MINUS_ONE_RETURN(writen(clientFD, &request, sizeof(request)),EFAULT);
	size_t pathlength = strlen(pathname)+1;
	MINUS_ONE_RETURN(writen(clientFD, &pathlength, sizeof(size_t)),EFAULT);
	MINUS_ONE_RETURN(writen(clientFD, (void * )pathname, pathlength),EFAULT);
	size_t serveranswer = -1;
	MINUS_ONE_RETURN_WITHMSG(readn(clientFD, &serveranswer, sizeof(serveranswer)),EFAULT, printf("Lettura del file \"%s\" fallita.\n",pathname)); //da sostituire con read success
	MINUS_ONE_RETURN(serveranswer,EFAULT);
	*buf = malloc(serveranswer);
	if(!*buf){ errno = ENOMEM; return -1; }
	memset(*buf,'\0',serveranswer);
	int fileread;
	MINUS_ONE_RETURN(fileread = readn(clientFD, *buf, (int)serveranswer),EFAULT);
	*size = serveranswer;
	VBON printf("Lettura del file \"%s\" riuscita.\n",pathname);
	return 0;
}


int removeFile(const char* pathname){
	int request = REMOVE;
	VBON printf("Provo a rimuovere il file \"%s\".\n",pathname);
	MINUS_ONE_RETURN(writen(clientFD, &request, sizeof(request)),EFAULT);
	size_t pathlength = strlen(pathname)+1;
	MINUS_ONE_RETURN(writen(clientFD, &pathlength, sizeof(size_t)),EFAULT);
	MINUS_ONE_RETURN(writen(clientFD, (void * )pathname, pathlength),EFAULT);
	int serveranswer = 1;
	MINUS_ONE_RETURN_WITHMSG(readn(clientFD, &serveranswer, sizeof(serveranswer)),EFAULT, printf("Rimozione del file \"%s\" fallita.\n",pathname)); //da sostituire con read success
	MINUS_ONE_RETURN(serveranswer,EFAULT);
	VBON printf("Rimozione del file \"%s\" riuscita.\n",pathname);
	return 0;
}


int lockFile(const char* pathname){
	int request = LOCK;
	VBON printf("Provo a lockare il file \"%s\".\n",pathname);
	MINUS_ONE_RETURN(writen(clientFD, &request, sizeof(request)),EFAULT);
	size_t pathlength = strlen(pathname)+1;
	MINUS_ONE_RETURN(writen(clientFD, &pathlength, sizeof(size_t)),EFAULT);
	MINUS_ONE_RETURN(writen(clientFD, (void * )pathname, pathlength),EFAULT);
	int serveranswer = 1;
	MINUS_ONE_RETURN_WITHMSG(readn(clientFD, &serveranswer, sizeof(serveranswer)),EFAULT, printf("Lock del file \"%s\" fallita.\n",pathname)); //da sostituire con read success
	MINUS_ONE_RETURN(serveranswer,EFAULT);
	VBON printf("Lock del file \"%s\" riuscita.\n",pathname);
	return 0;
}


int readNFiles(int N, const char* dirname){
	int request = READN;
	MINUS_ONE_RETURN(writen(clientFD, &request, sizeof(request)),EFAULT);
	int bytesread = 0;
	int bytestoreceive=0;
	char* receivedFile=malloc(sizeof(receivedFile));
	while(1)
	{
		MINUS_ONE_RETURN(readn(clientFD, &bytestoreceive, sizeof(bytestoreceive)),EFAULT);
		if (bytestoreceive==0)
			break;
		MINUS_ONE_RETURN(readn(clientFD, &receivedFile, sizeof(receivedFile)),EFAULT);
		saveLocalFile(receivedFile, dirname, bytestoreceive);
		bytesread+=bytestoreceive;
	}
	return bytesread;
	
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname){
	int request = APPEND;
	VBON printf("Provo ad appendere dati al file \"%s\".\n",pathname);
	MINUS_ONE_RETURN(writen(clientFD, &request, sizeof(request)),EFAULT);
	size_t pathlength = strlen(pathname)+1;
	MINUS_ONE_RETURN(writen(clientFD, &pathlength, sizeof(size_t)),EFAULT);
	MINUS_ONE_RETURN(writen(clientFD, (void * )pathname, pathlength),EFAULT);
	int serveranswer = 0;
	MINUS_ONE_RETURN_WITHMSG(readn(clientFD, &serveranswer, sizeof(serveranswer)),EFAULT, printf("Non riesco ad aprire il file \"%s\".\n",pathname)); //da sostituire con read success
	MINUS_ONE_RETURN(writen(clientFD, &size, sizeof(size_t)),EFAULT);
	MINUS_ONE_RETURN(writen(clientFD, buf, size),EFAULT);
	serveranswer=-1;
	MINUS_ONE_RETURN(readn(clientFD, &serveranswer, sizeof(int)),EQFULL);
	if (serveranswer==ERROR_NOSPACELEFT) {
		size_t bytestoreceive;
		MINUS_ONE_RETURN((readn(clientFD, &bytestoreceive, sizeof(bytestoreceive))),EFAULT);
		while (bytestoreceive>0) {
			char* receivedFile = malloc(bytestoreceive);
			if (receivedFile==NULL)
				return -1;
			if (saveLocalFile(receivedFile, dirname, bytestoreceive)==-1)
				VBON printf("Impossibile salvare il file \"%s\" nella cartella \"%s\".\n",pathname,dirname);
			free(receivedFile);
		}
	}
	serveranswer=-1;
	MINUS_ONE_RETURN_WITHMSG(readn(clientFD, &serveranswer, sizeof(serveranswer)),EFAULT, printf("L'append del file \"%s\" è fallito!\n",pathname));
	printf("L'append del file \"%s\" è riuscito.\n",pathname);
	return 0;
}
