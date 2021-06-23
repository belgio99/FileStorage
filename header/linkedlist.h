//
//  linkedlist.h
//  Server
//
//  Created by BelGio on 20/06/21.
//

#ifndef linkedlist_h
#define linkedlist_h

#include <stdio.h>

typedef struct file {
	char* path;
	_Bool isOpened;
	_Bool isLocked;
	pthread_mutex_t filemtx;
	int user;
	size_t size;
	char* filecontent;
} serverFile;


typedef struct _list{
	int fd;
	struct _list *next;
} list;

typedef struct _list2{
	int fd;
	serverFile *fileopened;
	struct _list2 *next;
} listfiles;


int insertNode(int node, list **last, list **l);
int removeNode(list **l, list **last);
int insertInList(int node, serverFile **file, listfiles **l);
serverFile* checkAndRemoveDuplicate(int node, listfiles **l);
serverFile* checkAndRemoveSamePath(char *path, listfiles **l);


#endif /* linkedlist_h */
