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



typedef struct settings
{
	char* socketname;
	char* dirname_capacitymisses;
	char* dirname_readnwrite;
	int time;
	_Bool p;
	_Bool dflag;
	_Bool rflag;
	_Bool Rflag;
} Settings;

Settings clientsettings = {NULL,NULL,NULL,0,FALSE,FALSE,FALSE,FALSE};

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


void recursiveReadFolders(char* path, int *n)
{
	char* abspath = malloc(sizeof(char)*1000);
	struct dirent* entry = NULL;
	DIR* directory = opendir(path);
	while ((entry=readdir(directory))!=NULL)
		{
		if (n==0)
			break;
		if (entry->d_type==DT_DIR)
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {}
			else
				{

				realpath(path, abspath);
				abspath=strcat(abspath, "/");
				abspath=strcat(abspath, entry->d_name);
				recursiveReadFolders(abspath, n);
				}
			else if (entry->d_type==DT_REG)
				{
				realpath(path,abspath);
				abspath=strcat(abspath, "/");
				abspath=strcat(abspath, entry->d_name);
				writeFile(abspath, clientsettings.dirname_readnwrite);
				usleep(clientsettings.time*1000);
				n-=1;
				}
		}
	free(abspath);
	closedir(directory);
	return;
}
int w_handler(char* string)
{
	char* tempstring;
	char* dirname = strtok_r(string, ",",&tempstring);
	char* stringn = strtok_r(NULL, "," ,&tempstring);
	int n;
	if (stringn==NULL || atoi(stringn)==0)
		n=-1;
	else if (isNumeric(stringn))
		n=atoi(stringn);
	else
		return -1;
	printf("Scrittura di %d file separati da virgola",n);
	char* abspath=realpath(dirname, NULL);
	recursiveReadFolders(abspath,&n);
	return 0;
}
int W_handler(char* string, char* dirname)
{
	if (clientsettings.p) printf("Scrittura di uno o più file separati da virgola");
	char* tempstring;
	if (string==NULL)
		return -1;
	char* currentfile=malloc(sizeof(char)*1024);
	strcpy(currentfile,strtok_r(string, ",",&tempstring));
	while (currentfile!=NULL)
		{
		writeFile(currentfile, dirname);
		usleep(clientsettings.time*1000);
		char* token=strtok_r(NULL, ",",&tempstring);
		if (token==NULL)
			break;
		strcpy(currentfile,token);
		}
	free(currentfile);
	if (clientsettings.p) printf("Fine scrittura");
	return 0;
}
int r_handler(char* string, char* dirname)
{
	if (clientsettings.p) printf("Lettura di più file separati da virgola.\n");
	char* tempstring;
	void *buf = NULL;
	size_t filesize = -1;
	if (string==NULL)
		return -1;
	char* currentfile=malloc(sizeof(char)*1024);
	strcpy(currentfile,strtok_r(string, ",",&tempstring));

	while (currentfile!=NULL)
		{
		if (clientsettings.p) printf("Leggo il file %s.\n",currentfile);
		readFile(currentfile, &buf,&filesize);
		usleep(clientsettings.time*1000);
		char* token=strtok_r(NULL, ",",&tempstring);
		if (token==NULL)
			break;
		strcpy(currentfile,token);
		}
	free(currentfile);
	if (clientsettings.p) printf("Tutti i file sono stati letti. Esco.");
	return 0;
}
int l_handler(char* string)
{
	if (clientsettings.p) printf("Lock di uno o più file separati da virgola");
	char* tempstring;
	if (string==NULL)
		return -1;
	char* currentfile=malloc(sizeof(char)*1024);
	strcpy(currentfile,strtok_r(string, ",",&tempstring));

	while (currentfile!=NULL)
		{
		if (clientsettings.p) printf("Leggo il file %s.\n",currentfile);
		lockFile(currentfile);
		usleep(clientsettings.time*1000);
		char* token=strtok_r(NULL, ",",&tempstring);
		if (token==NULL)
			break;
		strcpy(currentfile,token);
		}
	if (clientsettings.p) printf("Tutti i file sono stati letti. Esco.");
	free(currentfile);
	return 0;
}
int u_handler(char* string)
{
	if (clientsettings.p) printf("Unlock di uno o più file separati da virgola");
	char* tempstring;
	if (string==NULL)
		return -1;
	char* currentfile=malloc(sizeof(char)*1024);
	strcpy(currentfile,strtok_r(string, ",",&tempstring));

	while (currentfile!=NULL)
		{
		unlockFile(currentfile);
		usleep(clientsettings.time*1000);
		char* token=strtok_r(NULL, ",",&tempstring);
		if (token==NULL)
			break;
		strcpy(currentfile,token);
		}
	free(currentfile);
	return 0;
}
int c_handler(char* string)
{
	char* tempstring;
	if (string==NULL)
		return -1;
	char* currentfile=malloc(sizeof(char)*1024);
	strcpy(currentfile,strtok_r(string, ",",&tempstring));

	while (currentfile!=NULL)
		{
		removeFile(currentfile);
		usleep(clientsettings.time*1000);
		char* token=strtok_r(NULL, ",",&tempstring);
		if (token==NULL)
			break;
		strcpy(currentfile,token);
		}
	free(currentfile);
	return 0;
}
int R_handler(int N)
{
	int bytesread=readNFiles(N, clientsettings.dirname_readnwrite);
	printf("Fine dell'operazione di lettura di N files. Letti %d bytes.\n",bytesread);
	return 0;
}
int a_handler(char* string)
{
	char* tempstring;
	if (string==NULL)
		return -1;
	char* filetoappend=malloc(sizeof(char)*1024);
	strcpy(filetoappend,strtok_r(string, ",",&tempstring));
	char* datatoappend=malloc(sizeof(char)*1024);
	strcpy(datatoappend,strtok_r(NULL, ",",&tempstring));
	size_t size = strlen(datatoappend);
	appendToFile(filetoappend, datatoappend, size, clientsettings.dirname_capacitymisses);
	usleep(clientsettings.time*1000);
	return 0;
}

