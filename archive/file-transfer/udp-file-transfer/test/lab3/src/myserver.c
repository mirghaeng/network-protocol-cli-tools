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
#include "packet.h"

void sendFileSize(int listenfd, packetinfo *pkt_in, struct sockaddr_in* cliaddr, socklen_t* clilen)
{
	struct stat st;
	packetinfo *pkt_out;

	printf("PKT_IN: FILESIZE REQUEST\n");

	if(stat(pkt_in->filename, &st) != 0) {
		printf("ERROR: error pkt\n");
		exit(-1);
	} else {
		pkt_out = revised_pkt(respond_filesize, st.st_size, 0, pkt_in->filename, NULL);
		sendto(listenfd, pkt_out, sizeof(packetinfo), 0, (struct sockaddr*) cliaddr, sizeof(*cliaddr));
	}
	free(pkt_out);
}

void sendChunkContent(int listenfd, packetinfo* pkt_in, struct sockaddr_in* cliaddr, socklen_t* clilen)
{
	struct stat st;
	FILE* fp;
	packetinfo *pkt_out;

	printf("FILENAME: %s.\n", pkt_in->filename);
	fp = fopen(pkt_in->filename, "r+");
	
	if(fp == NULL)
	{
		printf("ERROR: can't open file\n");
		exit(-1);
	} else {
		fseek(fp, pkt_in->start_byte, SEEK_SET);
		pkt_out = revised_pkt(respond_chunkcontent, pkt_in->byte_size, pkt_in->start_byte, pkt_in->filename, NULL);
		fgets(pkt_out->content, pkt_in->byte_size, fp);
		fclose(fp);
	}
	sendto(listenfd, pkt_out, sizeof(packetinfo) + pkt_out->byte_size, 0, (struct sockaddr*) cliaddr, sizeof(*cliaddr));
	free(pkt_out);
}

int main(int argc, char** argv)
{
	int listenfd;
	long port;
	socklen_t clilen;
	struct sockaddr_in servaddr, cliaddr;
	packetinfo *pkt_recv;

	if(argc != 2)
	{
		printf("Usage: ./myserver <port-number>\n");
		exit(1);
	}

	port = strtol(argv[1], NULL, 10);

	listenfd = socket(AF_INET, SOCK_DGRAM, 0);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr));
	packetinfo *pkt_in = empty_pkt();
	
	while(1)
	{
		clilen = sizeof(cliaddr);
		int n = recvfrom(listenfd, pkt_in, sizeof(packetinfo), 0, (struct sockaddr*) &cliaddr, &clilen);
		if(n > 0)
		{
			if(pkt_in->command == request_filesize)
			{
				printf("SEND FILE SIZE\n");
				sendFileSize(listenfd, pkt_in, &cliaddr, &clilen);
			} else if(pkt_in->command == request_chunkcontent)
			{
				printf("first time printing filename: %s\n", pkt_in->filename);
				sendChunkContent(listenfd, pkt_in, &cliaddr, &clilen);
			}
		}
		free(pkt_in);
	}
}
