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
#include "../header/api.h"
#include "../header/utils.h"
#include <dirent.h>


static char* dirname;
static _Bool connectedflag = FALSE;

typedef struct settings
{
	char* socketname;
	char* dirname_capacitymisses;
	char* dirname_readnwrite;
	int time;
	_Bool p;
} Settings;

Settings clientsettings = {NULL,NULL,NULL,0,0};

void printHelp(void)
{
	printf("%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
			 "Utilizzo: ",
			 "-h",
			 "-f filename",
			 "-w dirname[,n=0]",
			 "-W file1[,file2]",
			 "-r file1[,file2]",
			 "-R[n=0]",
			 "-d dirname",
			 "-t time",
			 "-l file1[,file2]",
			 "-u file1[,file2]",
			 "-c file1[,file2]",
			 "-p");
	exit(0);
}

char* readSocketFile(char* pathname)
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
void recursiveReadFolders(char* abspath, int* n)
{
	struct dirent* entry = NULL;
	DIR* directory = opendir(abspath);
	while ((entry=readdir(directory))==NULL || n==0)
	{
		if (entry->d_type==DT_DIR)
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {}
			else
			{
				char* newpath=realpath(entry->d_name, NULL);
				recursiveReadFolders(newpath, n);
			}
			else if (entry->d_type==DT_REG)
			{
				abspath=(entry->d_name);
				writeFile(abspath, NULL);
				usleep(clientsettings.time*1000);
				n-=1;
			}
	}
	closedir(directory);
	return;
}
int w_handler(char* string)
{
	char* tempstring;
	char* dirname = strtok_r(string, ",",&tempstring);
	char* nstring = NULL;
	nstring = strtok_r(string, NULL ,&tempstring);
	int n=0;
	if (nstring!=NULL)
		sscanf(nstring, "n=%d",&n);
	char* abspath=realpath(dirname, NULL);
	recursiveReadFolders(abspath,&n);
	return 0;
}
int W_handler(char* string, char* dirname)
{
	char* tempstring;
	char* firstfile=strtok_r(string, ",",&tempstring);
	writeFile(firstfile, dirname);
	usleep(clientsettings.time*1000);
	char* currentfile=malloc(sizeof(char)*1024);
	while ((strcpy(currentfile,strtok_r(string, NULL ,&tempstring)))!=NULL)
	{
		writeFile(currentfile, dirname);
		usleep(clientsettings.time*1000);
	}
	free(currentfile);
	return 0;
}
int r_handler(char* string, char* dirname)
{
	void *buf = NULL;
	size_t filesize = -1;
	char* tempstring;
	char* firstfile=strtok_r(string, ",",&tempstring);
	writeFile(firstfile, dirname);
	usleep(clientsettings.time*1000);
	char* currentfile=malloc(sizeof(char)*1024);
	while ((strcpy(currentfile,strtok_r(string, NULL ,&tempstring)))!=NULL)
	{
		readFile(currentfile, &buf,&filesize);
		usleep(clientsettings.time*1000);
	}
	free(currentfile);
	return 0;
}
int l_handler(char* string)
{
	char* tempstring;
	char* firstfile=strtok_r(string, ",",&tempstring);
	writeFile(firstfile, dirname);
	char* currentfile=malloc(sizeof(char)*1024);
	while ((strcpy(currentfile,strtok_r(string, NULL ,&tempstring)))!=NULL)
	{
		lockFile(currentfile);
		usleep(clientsettings.time*1000);
	}
	free(currentfile);
	return 0;
}
int u_handler(char* string)
{
	char* tempstring;
	char* firstfile=strtok_r(string, ",",&tempstring);
	writeFile(firstfile, dirname);
	char* currentfile=malloc(sizeof(char)*1024);
	while ((strcpy(currentfile,strtok_r(string, NULL ,&tempstring)))!=NULL)
	{
		unlockFile(currentfile);
		usleep(clientsettings.time*1000);
	}
	free(currentfile);
	return 0;
}
int c_handler(char* string)
{
	char* tempstring;
	char* firstfile=strtok_r(string, ",",&tempstring);
	writeFile(firstfile, dirname);
	char* currentfile=malloc(sizeof(char)*1024);
	while ((strcpy(currentfile,strtok_r(string, NULL ,&tempstring)))!=NULL)
	{
		removeFile(currentfile);
		usleep(clientsettings.time*1000);
	}
	free(currentfile);
	return 0;
}
int R_handler(int N)
{
	readNFiles(N, dirname);
	return 0;
}

void parseCmdLine(int argc, const char * argv[],const char* sockname)
{
	int opt;
	while ((opt = getopt(argc, (char *const *) argv, "d:t:p")) != -1)
	{
		switch (opt) {
			case 'D':
				clientsettings.dirname_capacitymisses=optarg;
				break;
			case 'd':
				clientsettings.dirname_readnwrite=optarg;
				break;
			case 't':
				clientsettings.time=(int)optarg;
				break;
			case 'p':
				clientsettings.p=1;
				break;
		}
	}
	while ((opt = getopt(argc, (char *const *) argv, "hf:w:W:r:R:l:u:c:")) != -1)
	{
		switch (opt) {
			case 'h':
				printHelp();
				break;
			case 'f':
				sockname=optarg;
				break;
			case 'w':
				w_handler(optarg);
				break;
			case 'W':
				W_handler(optarg,dirname);
				break;
			case 'r':
				r_handler(optarg,dirname);
				break;
			case 'R':
				R_handler((int)optarg);
				break;
			case 'l':
				l_handler(optarg);
				break;
			case 'u':
				u_handler(optarg);
				break;
			case 'c':
				c_handler(optarg);
				break;
			case 'D':
				break;
			case 'd':
				break;
			case 't':
				break;
			case 'p':
				break;
			case ':':
				printf("L'opzione %c inserita sarà ignorata perché richiede un argomento.",optopt);
			case '?':
				printf("Opzione non supportata!");
				exit(EXIT_FAILURE);
				break;
			default:
				break;
		}
	}
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
