//
//  api.c
//  Client
//
//  Created on 14/06/21.
//
#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif

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
#include <libgen.h>

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
#define CHECKNULL(X,err)																\
if (X==NULL) {																			\
errno=err;																				\
return -1; }
#define NULL_RETURN(X,err)													\
if ((X)==-1) {																			\
errno = err;																			\
return NULL;																				\
}
#define NULL_RETURN_WITHFREE(X,err,elemtofree)													\
if ((X)==-1) {																			\
errno = err;																			\
free(elemtofree);																	\
return NULL;																				\
}

static _Bool connected = FALSE;
typedef struct sockaddr_un serverAddress;
serverAddress sa;
static int clientFD;

typedef struct cfile {
	char* path;
	size_t size;
	char* filecontent;
} clientFile;



int sendRequestAndPathname(int request, const char* pathname) {
	char* basepath=basename((char*)pathname);
	size_t pathlength = strlen(basepath);
	MINUS_ONE_RETURN(writen(clientFD, &request, sizeof(request)),EFAULT);
	MINUS_ONE_RETURN(writen(clientFD, &pathlength, sizeof(pathlength)),EFAULT);
	MINUS_ONE_RETURN(writen(clientFD, (void * )basepath, pathlength),EFAULT);
	return 0;
}

/*clientFile* receiveServerFile(void) {
	clientFile* fileReceived = malloc(sizeof(clientFile));
	size_t pathlength = 0;
	NULL_RETURN_WITHFREE(readn(clientFD, &pathlength, sizeof(pathlength)), EFAULT,fileReceived);
	NULL_RETURN_WITHFREE(readn(clientFD, &fileReceived->path, (int)pathlength), EFAULT,fileReceived);
	NULL_RETURN_WITHFREE(readn(clientFD, &fileReceived->size, sizeof(size_t)), EFAULT,fileReceived);
	NULL_RETURN_WITHFREE(readn(clientFD, &fileReceived->filecontent, (int)fileReceived->size), EFAULT,fileReceived);
	return fileReceived;
}*/


int fileReplacement(int clientFD, const char* dirname)
{
	int serverAnswer;

	while (1)
	{
		size_t pathlength = 0;
		size_t filesize = 0;
		MINUS_ONE_RETURN(readn(clientFD, &serverAnswer, sizeof(serverAnswer)),EFAULT);
		if (serverAnswer==SUCCESS || serverAnswer==ERROR)
			break;
		MINUS_ONE_RETURN(readn(clientFD, &pathlength, sizeof(pathlength)),EFAULT);
		if (pathlength==0 || pathlength>PATH_MAX) {
			serverAnswer=ERROR;
			break;
		}
		char* pathname=malloc(sizeof(char)*pathlength+1);
		memset(pathname, '\0', pathlength+1);
		MINUS_ONE_RETURN_WITHFREE(readn(clientFD, pathname, pathlength),EFAULT,pathname);
		MINUS_ONE_RETURN_WITHFREE(readn(clientFD, &filesize, sizeof(filesize)),EFAULT,pathname);
		char* filecontents=malloc(sizeof(char)*filesize+1);
		memset(filecontents, '\0', filesize+1);
		if (readn(clientFD, filecontents, filesize)==-1) {
			free(pathname);
			free(filecontents);
			errno=EFAULT;
			return -1;
		}
		if (dirname!=NULL) {
			char* savepathname=malloc(sizeof(char)*PATH_MAX+1);
			memset(savepathname, '\0', PATH_MAX+1);
			strncpy(savepathname,dirname,PATH_MAX);
			strncat(savepathname, "/",1);
			strncat(savepathname, pathname,pathlength);
			if (saveLocalFile(filecontents, savepathname, filesize)==-1)
				errno=EIO;
			free(pathname);
			free(filecontents);
			free(savepathname);
		}
	}
	return serverAnswer;
}


