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

#include "utils.h"

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
const char* readSocketFile(char* pathname)
{
	char* text=malloc(sizeof(char)*35);
	FILE* fPtr = NULL;
	fPtr=fopen(pathname, "r");
	if (fPtr==NULL)
	{
		printf("File del socket non esistente in %s\n",pathname);
		exit(EXIT_FAILURE);
	}
	fscanf(fPtr, "%s",text);
	if (text==NULL)
	{
		printf("Il nome del socket è vuoto o non valido.\n");
		exit(EXIT_FAILURE);
	}
	fclose(fPtr);
	return text;
}
