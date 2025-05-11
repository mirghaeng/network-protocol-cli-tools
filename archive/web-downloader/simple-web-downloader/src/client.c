#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <stdlib.h>

//./myweb www.example.com 93.184.216.34:80/index.html

int main(int argc, char* argv[]) {
	struct sockaddr_in servaddr;
	int sockfd;

	// check if argc == 2 or 3

	// set pathname
	char* token = strtok(argv[2], "/");
	char* pathname = strtok(NULL, "/");

	// get ip & port#
	char* inputs = strtok(token, ":");
	char* ip_port[2] = {NULL, NULL};
	int i = 0;
	while(inputs != NULL && i < 2)
	{
		ip_port[i] = inputs;
		i++;	
		inputs = strtok(NULL, ":");
	}

	// set port#	
	int16_t port;
	if(ip_port[1] == NULL)
	{
		port = 80;
	} else {
		port = atoi(ip_port[1]);
	}

	// create socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0)
	{
		fprintf(stderr, "ERROR: fail to create socket");
		return(-1);
	}

	// setup servaddr
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);

	// set ip
	if(inet_pton(AF_INET, ip_port[0], &servaddr.sin_addr) <= 0)
	{
		fprintf(stderr, "ERROR: fail to process ip");
		return(-1);
	}

//	if(bind(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0)
//	{
//		fprintf(stderr, "ERROR: fail to reserve address to socket");
//		return(-1);
//	}

	// tcp handshake	
	if(connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0)
	{
		fprintf(stderr, "ERROR: fail to connect to socket");
		return(-1);
	}

	while(1)
	{
		char request[1024], response[sizeof(char)+1];
		bzero(request, 1024);
		bzero(response, sizeof(char)+1);

		// make request
		char* request_format = "%s %c%s HTTP/1.1\r\nHost: %s\r\n\r\n";
		char* type;
		if(argv[3] != NULL && strcmp(argv[3], "-h") == 0) // getopt()
		{
			type = "HEAD";
		} else {
			type = "GET";
		}
		sprintf(request, request_format, type, '/', pathname, argv[1]);

		// send request
		write(sockfd, request, strlen(request)+1);

		char* end_of_header;
		char temp[2048];

		if(strcmp(type, "HEAD") == 0)
		{
			while(read(sockfd, response, sizeof(char)) > 0)
			{
				fprintf(stdout, "%s", response);
				strcat(temp, response);
				if((end_of_header = strstr(temp, "\r\n\r\n")) != NULL)
				{
					break;
				}
				bzero(response, sizeof(char));
			}
			return(0);
		} else if(strcmp(type, "GET") == 0) {
			while(read(sockfd, response, sizeof(char)) > 0)
			{
				strcat(temp, response);
				if((end_of_header = strstr(temp, "\r\n\r\n")) != NULL)
				{
					break;
				}
				bzero(response, sizeof(char));
			}	

			int length;
			char* content_header_remainder = strstr(temp, "Content-Length: ");
			if(content_header_remainder == NULL)
			{
				fprintf(stderr, "ERROR: response does not have Content-Length");
			}
			char* content_header = strchr(content_header_remainder, '\r');
			if(content_header != NULL) *content_header = '\0';
			if(sscanf(content_header_remainder, "Content-Length: %d", &length) == EOF)
			{
				fprintf(stderr, "ERROR: unable to scan Content-Length\n");
			}

			FILE* fp = fopen("output.dat", "w");
			int n = 0;
			while(read(sockfd, response, sizeof(char)) > 0 && n < length)		
			{	
				fprintf(fp, "%s", response);
				n++;
			}
			fclose(fp);

			return(0);
		}
	}
}
