//
//  main.c
//  Client
//
//  Created on 09/06/21.
//

#define _GNU_SOURCE


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
	const char* socketname;
	char* dirname_capacitymisses;
	char* dirname_readnwrite;
	int msec;
	_Bool p;
	_Bool dflag;
	_Bool rflag;
	_Bool Rflag;
} Settings;

Settings clientsettings = {NULL,NULL,NULL,0,FALSE,FALSE,FALSE,FALSE};

int hour=18;
int minutes=0;

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
	char* abspath = malloc(sizeof(char)*PATH_MAX);
	struct dirent* entry = NULL;
	DIR* directory = opendir(path);
	while ((entry=readdir(directory))!=NULL)
	{
		if (n==0)
			break;
		if (strcmp(entry->d_name, ".DS_Store") == 0) {}
		else if (entry->d_type==DT_DIR)
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {}
			else
			{
				if (realpath(path, abspath)==NULL) {
					free(abspath);
					return;
				}
				abspath=strcat(abspath, "/");
				abspath=strcat(abspath, entry->d_name);
				recursiveReadFolders(abspath, n);
			}
			else if (entry->d_type==DT_REG)
			{

				if (realpath(path, abspath)==NULL) {
					free(abspath);
					return;
				}
				abspath=strcat(abspath, "/");
				abspath=strcat(abspath, entry->d_name);
				if (writeFile(abspath, clientsettings.dirname_capacitymisses)==-1) {
					printf("Scrittura del file fallita! C'è bisogno dell'algoritmo di rimpiazzamento.\n");
					printf("Causa dell'errore: %s \n",strerror(errno));
				}
				usleep(clientsettings.msec*1000);
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
	int n=0;
	if (stringn==NULL)
		n=-1;
	else
		sscanf(stringn, "n=%d",&n);
	if (n==-1 || n==0) {
		if (clientsettings.p) printf("Scrittura di tutti i file della cartella %s\n",dirname); }
	else
		if (clientsettings.p) printf("scrittura di al più %d file separati da virgola\n",n);

	char* abspath=malloc(sizeof(char)*PATH_MAX);
	strncpy(abspath,dirname,PATH_MAX);
	if (clientsettings.p) printf("Il path è %s\n",abspath);
	if (realpath(dirname, abspath)==NULL) {
		free(abspath);
		return -1;
	}
	recursiveReadFolders(abspath,&n);
	free(abspath);
	return 0;
}
int W_handler(char* string, char* dirname)
{
	if (clientsettings.p) printf("Scrittura di uno o più file separati da virgola\n");
	char* tempstring;
	if (string==NULL)
		return -1;
	char* currentfile=malloc(sizeof(char)*1024);
	strcpy(currentfile,strtok_r(string, ",",&tempstring));
	while (currentfile!=NULL)
	{
		if (clientsettings.p) printf("Sto per scrivere il file \"%s\".\n",currentfile);
		if ((writeFile(currentfile, dirname)==-1)) {
			if (clientsettings.p) printf("Errore nella scrittura del file \"%s\"!\n",currentfile);
			printf("Causa dell'errore: %s \n",strerror(errno));
		}
		else
			if (clientsettings.p) printf("Il file \"%s\" è stato scritto correttamente.\n",currentfile);
		usleep(clientsettings.msec*1000);
		char* token=strtok_r(NULL, ",",&tempstring);
		if (token==NULL)
			break;
		strcpy(currentfile,token);
	}
	free(currentfile);
	if (clientsettings.p) printf("Fine della scrittura di tutti i file\n");
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
	char* currentfile=malloc(sizeof(char)*PATH_MAX);
	strcpy(currentfile,strtok_r(string, ",",&tempstring));

	while (currentfile!=NULL)
	{
		if (clientsettings.p) printf("Leggo il file %s...\n",currentfile);
		if (readFile(currentfile, &buf,&filesize)==-1) {
			printf("Lettura del file %s fallita!\n",currentfile);
			printf("Causa dell'errore: %s \n",strerror(errno));
		}
		else {
			printf("Lettura del file %s riuscita.\n",currentfile);
		if (dirname!=NULL) {
			size_t pathlength = strlen(dirname)+strlen(currentfile)+2;
			char* savepathname=malloc(sizeof(char)*pathlength);
			memset(savepathname, '\0', pathlength);
			strncpy(savepathname,dirname,strlen(dirname));
			strncat(savepathname, "/",1);
			strncat(savepathname, currentfile,pathlength);
			if (saveLocalFile(buf, savepathname, filesize)==-1)
				printf("Salvataggio del file %s fallito!\n",currentfile);
			else
				printf("Salvataggio del file %s riuscito!\n",currentfile);
			free(savepathname);
		}
		}
		free(buf);
		usleep(clientsettings.msec*1000);
		char* token=strtok_r(NULL, ",",&tempstring);
		if (token==NULL)
			break;
		strncpy(currentfile,token,strlen(token));
	}
	free(currentfile);
	if (clientsettings.p) printf("Tutti i file sono stati letti. Esco.");
	return 0;
}
int l_handler(char* string)
{
	if (clientsettings.p) printf("Lock di uno o più file separati da virgola\n");
	char* tempstring;
	if (string==NULL)
		return -1;
	char* currentfile=malloc(sizeof(char)*PATH_MAX+1);
	memset(currentfile, '\0', PATH_MAX+1);
	strncpy(currentfile,strtok_r(string, ",",&tempstring),PATH_MAX);

	while (currentfile!=NULL)
	{
		if (clientsettings.p) printf("Lock del file %s...\n",currentfile);
		if ((lockFile(currentfile)==-1)) {
			if (clientsettings.p) printf("Errore nel lock del file \"%s\"!\n",currentfile);
			printf("Causa dell'errore: %s \n",strerror(errno));
		}
		else
			if (clientsettings.p) printf("Lock del file \"%s\" riuscito.\n",currentfile);
		usleep(clientsettings.msec*1000);
		char* token=strtok_r(NULL, ",",&tempstring);
		if (token==NULL)
			break;
		strcpy(currentfile,token);
	}
	if (clientsettings.p) printf("Fine richiesta di lock dei file.\n");
	free(currentfile);
	return 0;
}
int u_handler(char* string)
{
	if (clientsettings.p) printf("Unlock di uno o più file separati da virgola\n");
	char* tempstring;
	if (string==NULL)
		return -1;
	char* currentfile=malloc(sizeof(char)*PATH_MAX+1);
	memset(currentfile, '\0', PATH_MAX+1);
	strncpy(currentfile,strtok_r(string, ",",&tempstring),PATH_MAX);

	while (currentfile!=NULL)
	{
		if (clientsettings.p) printf("Unlock del file %s...\n",currentfile);
		if (unlockFile(currentfile)==-1) {
		if (clientsettings.p) printf("Unlock del file %s, fallito!\n",currentfile);
			if (clientsettings.p)	printf("Causa dell'errore: %s \n",strerror(errno));
		}
		else
			if (clientsettings.p) printf("Unlock del file %s riuscito.\n",currentfile);
		usleep(clientsettings.msec*1000);
		char* token=strtok_r(NULL, ",",&tempstring);
		if (token==NULL)
			break;
		strcpy(currentfile,token);
	}
	if (clientsettings.p) printf("Fine richiesta di unlock dei file.\n");
	free(currentfile);
	return 0;
}
int C_handler(char* string)
{
	if (clientsettings.p) printf("Chiusura di uno o più file separati da virgola\n");
	char* tempstring;
	if (string==NULL)
		return -1;
	char* currentfile=malloc(sizeof(char)*PATH_MAX+1);
	memset(currentfile, '\0', PATH_MAX+1);
	strncpy(currentfile,strtok_r(string, ",",&tempstring),PATH_MAX);

	while (currentfile!=NULL)
	{
		if (clientsettings.p) printf("Chiusura del file %s...\n",currentfile);
		if (closeFile(currentfile)==-1) {
			if (clientsettings.p) printf("Chiusura del file %s, fallita!\n",currentfile);
			if (clientsettings.p)	printf("Causa dell'errore: %s \n",strerror(errno));
		}
		else
			if (clientsettings.p) printf("Chiusura del file %s riuscita.\n",currentfile);
		usleep(clientsettings.msec*1000);
		char* token=strtok_r(NULL, ",",&tempstring);
		if (token==NULL)
			break;
		strcpy(currentfile,token);
	}
	if (clientsettings.p) printf("Fine richiesta di chiusura dei file.\n");
	free(currentfile);
	return 0;
}
int O_handler(char* string)
{
	if (clientsettings.p) printf("Apertura di uno o più file separati da virgola\n");
	char* tempstring;
	if (string==NULL)
		return -1;
	char* currentfile=malloc(sizeof(char)*PATH_MAX+1);
	memset(currentfile, '\0', PATH_MAX+1);
	strncpy(currentfile,strtok_r(string, ",",&tempstring),PATH_MAX);

	while (currentfile!=NULL)
	{
		if (clientsettings.p) printf("Apertura del file %s...\n",currentfile);
		if (openFile(currentfile,0)==-1) {
			if (clientsettings.p) printf("Apertura del file %s, fallita!\n",currentfile);
			if (clientsettings.p)	printf("Causa dell'errore: %s \n",strerror(errno));
		}
		else
			if (clientsettings.p) printf("Apertura del file %s riuscita.\n",currentfile);
		usleep(clientsettings.msec*1000);
		char* token=strtok_r(NULL, ",",&tempstring);
		if (token==NULL)
			break;
		strcpy(currentfile,token);
	}
	if (clientsettings.p) printf("Fine richiesta di apertura dei file.\n");
	free(currentfile);
	return 0;
}
int c_handler(char* string)
{
	char* tempstring;
	if (string==NULL)
		return -1;
	char* currentfile=malloc(sizeof(char)*PATH_MAX+1);
	memset(currentfile, '\0', PATH_MAX+1);
	strncpy(currentfile,strtok_r(string, ",",&tempstring),PATH_MAX);

	while (currentfile!=NULL)
	{
		if (removeFile(currentfile)==-1) {
			if (clientsettings.p) printf("Errore nella rimozione del file!");
			if (clientsettings.p) printf("Causa dell'errore: %s \n",strerror(errno));
		}
		usleep(clientsettings.msec*1000);
		char* token=strtok_r(NULL, ",",&tempstring);
		if (token==NULL)
			break;
		strcpy(currentfile,token);
	}
	free(currentfile);
	return 0;
}
int R_handler(char* stringn)
{
	int n=0;
	if (stringn==NULL)
		n=-1;
	else if (isNumeric(stringn))
		sscanf(stringn, "n=%d",&n);
	else
		return -1;
	int bytesread=readNFiles(n, clientsettings.dirname_readnwrite);
	printf("Fine dell'operazione di lettura di N files. Letti %d bytes.\n",bytesread);
	return 0;
}
int a_handler(char* string)
{
	//prende due stringhe separate da virgola,la prima è il nome del file e la seconda i dati da appendere
	char* tempstring;
	if (string==NULL)
		return -1;
	char* filetoappend=malloc(sizeof(char)*PATH_MAX+1);
	memset(filetoappend, '\0', PATH_MAX+1);
	strncpy(filetoappend,strtok_r(string, ",",&tempstring),PATH_MAX);
	char* datatoappend=malloc(sizeof(char)*_POSIX_PIPE_BUF+1);
	memset(datatoappend, '\0', _POSIX_PIPE_BUF+1);
	strncpy(datatoappend,strtok_r(NULL, ",",&tempstring),_POSIX_PIPE_BUF);
	if (filetoappend==NULL || datatoappend==NULL) {
		if (clientsettings.p) printf("I parametri passati all'append non sono corretti!\n");
		return -1;
	}
	size_t size = strlen(datatoappend);
	if (appendToFile(filetoappend, datatoappend, size, clientsettings.dirname_capacitymisses)) {
		if (clientsettings.p) printf("Append del file fallito!\n");
		if (clientsettings.p) printf("Causa dell'errore: %s \n",strerror(errno));
	}
	usleep(clientsettings.msec*1000);
	return 0;
}

int parseSettings(int argc, const char **argv) {
	printf("Parso le impostazioni del client.\n");
	int opt;
	while ((opt = getopt(argc, (char *const *) argv, "phf:w:W:D:d:t:p:r:R:f:l:u:c:a:C:O:H:M:")) != -1)
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
				clientsettings.msec=atoi(optarg);
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
				if (optarg==NULL)
				{
					printf("L'opzione -f è necessaria!");
					exit(EXIT_FAILURE);
				}
				clientsettings.socketname=optarg;
				break;
			case 'h':
				printHelp();
				break;
			case 'H':
				hour=atoi(optarg);
				break;
			case 'M':
				minutes=atoi(optarg);
				break;
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
	return 0;
}

void parseCmdLine(int argc, const char * argv[])
{
	if (clientsettings.dflag==TRUE && (clientsettings.rflag==FALSE && clientsettings.Rflag==FALSE))
	{
		printf("Opzioni non valide: -d deve essere usata insieme a -r o -R");
		exit(EXIT_FAILURE);
	}
	optind=1;
	int opt;
	while ((opt = getopt(argc, (char *const *) argv, "phf:w:W:D:d:t:p:r:R:f:l:u:c:a:C:O:H:M:")) != -1)
	{
		switch (opt) {
			case 'w':
				w_handler(optarg);
				break;
			case 'W':
				W_handler(optarg,clientsettings.dirname_capacitymisses);
				break;
			case 'r':
				r_handler(optarg,clientsettings.dirname_readnwrite);
				break;
			case 'R':
				R_handler(optarg);
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
				break;
			case 'C':
				C_handler(optarg);
				break;
			case 'O':
				O_handler(optarg);
				break;
			case 'D':
			case 'd':
			case 't':
			case 'p':
				break;
			case ':':
				printf("L'opzione %c inserita sarà ignorata perché richiede un argomento.",optopt);
				break;
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
		if (scanf("%2d:%2d",hour,minutes)!=0)
			printf("Orario invalido!");
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
	tspec.tv_nsec=0;
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
	if (clientsettings.p) printf("Impostazioni del client caricate.\n");
	const struct timespec abstime=inputTime(&hour,&minutes);
	if (clientsettings.p) printf("Mi connetto al server...\n");
	if (openConnection(clientsettings.socketname,clientsettings.msec,abstime)==-1) {
		printf("Non connesso");
		exit(EXIT_FAILURE);
	}
	if (clientsettings.p) printf("Connessione riuscita\n");
	parseCmdLine(argc, argv);
	closeConnection(clientsettings.socketname);
	if (clientsettings.p) printf("Chiusura client\n");
	return 0;
}
