//
//  linkedlist.c
//  Server
//
//  Created on 20/06/21.
//



#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>

#include "../header/icl_hash.h"
#include "../header/linkedlist.h"


int insertNode(int node, list **last, list **l){
	list *new = malloc(sizeof(list));
	if(!new){
		perror("malloc");
		return -1;
	}
	new->fd = node;
	new->next = NULL;
	
	if(*last == NULL){
		*last = new;
		*l = new;
		return 0;
	}
	
	(*last)->next = new;
	return 0;
}

int removeNode(list **l, list **last){
	if(*l == NULL)
		return -1;
	list *tmp = *l;
	*l = (*l)->next;
	int ret = tmp->fd;
	if(*l == NULL)
		*last = NULL;
	free(tmp);
	return ret;
}

int insertInList(int node, serverFile **file, listfiles **l){
	listfiles *new = malloc(sizeof(listfiles));
	if(new == NULL){
		perror("malloc");
		return -1;
	}
	new->fd = node;
	new->fileopened = *file;
	new->next = *l;
	*l = new;
	return 0;
}
