//
//  main.c
//  Server
//
//  Created on 09/06/21.
//
/*
 parse argv
 parse config
 create dispatcher thread
 create socket
 
 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#define UNIX_PATH_MAX 104
#define SOCKNAME "asdac"
#define N 100

#define MAXFILES 100


void socketListener(int sk){
	int i=0;
	while(accept(sk, NULL, 0)<0)
	{
		//printf("waiting\n");
		sleep(1);
		printf("Waiting for: %d\n",i++);
	}
	printf("connected!");
}
int createServerSocket(void)
{
	struct sockaddr_un sa;
	sa.sun_family=AF_UNIX;
	strncpy(sa.sun_path, SOCKNAME,UNIX_PATH_MAX);
	int sk=socket(AF_UNIX,SOCK_STREAM,0);
	struct sockaddr* test = (struct sockaddr*)&sa;
	int a = bind(sk,test, sizeof(sa));
	printf("Bind result: %d\n",a);
	if (a<0)
		exit(EXIT_FAILURE);
	listen(sk, SOMAXCONN);
	return sk;
}
//static int storageFiles = 0;
//static int storageBytes = 0;
void readconfig(void)
{
	char* pathname="config.txt";
	char* text = NULL;
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
}

int main(int argc, const char * argv[]) {
	readconfig();
	
	pthread_t dispatcher;
	int sk = createServerSocket();
	pthread_create(&dispatcher,NULL,&socketListener, sk);
	//socketListener(sk);
	//int b = accept(sk, NULL, 0);
	sleep(400000);
	
}
