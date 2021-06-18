//
//  api.c
//  Client
//
//  Created by BelGio on 14/06/21.
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
			if (VERBOSEMODE) printf("Provo ad aprire \"%s\" in modalità di creazione.\n",pathname);
			break;
		case O_LOCK:
			request = OPENLOCK;
			if (VERBOSEMODE) printf("Provo ad aprire \"%s\" in modalità locked.\n",pathname);
			break;
		case O_LOCK_CREATE:
			request = OPENLOCKCREATE;
			if (VERBOSEMODE) printf("Provo ad aprire \"%s\" in modalità locked e di creazione.\n",pathname);
			break;
		default:
			if (VERBOSEMODE) printf("I flags non sono stati riconosciuti. Il file \"%s\" verrà aperto in modalità read-only.\n",pathname);
		case 0:
			request = OPENFILE;
			if (VERBOSEMODE) printf("Provo ad aprire \"%s\" in modalità read-only.\n",pathname);
			break;
	}
	int serveranswer;
	if (writen(clientFD, &request, sizeof(request))==-1)
	{
		errno=EFAULT;
		return -1;
	}
	size_t pathnamelenght = strlen(pathname)+1;
	if(writen(clientFD, &pathnamelenght, sizeof(size_t)) == -1){
		errno = EFAULT;
		return -1;
	}
	if(writen(clientFD, (void * )pathname, pathnamelenght) == -1){
		errno = EFAULT;
		return -1;
	}
	if(readn(clientFD, &serveranswer, sizeof(serveranswer)) == -1){
		errno = EFAULT;
		return -1;
	}
	switch (serveranswer){
		case SUCCESS_OPEN:
			if (VERBOSEMODE) printf("Il file \"%s\" è stato aperto.\n",pathname);
			break;
		case SUCCESS_CREATE:
			if (VERBOSEMODE) printf("Il file \"%s\" è stato creato.\n",pathname);
			break;
		case SUCCESS_LOCK:
			if (VERBOSEMODE) printf("Il file \"%s\" è stato lockato con successo.\n",pathname);
			break;
		case SUCCESS_CREATELOCK:
			if (VERBOSEMODE) printf("Il file \"%s\" è stato creato e lockato con successo.\n",pathname);
			break;
		case ERROR_FILENOTFOUND:
			if (VERBOSEMODE) printf("Il file \"%s\" non è stato trovato nel server.\n",pathname);
			return -1;
			break;
		case ERROR_FILEISLOCKED:
			if (VERBOSEMODE) printf("Non posso accedere al file \"%s\" perché risulta lockato.\n",pathname);
			return -1;
			break;
		case ERROR_FILEALREADY_EXISTS:
			if (VERBOSEMODE) printf("Il file \"%s\" è già esistente nel server.\n",pathname);
			return -1;
		case ERROR_FILEALREADY_OPENED:
			if (VERBOSEMODE) printf("Il file \"%s\" è aperto.\n",pathname);
			return -1;
		case ERROR_NOSPACELEFT:
			if (VERBOSEMODE) printf("Il server è pieno.\n");
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
		if (VERBOSEMODE) printf("Il file \"%s\" non è stato trovato.\n",pathname);
		errno = EINVAL;
		return -1;
	}
	if (writen(clientFD, &request, sizeof(request)) == -1)
	{
		errno = EFAULT;
		return -1;
	}
	if ((openreturnvalue=openFile(pathname, O_LOCK_CREATE) == -1))
	{
		if (VERBOSEMODE) printf("Non posso creare e lockare il file\"%s\" nel server.\n",pathname);
		errno=EFAULT;
		free(filetosend);
		return -1;
	}
	if(writen(clientFD, &filesize, sizeof(filesize)) == -1){
		errno = EFAULT;
		free(filetosend);
		return -1;
	}
	if(writen(clientFD, filetosend, filesize) == -1){
		errno = EFAULT;
		free(filetosend);
		return -1;
	}
	if (VERBOSEMODE) printf("File inviato al server!\n");
	if (VERBOSEMODE) printf("Attendo la risposta dal server per eventuali file da ricevere...\n");
	free(filetosend);
	
	int serveranswer;
	if(readn(clientFD, &serveranswer, sizeof(size_t)) == -1){
		errno = EQFULL;
		return -1;
	}
	if (serveranswer==ERROR_NOSPACELEFT) {
		
		size_t bytestoreceive;
		
		if(readn(clientFD, &bytestoreceive, sizeof(bytestoreceive)) == -1){
			errno = EFAULT;
			return -1;
		}
		while (bytestoreceive>0) {
			char* receivedFile = malloc(bytestoreceive);
			if (receivedFile==NULL)
				return -1;
			if (saveLocalFile(receivedFile, dirname, (size_t*)bytestoreceive)==-1)
				if (VERBOSEMODE) printf("Impossibile salvare il file \"%s\" nella cartella \"%s\".\n",pathname,dirname);
			free(receivedFile);
		}
	}
	if(serveranswer == ERROR_FAILTOSAVE){
		if (VERBOSEMODE) printf("Scrittura del file \"%s\" fallita!\n",pathname);
		errno = EFAULT;
		return -1;
	}

	if (VERBOSEMODE) printf("Scrittura del file \"%s\" riuscita. Sono stati scritti %zu bytes\n",pathname,filesize);
	return 0;
}
