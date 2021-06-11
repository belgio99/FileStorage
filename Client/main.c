//
//  main.c
//  Client
//
//  Created on 09/06/21.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/un.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
//#include <stdint.h>

#define UNIX_PATH_MAX 104
#define SOCKNAME "asdac"
#define N 100

void printHelp(void)
{
	printf("helpone");
	exit(0);
}

void StringTokenizer(char * string)
{
	char* tokenized = strtok(string, ",");
	while (tokenized!=NULL)
	{
		tokenized = strtok(NULL, ",");
	}
	
}
char* readFile(char* pathname)
{
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
	return text;
}
void parseCmdLine(int argc, const char * argv[],const char* sockname)
{
	int opt;
	while ((opt = getopt(argc, (char *const *) argv, "hf:W:r:R:d:t:l:u:c:p:")) != -1)
	{
		switch (opt) {
			case 'h':
				printHelp();
				break;
			case 'f':
				sockname=optarg;
				//sockname=readFile(optarg); //Da implementare
				break;
			case 'W':
				StringTokenizer(optarg);
				break;
			case 'r':
				break;
			case 'R':
				break;
			case 'd':
				break;
			case 't':
				break;
			case 'l':
				break;
			case 'u':
				break;
			case 'c':
				break;
			case 'p':
				break;
			default:
				break;
		}
	}
}

int openConnection(const char* sockname, int msec, const struct timespec abstime)
{
	struct sockaddr_un sa; //test
	strncpy(sa.sun_path, sockname,UNIX_PATH_MAX);
	sa.sun_family=AF_UNIX;
	int sk=socket(AF_UNIX,SOCK_STREAM,0);
	struct sockaddr* test = (struct sockaddr*)&sa;
	int connstatus;
	while ((connstatus = connect(sk,test, sizeof(sa)) == -1 && abstime.tv_sec>time(NULL)))
		usleep(msec*1000);
	
	return connstatus;
}

void closeConnection(const char* sockname)
{
	
}
struct timespec inputTime(int* hour, int* minutes)
{
	printf("Inserire un orario in formato HH:MM\n");
	scanf("%2d:%2d",hour,minutes);
	while (*hour>=24 && *hour<0 && *minutes>=60 && *minutes<0)
	{
		printf("Orario non corretto. Inserire un orario corretto in formato HH:MM\n");
		scanf("%2d:%2d",hour,minutes);
	}
	time_t t = time(NULL);
	struct tm time=*localtime(&t);
	time.tm_hour=*hour;
	time.tm_min=*minutes;
	time.tm_sec=0;
	time_t epoch = mktime(&time);
	if (epoch<t)
		epoch=epoch+86400;
	struct timespec tspec;
	tspec.tv_sec=epoch;
	return tspec;
	
}

int main(int argc, const char * argv[]) {
	
	char* sockname;
	parseCmdLine(argc, argv, sockname);
	int hour=-1, minutes=-1;
	const struct timespec abstime=inputTime(&hour,&minutes);
	if (openConnection(sockname,1000,abstime)==-1) {
		printf("Non connesso");
		exit(EXIT_FAILURE);
	}
	
	
	
	return 0;
}