int openConnection(const char* sockname, int msec, const struct timespec abstime)
{
	memset(&sa, 0, sizeof(sa));
	if (sockname==NULL)
		return -1;
	strncpy(sa.sun_path, sockname,UNIX_PATH_MAX);
	sa.sun_family=AF_UNIX;
	clientFD=socket(AF_UNIX,SOCK_STREAM,0);
	int connstatus=-1;
	while ((connstatus = connect(clientFD,(struct sockaddr*)&sa, sizeof(sa))) == -1 && abstime.tv_sec>time(NULL))
		usleep(msec*1000);
	errno = (connstatus==-1) ? ETIMEDOUT : errno;
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
	CHECKNULL(pathname, EINVAL);
	int request;
	switch (flags)
	{
		case O_CREATE:
			request = OPENCREATE;
			break;
		case O_LOCK:
			request = OPENLOCK;
			break;
		case O_LOCK_CREATE:
			request = OPENLOCKCREATE;
			break;
		default:
		case 0:
			request = OPENFILE;
			break;
	}
	int serverAnswer;
	MINUS_ONE_RETURN(sendRequestAndPathname(request, (char*)pathname),EFAULT);
	MINUS_ONE_RETURN(readn(clientFD, &serverAnswer, sizeof(serverAnswer)),EFAULT);
	if (serverAnswer==-1)
		readn(clientFD, &errno, sizeof(errno));
	return serverAnswer;
}

int writeFile(const char* pathname, const char* dirname)
{
	size_t filesize;
	char* filetosend;
	int request = WRITE;
	if ((filetosend=readLocalFile(pathname, &filesize))==NULL)
	{
		errno = ENOENT;
		return -1;
	}
	MINUS_ONE_RETURN_WITHFREE(sendRequestAndPathname(request, (char*)pathname),EFAULT,filetosend);
	MINUS_ONE_RETURN_WITHFREE(writen(clientFD, &filesize, sizeof(filesize)),EFAULT,filetosend);
	MINUS_ONE_RETURN_WITHFREE(writen(clientFD, filetosend, filesize),EFAULT,filetosend);
	free(filetosend);
	int serverAnswer = -1;
	if ((serverAnswer = fileReplacement(clientFD, dirname)==0)) {
		MINUS_ONE_RETURN(readn(clientFD, &serverAnswer, sizeof(serverAnswer)),EFAULT); }
	else {
		readn(clientFD, &errno, sizeof(errno));
		return -1;
	}
	
	return serverAnswer;
}
int readFile(const char* pathname, void** buf, size_t* size)
{
	MINUS_ONE_RETURN(sendRequestAndPathname(READ, pathname),EFAULT);
	size_t pathlength = 0;
	MINUS_ONE_RETURN(readn(clientFD, &pathlength, sizeof(pathlength)),EFAULT);
	if (pathlength==0) {
		errno=ENOENT;
		return -1;
	}
	char* tmp = malloc(sizeof(char)*pathlength+1);
	memset(tmp, '\0', pathlength+1);
	MINUS_ONE_RETURN(readn(clientFD, tmp, pathlength),EFAULT);
	free(tmp);
	MINUS_ONE_RETURN(readn(clientFD, size, sizeof(size_t)),EFAULT);
	*buf = malloc(sizeof(char)**size+2);
	if(*buf==NULL) {
		errno = ENOMEM;
		return -1;
	}
	memset(*buf, '\0', *size+1);
	MINUS_ONE_RETURN(readn(clientFD, *buf, *size),EFAULT);
	int serverAnswer;
	MINUS_ONE_RETURN(readn(clientFD, &serverAnswer, sizeof(serverAnswer)),EFAULT);
	if (serverAnswer==-1)
		readn(clientFD, &errno, sizeof(errno));
	return 0;
}

int removeFile(const char* pathname)
{
	MINUS_ONE_RETURN(sendRequestAndPathname(REMOVE, pathname),EFAULT);
	int serverAnswer = -1;
	MINUS_ONE_RETURN(readn(clientFD, &serverAnswer, sizeof(serverAnswer)),EFAULT);
	if (serverAnswer==-1)
		readn(clientFD, &errno, sizeof(errno));
	return serverAnswer;
}


