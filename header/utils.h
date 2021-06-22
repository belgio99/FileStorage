//
//  utils.h
//  Client
//
//  Created by BelGio on 18/06/21.
//

#ifndef utils_h
#define utils_h



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
#define SUCCESS_OPEN    				1
#define SUCCESS_CREATE  				2
#define SUCCESS_OPENLOCK 				3
#define SUCCESS_OPENCREATE 			4
#define SUCCESS_OPENLOCKCREATE 		5
#define SUCCESS_LOCK    				6
#define SUCCESS_UNLOCK					7
#define ERROR_FILEALREADY_EXISTS		8
#define ERROR_FILEALREADY_OPENED 	9
#define ERROR_FILEISLOCKED				10
#define ERROR_FILENOTFOUND				404
#define ERROR_NOSPACELEFT 				11
#define ERROR_FAILTOSAVE				12
#define ERROR_FILETOOBIG				13
#define ERROR 								-1

#define O_CREATE 							1
#define O_LOCK 							2
#define O_LOCK_CREATE 					3

#define UNIX_PATH_MAX 					104
#define FALSE 								0
#define TRUE 								1


#define VERBOSEMODE 1

#endif /* utils_h */
ssize_t readn(int fd, void *ptr, size_t n);
ssize_t writen(int fd, void *ptr, size_t n);
int isNumeric (const char * s);
const char* readSocketFile(char* pathname);
