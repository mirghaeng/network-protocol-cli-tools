#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	struct sockaddr_in servaddr;
	int sockfd;

	// create socket
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
	{
		fprintf(stderr, "ERROR: fail to create socket\n");
		exit(-1);
	}
	
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[2]));

	// set ip
	if(inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0)
	{
		fprintf(stderr, "ERROR: fail to process ip\n");
		exit(-1);
	}

	// tcp handshake
	if(connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0)
	{
		fprintf(stderr, "ERROR: fail to connect to socket\n");
		exit(-1);
	}

	while(1)
	{
		// write command
		char* cmd = readline("client $ ");
		if(strcmp(cmd, "exit") == 0)
		{
			close(sockfd);
			exit(0);
		}
		write(sockfd, cmd, strlen(cmd)+1);
		free(cmd);
		
		// read response
		//char c;
		//int numbytes;
		//int end = 1;

		int len; // get output length to stop recv loop
		char response[8192];
		recv(sockfd, &len, sizeof(int), 0);
		recv(sockfd, response, len, 0);
		printf("%s", response);
/*
		while(end)
		{
			numbytes = recv(sockfd, &c, sizeof(char), 0);
			if(numbytes == 0) end = 0;
			else if(numbytes == -1) end = 0;	
			else printf("%c", c);
		}*/
	}

	exit(0);

}
