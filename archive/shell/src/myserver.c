#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
//#include <readline/readline.h>
//#include <readline/history.h>
#include <unistd.h>


int main(int argc, char** argv)
{
	int listenfd;
	struct sockaddr_in listenaddr;

	// initialize socket
	if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		fprintf(stderr, "Fail to create new socket\n");
		exit(-1);
	}
	
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0)
    		printf("setsockopt(SO_REUSEADDR) failed");
	
	// initialize address
	memset(&listenaddr, 0, sizeof(listenaddr));
	listenaddr.sin_family = AF_INET;
	listenaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	listenaddr.sin_port = htons(atoi(argv[1]));

	printf("Listening for connections on port %d\n", ntohs(listenaddr.sin_port));
	printf("Listening for connections on ip %d\n", ntohs(listenaddr.sin_addr.s_addr));

	// socket bind
	if(bind(listenfd, (struct sockaddr*) &listenaddr, sizeof(listenaddr)) < 0)
	{
		fprintf(stderr, "Fail to bind ip and socket\n");
		exit(-1);
	}

	// listen socket
	if(listen(listenfd, 100) < 0)
	{
		fprintf(stderr, "fail to listen for connections on the socket\n");
		exit(-1);
	}
	socklen_t addrlen = sizeof(listenaddr);
	getsockname(listenfd, (struct sockaddr*) &listenaddr, &addrlen);
	
	while(1)
	{
		int connfd = accept(listenfd, (struct sockaddr*) &listenaddr, &addrlen);
		printf("New connection: %d\n", connfd);
		if(fork() == 0)
		{
			pid_t pid = getpid();
			printf("%d: forked\n\n", pid);
			char buf[100];
			while(1)
			{
				ssize_t recvbytes = recv(connfd, buf, sizeof(buf), 0);
				if(recvbytes == 0)
				{
					printf("%d: received end-of-connection, thus closing connection and exiting\n", pid);	
					shutdown(connfd, SHUT_WR);
					close(connfd);
					exit(0);
				}
				else if(strcmp(buf, "exit") == 0)
				{
					printf("exit\n");
					shutdown(connfd, SHUT_WR);
					close(connfd);
					exit(0);
				}
				else
				{
					printf("client $ %s\n", buf);
					FILE* fp = popen(buf, "r");
					char c;
					int i = 0;
					char response[8192];
					while(fread(&c, sizeof(char), 1, fp) > 0)
					{
						strcat(response, &c);
						i++;
						//send(connfd, &c, sizeof(char), 0);
					}
					printf("%s", response);
					send(connfd, response, sizeof(response), 0);
					fclose(fp);
				}
			}
		} else {
			close(connfd);
		}
		
	}

	exit(0);

}
