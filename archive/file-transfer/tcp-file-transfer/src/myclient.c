#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <stdint.h>

typedef struct {
	char** ip;
	int* port;
} servinfo;

typedef struct {
	int pid;
	int segment_start;
	int segment_size;
	char* filename;
} pthreadinfo;

servinfo *myserver;
pthreadinfo *myprocesses;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void* fileTransfer(void* myprocesses)
{
	pthreadinfo *thread = (pthreadinfo*)myprocesses;
	struct sockaddr_in servaddr;
	int sockfd, connfd;
	uint32_t byte_cursor, filesize;
	int writeerrno, numbytes;
	char chunk[2048], response;

	// init socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0) perror("socket() error");

	// init address
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(myserver->ip[2]);
	//servaddr.sin_addr.s_addr = inet_addr(myserver->ip[thread->pid]);
	servaddr.sin_port = htons(myserver->port[thread->pid]);	
	// connect socket
	connfd = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	if(connfd < 0) perror("connect() error"); 

	printf("---------- Thread %d ----------\n", thread->pid);

	// send filename
	write(sockfd, thread->filename, sizeof(thread->filename));
	printf("SEND: write filename to server\n");

	read(sockfd, &response, sizeof(response));

	if (strncmp(&response, "0", 1) == 0)
	{
		printf("FILE: '%s' accepted by server\n", thread->filename);	
	
		// request to download
		write(sockfd, "1", sizeof("1"));
		printf("AGENDA: download file from server\n");

		// send pointer to byte for download
		byte_cursor = htonl(thread->segment_start);
		writeerrno = write(sockfd, &byte_cursor, sizeof(byte_cursor));
		if(writeerrno < 0) {perror("write() error"); exit(-1);}
		printf("SEND: byte cursor - %d\n", thread->segment_start);

		// send file download size
		filesize = htonl(thread->segment_size);
		writeerrno = write(sockfd, &filesize, sizeof(filesize));
		if(writeerrno < 0) {perror("write() error"); exit(-1);}
		printf("SEND: segment size - %d\n", thread->segment_size);

		// read downloaded chunk
		numbytes = read(sockfd, chunk, thread->segment_size);
		if(numbytes < 0) {perror("read() error"); exit(-1);}
		printf("READ: thread %d (byte size: %d)\n", thread->pid, numbytes);
		printf("FILE: %s\n", chunk);
		printf("DONE: thread %d done downloading\n", thread->pid);
	}

	// start critical section
	pthread_mutex_lock(&mutex);

	// end critical section
	pthread_mutex_unlock(&mutex);

	pthread_exit(0);
}

int main(int argc, char* argv[])
{
	struct sockaddr_in servaddr;
	int sockfd, connfd;
	int num;
	int readerrno;
	char flag;
	int response;
	int filesize;

	// check command arguments
	if(argc != 4)
	{
		fprintf(stderr, "USAGE: ./myclient <server-info.text> <num-connections> <filename>");
		exit(-1);
	}

	// get server info
	FILE* servfile = fopen(argv[1], "r");
	char line[100];
	num = atoi(argv[2]);
	int actualnum = 0;
	int i = 0;
	myserver = calloc(2, sizeof(*myserver));
	myserver->ip = malloc(sizeof(*(myserver->ip))*num);
	myserver->port = malloc(sizeof(int)*num);
	while(fgets(line, 100, servfile) != NULL && i < num)
	{
		myserver->ip[i] = malloc(sizeof(myserver->ip)*16);
		memcpy(myserver->ip[i], strtok(line, " \t"), sizeof(char)*16);
		myserver->port[i] = atoi(strtok(NULL, " \t"));
		bzero(line, 100);
		actualnum++;
		i++;
	}
	num = actualnum;
	fclose(servfile);

	// init socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0) perror("socket() error");

	// init address
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(myserver->ip[2]);
	servaddr.sin_port = htons(myserver->port[0]);	

	// connect socket
	connfd = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	if(connfd < 0) perror("connect() error");

	// get file size
	printf("-------- GET FILE SIZE --------\n");
	write(sockfd, argv[3], strlen(argv[3]));
	readerrno = read(sockfd, &flag, 1);
	if(readerrno < 0) perror("READ: file does not exist");
	else
	{
		printf("READ: file exists\n");
		write(sockfd, "0", 1);
		read(sockfd, &response, sizeof(response));
		filesize = ntohl(response);
		printf("FILE: file size - %d\n", filesize);
	}
	close(sockfd);

	pthreadinfo *myprocesses[num];
	pthread_t tid[num];
	int pthreaderrno;
	for(i = 0; i < num; i++)
	{
		// initialize pthread attr
		myprocesses[i] = calloc(3, sizeof(pthreadinfo));
		myprocesses[i]->pid = i;
		myprocesses[i]->filename = argv[3];

		// assign pthread segment attr
		if(i == (num - 1)) // last pid responsible for rest of file
		{
			myprocesses[i]->segment_start = i * (filesize / num);
			myprocesses[i]->segment_size = filesize - ((filesize / num) * (num - 1));
		}
		else
		{
			myprocesses[i]->segment_start = i * (filesize / num);
			myprocesses[i]->segment_size = filesize / num;
		}
		pthreaderrno = pthread_create(&tid[i], NULL, fileTransfer, (void*)myprocesses[i]);
		if(pthreaderrno != 0) { fprintf(stderr, "Unable to create thread %d\n", i); exit(-1);}
	}

	// wait until pthreads finish processing
	for(i = 0; i < num; i++) pthread_join(tid[i], NULL);

	exit(0);
}