static int parseSettings(int argc, const char **argv) {
	int opt;
	while ((opt = getopt(argc, (char *const *) argv, "hf:w:W:D:d:t:p:r:R:f:l:u:c:pa:")) != -1)
		{
		switch (opt) {
			case 'D':
				clientsettings.dirname_capacitymisses=optarg;
				break;
			case 'd':
				clientsettings.dirname_readnwrite=optarg;
				clientsettings.dflag=TRUE;
				break;
			case 't':
				clientsettings.time=atoi(optarg);
				break;
			case 'p':
				clientsettings.p=TRUE;
			case 'r':
				clientsettings.rflag=TRUE;
				break;
			case 'R':
				clientsettings.Rflag=TRUE;
				break;
			case 'f':
				clientsettings.socketname=malloc(sizeof(char)*35);
				strcpy(clientsettings.socketname, readSocketFile(optarg));
				break;
			case 'h':
			case 'w':
			case 'W':
			case 'l':
			case 'a':
			case 'u':
			case 'c':
			default:
				break;
		}
		}
	return opt;
}

void parseCmdLine(int argc, const char * argv[])
{
	if (clientsettings.dflag==TRUE && (clientsettings.rflag==FALSE && clientsettings.Rflag==FALSE))
		exit(EXIT_FAILURE);
	optind=1;
	int opt;
	while ((opt = getopt(argc, (char *const *) argv, "hf:w:W:D:d:t:p:r:R:f:l:u:c:pa:")) != -1)
		{
		switch (opt) {
			case 'h':
				printHelp();
				break;
			case 'w':
				w_handler(optarg);
				break;
			case 'W':
				W_handler(optarg,clientsettings.dirname_capacitymisses);
				break;
			case 'r':
				r_handler(optarg,clientsettings.dirname_capacitymisses);
				break;
			case 'R':
				R_handler(atoi(optarg));
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
			case 'a':
				a_handler(optarg);
			case 'D':
			case 'd':
			case 't':
			case 'p':
				break;
			case ':':
				printf("L'opzione %c inserita sarà ignorata perché richiede un argomento.",optopt);
			case '?':
				printf("Opzione non supportata!\n");
				exit(EXIT_FAILURE);
				break;
			default:
				break;
		}
		}
}




struct timespec inputTime(int* hour, int* minutes)
{
	*hour=-1;
	*minutes=-1;
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
	printf("Avvio client\n");
	if (argc==1)
	{
		printf("Errore, non sono stati passati gli argomenti corretti!\n");
		return 1;
	}
	parseSettings(argc, argv);
	int hour=18, minutes=-00;
	const struct timespec abstime=inputTime(&hour,&minutes);
	if (openConnection(clientsettings.socketname,1000,abstime)==-1) {
		printf("Non connesso");
		exit(EXIT_FAILURE);
	}
	parseCmdLine(argc, argv);
	closeConnection(clientsettings.socketname);
	free(clientsettings.socketname);
	printf("Chiusura client\n");
	return 0;
}
