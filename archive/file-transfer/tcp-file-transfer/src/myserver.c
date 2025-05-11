#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>

int main(int argc, char* argv[])
{
	struct sockaddr_in servaddr;
	int listenfd, binderrno, listenerrno;

	// check command arguments
	if(argc != 2)
	{
		perror("USAGE: ./myserver <port-number>");
		exit(-1);
	}

	// init socket
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if(listenfd < 0) { perror("socket() error"); exit(-1); }

	// init address
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(atoi(argv[1]));

	// socket bind
	binderrno = bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	if(binderrno < 0) { perror("bind() error"); exit(-1); }

	// socket listen
	listenerrno = listen(listenfd, 10);
	if(listenerrno < 0) { perror("listen() error"); exit(-1); }

	char flag;
	uint32_t response;
	uint32_t filesize;
	int format_size;
	uint32_t offset;
	uint32_t segment_size;
	int current_offset;
	char buff[2048];
	int senderrno;
	while(1)
	{
		// socket accept
		int servaddr_length = sizeof(servaddr);
		int newlistenfd = accept(listenfd, (struct sockaddr*)&servaddr, (socklen_t*)&servaddr_length);
		if(newlistenfd < 0) { perror("accept() error"); exit(-1); }

		// read filename
		char filename[300];
		int readerrno = read(newlistenfd, filename, 300);
		if(readerrno < 0) { perror("fail to read filename"); exit(-1); } 

		// open file
		FILE* fp = fopen(filename, "rb");
		if(fp == NULL)	// file DNE
		{
			// notify client about bad file
			write(newlistenfd, "1", 1);
			fprintf(stderr, "fail to open %s\n", filename);
			exit(-1); // <-------------------------------------------- do this for now
		} else //file exists
		{
			// notify client about successful file
			write(newlistenfd, "0", 1);
			printf("----- FILE '%s' OPEN SUCCESS -----\n", filename);

			// read client request about the file
			readerrno = read(newlistenfd, &flag, 1);
			if(readerrno < 0) { fprintf(stderr, "fail to read client request for %s", filename); exit(-1); }

			if(strncmp(&flag, "1", 1) == 0)	// client 
			{
				printf("AGENDA: client download file\n");

				// read starting byte of segment
				readerrno = read(newlistenfd, &response, sizeof(response));
				if(readerrno < 0) { perror("fail to read starting byte"); exit(-1); }
				offset = ntohl(response);
				printf("starting byte: %d\n", offset);			

				// read byte size of segment
				readerrno = read(newlistenfd, &response, sizeof(response));
				if(readerrno < 0) { perror("fail to read byte size"); exit(-1); }
				segment_size = ntohl(response);
				printf("byte size: %d\n", segment_size);

				// send segment chunk
				fseek(fp, offset, SEEK_SET);
				readerrno = fread(buff, sizeof(char), segment_size, fp);
				if(readerrno < 0) { perror("given byte size too small"); exit(-1); }
				senderrno = send(newlistenfd, buff, readerrno, 0);
				if(readerrno < 0) { perror("send() error"); exit(-1); }
				printf("SENT: %s\n", buff);
				printf("DONE: server sent chunk to client\n");
			}
			else if(strncmp(&flag, "0", 1) == 0)
			{
				printf("AGENDA: client get file size\n");
				current_offset = ftell(fp);
				fseek(fp, 0, SEEK_END);
				filesize = ftell(fp);
				fseek(fp, current_offset, SEEK_SET);
				format_size = htonl(filesize);
				write(newlistenfd, &format_size, sizeof(format_size));
				printf("FILE: file size - %d\n", format_size);
				printf("DONE: server sent file size\n");
			}
			fclose(fp);
		}
		close(newlistenfd);
	}
	close(listenfd);
	exit(0);
}

