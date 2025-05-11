// myclient.c
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

void dg_cli(FILE* fp, int sockfd, const struct sockaddr *pservaddr, socklen_t servlen)
{
	int n;
	char sendline[100000], recvline[100000+1];
	while(fgets(sendline, 100000, sockfd) != NULL)
	{
		sendto(sockfd, sendline, strlen(sendline), 0, pservaddr, servlen);
		n = recvfrom(sockfd, recvline, 100000, 0, NULL, NULL);
		recvline[n] = 0;
		fputs(recvline, stdout);
	}	
}

// ./myclient <server-info.txt> <num-chunks> <filename>
int main(int argc, char* argv[])
{
	struct sockaddr_in servaddr;
	int sockfd;
	
	//if(argc != 2)
	//{
	//	printf("usage: udpcli <IPaddress>");
	//	exit(-1);
	//}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(5555);

	if(inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr) <= 0)
	{
		printf("fail to process ip\n");
		exit(-1);
	}

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	dg_cli(stdin, sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr));

	exit(0);
}
