//
//  utils.c
//  Client
//
//  Created on 18/06/21.
//

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include "../header/utils.h"



ssize_t readn(int fd, void *ptr, size_t n) {
	size_t   nleft;
	ssize_t  nread;
	
	nleft = n;
	while (nleft > 0) {
		if((nread = read(fd, ptr, nleft)) < 0) {
			if (nleft == n) return -1;
			else break;
		} else if (nread == 0) break;
		nleft -= nread;
		ptr   += nread;
	}
	return(n - nleft);
}

ssize_t writen(int fd, void *ptr, size_t n) {
	size_t   nleft;
	ssize_t  nwritten;
	
	nleft = n;
	while (nleft > 0) {
		if((nwritten = write(fd, ptr, nleft)) < 0) {
			if (nleft == n) return -1;
			else break;
		} else if (nwritten == 0) break;
		nleft -= nwritten;
		ptr   += nwritten;
	}
	return(n - nleft);
}
int isNumeric (const char * s)
{
	if (s==NULL || *s=='\0' || isspace(*s))
		return 0;
	char* p;
	strtod (s, &p);
	return *p == '\0';
}

char* readLocalFile(const char* pathname, size_t* filesize)
{
	int fd;
	if ((fd=open(pathname,O_RDONLY))==-1)
		return NULL;
	struct stat fdbuffer;
	fstat(fd,&fdbuffer);
	*filesize=fdbuffer.st_size+1;
	char* file;
	if((file = malloc(fdbuffer.st_size+1))==NULL)
		return NULL;
	memset(file,'\0',(int)fdbuffer.st_size+1);
	if (readn(fd, file, fdbuffer.st_size+1)==-1)
		return NULL;
	close(fd);
	return file;
}
int saveLocalFile(const char* filecontents, const char* path, size_t filesize)
{
	int fd;
	if((fd = open(path, O_RDWR|O_CREAT , 0666)) == -1 || writen(fd, (void*)filecontents, filesize) == -1)
		return -1;
	close(fd);
	return 0;
}
