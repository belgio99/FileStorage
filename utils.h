//
//  utils.h
//  Client
//
//  Created by BelGio on 18/06/21.
//

#ifndef utils_h
#define utils_h

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/fcntl.h>
#include <sys/stat.h>



#define OPENFILE							1
#define OPENCREATE 						2
#define OPENLOCK 							3
#define OPENLOCKCREATE 					4
#define READ 								5
#define READN 								6
#define APPEND 							7
#define LOCK 								8
#define UNLOCK 							9
#define CLOSEFILE 						10
#define REMOVE 							11
#define CLOSECONN 						12
#define WRITE								13

#define SUCCESS_OPEN    				1
#define SUCCESS_CREATE  				2
#define SUCCESS_LOCK    				3
#define SUCCESS_CREATELOCK 			4
#define ERROR_FILEALREADY_EXISTS		5
#define ERROR_FILEALREADY_OPENED 	6
#define ERROR_FILEISLOCKED				7
#define ERROR_FILENOTFOUND				404
#define ERROR_NOSPACELEFT 				8

#define O_CREATE 							1
#define O_LOCK 							2
#define O_LOCK_CREATE 					3

#define UNIX_PATH_MAX 					104
#define FALSE 								0
#define TRUE 								1

#define SOCKNAME 							"blablabla"

#endif /* utils_h */
ssize_t readn(int fd, void *ptr, size_t n);
ssize_t writen(int fd, void *ptr, size_t n);
int isNumeric (const char * s);
char* readLocalFile(int fd, const char* pathname, size_t* size);
