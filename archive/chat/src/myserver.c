#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "clientlist.c"

#define MAX_MSG_LENGTH 1024
#define MAX_IP_LENGTH 15
#define MAX_ID_LENGTH 32

List clientList;

void *serverThread(void* arg)
{
	int connfd = (intptr_t) arg;
	char *id = malloc(MAX_ID_LENGTH*sizeof(char));
	
	if(recv(connfd, id, MAX_ID_LENGTH, 0) <= 0)
	{
		perror("Failed to receive client id");
		close(connfd);
	}

	struct sockaddr_in servaddr;
	socklen_t len = sizeof(struct sockaddr_in);
	if(getpeername(connfd, (struct sockaddr*) &servaddr, &len) < 0) {
		perror("Fail to get IP corresponding to specified port");
		exit(-1);
	}
printf("bug here2");
	char command[MAX_MSG_LENGTH];
	while(recv(connfd, command, MAX_MSG_LENGTH, 0) > 0) {
		char* token = strtok(command, " ");
		if(token[0] == '/')
		{
			if(strcmp(token, "/list")==0) {
				printf("bug here\n");
				if(sendClientList(clientList, connfd) < 0) {
					perror("Fail to send list to client");
					exit(-1);
				}
			} else if(strcmp(token, "/wait")==0) {
				char port[MAX_MSG_LENGTH];
				if(recv(connfd, port, MAX_MSG_LENGTH, 0) < 0) {
					perror("Failed to receive new client port #");
					close(connfd);
				}
				char* ip = inet_ntoa(servaddr.sin_addr);
				if(insertToList(clientList, id, ip, atoi(port)) < 0) {
					perror("Fail to send list to client");
					exit(-1);
				}
				free(id);
			} else if(strcmp(token, "/connect")==0) {
				char* connectid = strtok(NULL, " ");
				if(sendClientInfo(clientList, connfd, connectid) < 0) {
					perror("Fail to send list to client");
					exit(-1);
				}
			}
		}
	}
}

int main(int argc, char** argv)
{
	struct sockaddr_in servaddr;
	int listenfd;

	if(argc != 2)
	{
		perror("USAGE: ./myserver <port-number>");
		exit(1);
	}

	// init address
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	servaddr.sin_port = htons(atoi(argv[1]));

	// init socket
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if(listenfd < 0)
	{
		perror("Fail to create new socket");
		exit(1);
	}

	// bind socket
	if(bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
	{
		perror("Fail to bind ip and port");
		exit(1);
	}

	// listen socket
	if(listen(listenfd, 100) < 0)
	{
		perror("Fail to listen for connections on the socket");
		exit(1);
	}

	clientList = newClientList();

	while(1)
	{
		socklen_t len = sizeof(servaddr);
		intptr_t connfd;
		pthread_t tid;

		if((connfd = accept(listenfd, (struct sockaddr *) &servaddr, &len)) < 0)
		{
			perror("Failed to accept socket");
			exit(1);
		}

		printf("connected!\n");
printf("bug here101\n");
		if(pthread_create(&tid, NULL, serverThread, (void*) connfd) != 0)
		{
			perror("Failed to create thread");
			exit(1);
		}

		exit(0);
	}
}
