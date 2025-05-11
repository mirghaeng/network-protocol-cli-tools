#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include "packet.h"
#define TIMEOUT 5

typedef struct {
	char **ip;
	int *port;	
} serverinfo;

typedef struct {
	char* filename;
	int segment_start;
	int segment_size;
	int pid;
	int filesize;
	int num;
} threadinfo;

serverinfo* myserver;
threadinfo* myprocess;

static void interrupt(int signal)
{
	return;
}

int getServerInfo(char** argv)
{
	FILE* servfile = fopen(argv[1], "r");
	char line[100];
	int num = atoi(argv[2]);
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
	return num;
}

int getFileSize(char** argv, int num)
{
	struct sigaction sa = {
		.sa_handler = interrupt,
		.sa_flags = 0
	};

	struct sockaddr_in servaddr;
	int sockfd;
	int num_retransmissions = 10;

	for(int i = 0; i < num; i++)
	{
		// init socket
		sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		if(sockfd < 0) perror("socket() error");

		// init address
		memset(&servaddr, 0, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = inet_addr(myserver->ip[i]);
		servaddr.sin_port = htons(myserver->port[i]);

		for(int j = 0; j < num_retransmissions; j++)
		{
			// create pkts
			packetinfo *pkt_in, *pkt_out;
			pkt_out = revised_pkt(request_filesize, 0, 0, argv[3], NULL);
			pkt_in = empty_pkt();

			// send pkt
			sendto(sockfd, pkt_out, sizeof(packetinfo), 0, (struct sockaddr *) &servaddr, sizeof(servaddr));
			printf("CLIENT REQUEST FOR FILE SIZE\n");
	
			// wait for timeout
			alarm(TIMEOUT);

			// get file size
			int len = sizeof(servaddr);
			int n = recvfrom(sockfd, pkt_in, sizeof(packetinfo) + MAX_CONTENT_SIZE, 0, (struct sockaddr *) &servaddr, &len);
			free(pkt_out);

			// check
			if(pkt_in->command == respond_filesize)
			{
				alarm(0);
				return pkt_in->byte_size;
			}
		}
	}
	return(-1);
}

void* fileTransfer(void* myprocess)
{
	threadinfo* thread = (threadinfo*) myprocess;
	char* content = calloc(thread->filesize, 1);

	struct sigaction sa = {
		.sa_handler = interrupt,
		.sa_flags = 0
	};

	struct sockaddr_in servaddr;
	int sockfd;
	int num_retransmissions = 10;

	for(int i = thread->pid; i < thread->num; i++)
	{
		// init socket
		sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		if(sockfd < 0) perror("socket() error");

		// init address
		memset(&servaddr, 0, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = inet_addr(myserver->ip[i]);
		servaddr.sin_port = htons(myserver->port[i]);

printf("error checking here!!!!\n");

		for(int j = 0; j < num_retransmissions; j++)
		{
			// create pkts
			packetinfo *pkt_in, *pkt_out;
			pkt_out = revised_pkt(request_chunkcontent, thread->segment_size, thread->segment_start, thread->filename, NULL);
			pkt_in = empty_pkt();

			// send pkt
			sendto(sockfd, pkt_out, sizeof(packetinfo), 0, (struct sockaddr *) &servaddr, sizeof(servaddr));
			printf("CLIENT REQUEST FOR FILE SIZE\n");
	
			// wait for timeout
			alarm(TIMEOUT);

			// get chunk content
			int len = sizeof(servaddr);
			int n = recvfrom(sockfd, pkt_in, sizeof(packetinfo) + MAX_CONTENT_SIZE, 0, (struct sockaddr *) &servaddr, &len);
			free(pkt_out);

			// check
			if(pkt_in->command == respond_chunkcontent)
			{
				alarm(0);
				memcpy(content, &(pkt_in->content), pkt_in->byte_size);
			}
		}
	}
	pthread_exit((void*)content);
}

int main(int argc, char** argv)
{
	if(argc != 4)
	{
		printf("Usage: ./myclient <server-info.text> <num-connections> <filename>");
		exit(1);
	}

	int num = getServerInfo(argv);
	int filesize = getFileSize(argv, num);
	printf("FILE SIZE: %d\n", filesize);

	
	pthread_t tid[num];
	threadinfo *myprocess[num];
	int pthreaderrno;
	for(int i = 0; i < num; i++)
	{
		
		// initialize pthread attr
		myprocess[i] = calloc(5, sizeof(threadinfo));
		myprocess[i]->pid = i;
		myprocess[i]->filename = argv[3];
		myprocess[i]->filesize = filesize;	
		myprocess[i]->num = num;
		
		// assign pthread segment attr
		if(i == (num - 1))
		{
			myprocess[i]->segment_start = i * (filesize/num);
			myprocess[i]->segment_size = filesize - ((filesize / num) * (num - 1));
		} else {
			myprocess[i]->segment_start = i * (filesize / num);
			myprocess[i]->segment_size = filesize / num;
		}
		pthreaderrno = pthread_create(&tid[i], NULL, fileTransfer, (void*) myprocess[i]);
		if(pthreaderrno != 0) {printf("Unable to create thread\n"); exit(-1); }
	}

	void* thread_chunk[num];
	char* chunks;

	// wait until pthreads finish processing
	for(int i = 0; i < num; i++)
	{
		pthread_join(tid[i], &thread_chunk[i]);	
	}

	// print threads to file
	char* output = calloc(strlen(argv[3]), sizeof(char));
	strcpy(output, argv[3]);

	FILE* fp = fopen(output, "w+");
	if(fp == NULL)
	{
		printf("unable to open output file\n");
		exit(-1);
	} else {
		for(int i = 0; i < num; i++)
		{
			chunks = (char*)thread_chunk[i];
			fputs(chunks, fp);
		}
	}
	
	for(int a = 0; a < num; a++)
	{	
		free(myprocess[a]);
	}
	fclose(fp);
}
