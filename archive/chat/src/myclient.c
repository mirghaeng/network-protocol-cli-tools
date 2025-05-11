#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <readline/readline.h>
#include <signal.h>

#define MAX_MSG_LENGTH 1024
#define MAX_IP_LENGTH 15
#define MAX_ID_LENGTH 32

typedef enum {
	INFO,
	WAIT,
	CHAT
} Mode;

typedef struct {
	int infosockfd;
	intptr_t waitsockfd;
	int chatsockfd;
} SocketMode;

typedef struct {
	char myClient[MAX_ID_LENGTH];
	char requestClient[MAX_ID_LENGTH];
} ClientID;

Mode clientMode;
SocketMode socketmode;
ClientID *id;

void handler(int sig) {
	if(clientMode == WAIT) {
		shutdown(socketmode.waitsockfd, SHUT_RDWR);
		printf("Stopped waiting\n");
	} else if(clientMode == CHAT) {
		shutdown(socketmode.chatsockfd, SHUT_RDWR);
		printf("Left conversation with %s.\n", id->requestClient);
	}

	clientMode = INFO;
}

int getClientList(int sockfd) {

	// send "/list" cmd to server
	char cmd[6] = "/list";
	if(send(sockfd, cmd, 5*sizeof(char), 0) < 0){
		perror("Send cmd error");
		return(-1);
	}

	// receive client list and print to stdin
	char buffer[MAX_ID_LENGTH];
	for(int i = 0; recv(sockfd, buffer, MAX_ID_LENGTH, 0) > 0; i++) {
		if(buffer[0] == '\n') {
			break;
		}
		printf("%d) %s\n", i, buffer);
		memset(buffer, 0, MAX_ID_LENGTH);
	}

	// success
	return(1);
}

void *chatThread(void *arg) {

	char requestID[MAX_ID_LENGTH];
	if(recv(socketmode.chatsockfd, requestID, MAX_ID_LENGTH, 0) < 0) {
		perror("Fail to receive request client");
		pthread_exit((void*)-1);
	}

	printf("Connection from %s.\n", requestID);
	printf("%s> \n", id->myClient);

	char response[MAX_MSG_LENGTH];
	while(recv(socketmode.chatsockfd, response, MAX_MSG_LENGTH, 0) > 0) {
		printf("%s: %s\n", requestID, response);
		printf("%s> \n", id->myClient);
	}
	printf("Left conversation with %s.\n", requestID);
	printf("%s> \n", id->myClient);

	pthread_exit((void*)1);
}

void *waitThread(void *arg) {
	int connfd = (intptr_t) arg;

	// set timeout
	struct timeval timeout;
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;

	// init file descriptor set
	fd_set set;

	while(1) {
		FD_ZERO(&set);
		FD_SET(connfd, &set);
		if(clientMode == INFO) {
			char buffer[MAX_MSG_LENGTH] = "/remove";
			if(send(socketmode.infosockfd, buffer, MAX_MSG_LENGTH, 0) < 0) {
				perror("Fail to send /remove command");
			}
			fflush(stdout);
			break;
		}
		int retval = select(socketmode.waitsockfd + 1, &set, NULL, NULL, &timeout);
		if(retval < 0) {
			perror("Select() error");
			pthread_exit((void*)-1);
		} else if(retval == 0) {
			sleep(1);
			continue;
		} else if(retval > 0) {
			FD_CLR(socketmode.waitsockfd, &set);
			int connfd = accept(socketmode.waitsockfd, NULL, NULL);
			pthread_t tid;
			socketmode.chatsockfd = connfd;
			if(pthread_create(&tid, NULL, chatThread, (void*) &socketmode.chatsockfd) < 0) {
				perror("Fail to create thread");
			}			

			if(send(socketmode.chatsockfd, id->myClient, MAX_ID_LENGTH, 0) < 0) {
				perror("Fail to exchange ID");
			}
			break;
		}
	}
	pthread_exit((void*)1);
}

