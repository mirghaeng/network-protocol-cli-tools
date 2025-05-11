// myserver.c
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void dg_echo(int sockfd, struct sockaddr* pcliaddr, socklen_t clilen)
{
	int n;
	socklen_t len;
	char msg[10000];

	while(1)
	{
		len = clilen;
		n = recvfrom(sockfd, msg, 100000, 0, pcliaddr, &len);
		//sendto(sockfd, msg, n, 0, pcliaddr, len);
		sendto(sockfd, "got message", strlen("got message"), 0, pcliaddr, len);
	}
}

int main(int argc, char* argv[])
{
	struct sockaddr_in servaddr, cliaddr;
	int sockfd, n;

	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		printf("can't create a socket\n");
		exit(-1);	
	}

	bzero(&servaddr, sizeof(servaddr));
	bzero(&cliaddr, sizeof(cliaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(5555);
	
	if((bind(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr))) == -1)
	{
		printf("can't bind socket and port\n");
		exit(-1);
	}

	dg_echo(sockfd, (struct sockaddr*) &cliaddr, sizeof(cliaddr));
}
