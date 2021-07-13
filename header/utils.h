//
//  utils.h
//  Client
//
//  Created on 18/06/21.
//


#ifndef utils_h
#define utils_h

#include <sys/types.h>
#include <time.h>
#include <limits.h>


#define OPENFILE							1
#define OPENCREATE 						2
#define OPENLOCK 							3
#define OPENLOCKCREATE 					4
#define READ 								5
#define READN 								6
#define APPEND 							7
#define LOCKFILE 							8
#define UNLOCKFILE 						9
#define CLOSEFILE 						10
#define REMOVE 							11
#define CLOSECONN 						12
#define WRITE								13

#define SUCCESS							0
#define ERROR_NOSPACELEFT 				11
#define ERROR 								-1

#define O_CREATE 							1
#define O_LOCK 							2
#define O_LOCK_CREATE 					3

#define UNIX_PATH_MAX 					104

#define FALSE 								0
#define TRUE 								1





ssize_t readn(int fd, void *ptr, size_t n);
ssize_t writen(int fd, void *ptr, size_t n);
int isNumeric (const char * s);
struct timespec;
char* readLocalFile(const char* pathname, size_t* filesize);
int saveLocalFile(const char* file, const char* pathname, size_t filesize);
#endif /* utils_h */