int joinWaitList(int servsockfd) {

	// create new port
	struct sockaddr_in servaddr;
	intptr_t sockfd;

	// init socket address
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = 0;

	// init socket
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 1) {
		perror("Fail to create new socket");
		return(-1);
	}

	// bind socket and ip pair
	if(bind(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0) {
		perror("Fail to bind socket");
		return(-1);
	}

	// listen to socket
	if(listen(sockfd, 100) < 0) {
		perror("Fail to listen to connections on socket");
		return(-1);
	}

	// get random port #
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	if(getsockname(sockfd, (struct sockaddr*) &addr, &len) < 0) {
		return(-1);
	}
	int newsockfd = ntohs(addr.sin_port);

	// send new port #
	char buffer[MAX_MSG_LENGTH];
	sprintf(buffer, "%d", newsockfd);
	if(send(servsockfd, buffer, MAX_MSG_LENGTH, 0) < 0) {
		perror("Fail to send msg\n");
		return(-1);
	}

	// set modes
	clientMode = WAIT;
	socketmode.waitsockfd = sockfd;

	// make new thread for WAIT
	pthread_t tid;
	if(pthread_create(&tid, NULL, waitThread, (void*) &(socketmode.waitsockfd)) < 0) {
		perror("Fail to create thread\n");
		return(-1);
	}
}

int connectClient(int servsockfd, char* connectToID) {
	char command[MAX_ID_LENGTH+9];
	sprintf(command, "/connect %s", connectToID);
	if(send(servsockfd, command, MAX_ID_LENGTH+9, 0) < 0) {
		perror("Fail to send ID to connect to");
		return(-1);
	}
	char clientinfo[MAX_MSG_LENGTH];
	char ip[MAX_IP_LENGTH];
	int port;
	if(recv(servsockfd, clientinfo, MAX_MSG_LENGTH, 0) < 0) {
		if(strcmp(clientinfo, "DNE")==0) {
			printf("ID not in list\n");
			return(1);
		} else {
			if(sscanf(clientinfo, "%s %d", ip, &port)==EOF) {
				return(-1);
			}
		}
	}

	struct sockaddr_in servaddr;
	int sockfd;

	// init socket
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Fail to create new socket");
		return(-1);
	}

	// init socket address
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);

	// convert IP to network standard
	if(inet_pton(AF_INET, ip, &servaddr.sin_addr) <= 0) {
		perror("Invalid address");
		return(-1);
	}

	// connect
	if(connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0) {
		perror("Connection fail");
		return(-1);
	}

	// set modes
	clientMode = CHAT;
	socketmode.chatsockfd = sockfd;

	// create new thread for CHAT
	pthread_t tid;
	if(pthread_create(&tid, NULL, chatThread, (void*) &(socketmode.chatsockfd)) < 0) {
		perror("Fail to create thread");
		return(-1);
	}

	if(send(sockfd, id->myClient, MAX_ID_LENGTH, 0) < 0) {
		perror("Fail to send id");
		return(-1);
	}
}

int main(int argc, char** argv) {

	// handle CTRL+C
	signal(SIGINT, handler);

	struct sockaddr_in servaddr;
	int sockfd;

	// check args
	if(argc != 4) {
		perror("USAGE: ./myclient <IP-Address> <Port-Number> <Client-ID>");
		exit(1);
	}

	// init socket
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Fail to create new socket");
		exit(1);
	}

	// init socket address
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[2]));

	// convert IP to network standard
	if(inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0) {
		perror("Invalid address");
		exit(1);
	}

	// connect
	if(connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0) {
		perror("Connection fail");
		exit(1);
	}

	// send client ID
	char buffer[MAX_ID_LENGTH];
	strcpy(buffer, argv[3]);
	if((send(sockfd, buffer, MAX_ID_LENGTH, 0)) < 0) {
		perror("Send msg error");
		exit(1);
	}

	// set modes
	clientMode = INFO;
	socketmode.infosockfd = sockfd;

	// prompt user cmd and execute cmd
	char prompt[MAX_ID_LENGTH+2];
	sprintf(prompt, "%s> ", argv[3]);
	char* cmd = readline(prompt);
	char* token = strtok(cmd, " ");
	if(cmd[0] == '/') {
		if(strcmp(token, "/quit")==0) {
			close(sockfd);
		} else if(strcmp(token, "/list")==0) {
			if(getClientList(sockfd) < 0) {
				perror("Fail to get client list");
			}
		} else if(strcmp(token, "/wait")==0) {
			if(joinWaitList(sockfd) < 0) {
				perror("Fail to join wait list");
			}
		} else if(strcmp(token, "/connect")==0) {
			char* connectToID = strtok(NULL, " ");
			if(connectClient(sockfd, connectToID) < 0) {

			}
		}

	}

}
