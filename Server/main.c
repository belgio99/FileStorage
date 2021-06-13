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
#include <ctype.h>

#define UNIX_PATH_MAX 104
#define SOCKNAME "asdac"
#define N 100

#define MAXFILES 100

typedef struct {
	int threadnumber;
	int memorysize;
	char* socketname;
} Settings;

Settings serversettings = {0,0,NULL};

void fatalError(char* reason)
{
	perror(strcat("Errore: ", reason));
	exit(EXIT_FAILURE);
}

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
void readconfig(char* configfilepath)
{
	FILE* fPtr=fopen(configfilepath, "r");
	if (fPtr==NULL)
		fatalError("File di configurazione non trovato!\n");
	int sanitizedValue;
	while(!feof(fPtr))
	{
		char* strtolptr = NULL;
		char* currentsetting = malloc(sizeof(char)*50);
		char* value = malloc(sizeof(char)*50);
		fscanf(fPtr, "%[^= ]=%s\n",currentsetting,value);
		if (currentsetting==NULL || value==NULL)
			fatalError("Il file di configurazione ha uno o più valori non validi!\n");
		else {
			if (!strcmp(currentsetting, "threadnumber"))
			{
				if ((int)strtol(value, &strtolptr, 10)>0 && (sanitizedValue=atoi(value)>0)) // da mettere anche max valore
					serversettings.threadnumber=sanitizedValue;
				else
					fatalError("Numero di thread worker non valido!\n");
			}
			else if (!strcmp(currentsetting, "memorysize"))
			{
				if ((int)strtol(value, &strtolptr, 10)>0 && (sanitizedValue=atoi(value)>0))
					serversettings.memorysize=sanitizedValue;
				else
					fatalError("Dimensione della memoria del server non valida!\n");
			}
			else if (!strcmp(currentsetting, "socketname"))
			{
				serversettings.socketname=value;
			}
			else
				printf("Configurazione \"%s\" del server non supportata, verrà ignorata",currentsetting);
		}
	}
		fclose(fPtr);
		if (!serversettings.threadnumber)
		{
			printf("Non è stato specificato il numero di thread nel file di configurazione, verrà usato 1 come failsafe.\n");
			serversettings.threadnumber=1;
		}
		if (!serversettings.memorysize)
		{
			printf("Non è stata specificata la dimensione della memoria del server nel file di configurazione, verranno usati 100 MB come failsafe.\n");
			serversettings.threadnumber=1;
		}
		if (serversettings.socketname==NULL)
			fatalError("Non è stato specificato alcun file del socket!");

	}


int main(int argc, const char * argv[]) {
	char* configfilepath="config.txt";
	readconfig(configfilepath);
	
	pthread_t dispatcher;
	int sk = createServerSocket();
	pthread_create(&dispatcher,NULL,&socketListener, sk);
	//socketListener(sk);
	//int b = accept(sk, NULL, 0);
	sleep(400000);
	
}