int lockFile(const char* pathname) {
	MINUS_ONE_RETURN(sendRequestAndPathname(LOCKFILE, pathname),EFAULT);
	int serverAnswer = -1;
	MINUS_ONE_RETURN(readn(clientFD, &serverAnswer, sizeof(serverAnswer)),EFAULT);
	if (serverAnswer==-1)
		readn(clientFD, &errno, sizeof(errno));
	return serverAnswer;
}
int unlockFile(const char* pathname) {
	MINUS_ONE_RETURN(sendRequestAndPathname(UNLOCKFILE, pathname),EFAULT);
	int serverAnswer = -1;
	MINUS_ONE_RETURN(readn(clientFD, &serverAnswer, sizeof(serverAnswer)),EFAULT);
	if (serverAnswer==-1)
		readn(clientFD, &errno, sizeof(errno));
	return serverAnswer;
}
int closeFile(const char* pathname) {
	MINUS_ONE_RETURN(sendRequestAndPathname(CLOSEFILE, pathname),EFAULT);
	int serverAnswer = -1;
	MINUS_ONE_RETURN(readn(clientFD, &serverAnswer, sizeof(serverAnswer)),EFAULT);
	if (serverAnswer==-1)
		readn(clientFD, &errno, sizeof(errno));
	return serverAnswer;
}

int readNFiles(int N, const char* dirname){
	sendRequestAndPathname(READN, "NULL");
	MINUS_ONE_RETURN(writen(clientFD, &N, sizeof(N)),EFAULT);
	int bytesread = 0;
	char* pathname = NULL;
	char* receivedFile = NULL;
	char* savepathname=malloc(sizeof(char)*PATH_MAX);
	size_t pathsize;
	while(1)
	{
		size_t filesize;
		MINUS_ONE_RETURN_WITHFREE(readn(clientFD, &pathsize, sizeof(pathsize)),EFAULT,savepathname);
		if (pathsize==0 || pathsize==-1)
			break;
		pathname=malloc(sizeof(char)*pathsize);
		memset(pathname, '\0', pathsize);
		MINUS_ONE_RETURN_WITHFREE(readn(clientFD, pathname, pathsize),EFAULT,savepathname);
		MINUS_ONE_RETURN_WITHFREE(readn(clientFD, &filesize, sizeof(size_t)),EFAULT,savepathname);
		receivedFile=malloc(filesize*sizeof(char));
		MINUS_ONE_RETURN_WITHFREE(readn(clientFD, receivedFile, filesize),EFAULT,savepathname);
		if (dirname!=NULL)
		{
			strcpy(savepathname,dirname);
			savepathname=strcat(savepathname, "/");
			savepathname=strncat(savepathname, pathname,pathsize);
			if (saveLocalFile(receivedFile, savepathname, filesize)==-1)
				errno=EIO;
		}
		bytesread+=filesize;
		filesize=0;
		memset(receivedFile, '\0', filesize);
		memset(savepathname, '\0', strlen(savepathname));
	}
	if (pathsize==-1)
		readn(clientFD, &errno, sizeof(errno));
	free(pathname);
	free(receivedFile);
	free(savepathname);
	return bytesread;
	
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname){
	MINUS_ONE_RETURN(sendRequestAndPathname(APPEND, pathname),EFAULT);
	MINUS_ONE_RETURN(writen(clientFD, &size, sizeof(size_t)),EFAULT);
	MINUS_ONE_RETURN(writen(clientFD, buf, size),EFAULT);
	int serverAnswer=-1;
	MINUS_ONE_RETURN(readn(clientFD, &serverAnswer, sizeof(int)),EFAULT);
	if (serverAnswer==ERROR_NOSPACELEFT)
		fileReplacement(clientFD, dirname);
	if(serverAnswer==ERROR){
		readn(clientFD, &errno, sizeof(errno));
		return -1;
	}
	return 0;
}
