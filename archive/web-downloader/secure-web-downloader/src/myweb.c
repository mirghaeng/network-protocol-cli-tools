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
#include <netinet/in.h>

#include <openssl/ssl.h>

// gcc -lssl -lcrypto -o myweb myweb.c
// ./myweb https://www.example.com:443/index.html
// GET /index.html HTTP/1.1\r\nHost: www.example.com\r\n\r\n
// ./myweb http://www.example.com/index.html -h
// HEAD /index.html HTTP/1.1\r\nost: www.example.com\r\n\r\n

int main(int argc, char** argv) {

	// init
	struct sockaddr_in servaddr;
	int sockfd;
	const SSL_METHOD *method;
	SSL_CTX *ctx;
	SSL *myssl;

// --------------------------------------

	// set pathname
	int SSLmode = 0;
	char* token = strtok(argv[1], "/");
	if(strcmp(token, "https:")==0) {
		token = strtok(NULL, "/");
		SSLmode = 1;
	}
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

	// dns - convert hostname to ip 
	struct hostent *hen = gethostbyname(ip_port[0]);
	if(hen == NULL) {
		perror("ERROR: host not found\n");
		exit(1);
	}

	// set port#	
	int16_t port;
	if(ip_port[1] == NULL)
	{
		port = 80;
	} else {
		port = atoi(ip_port[1]);
	}
// --------------------------------------

	// create socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd == -1) {
		perror("ERROR: failed to create socket");
		exit(1);
	}

	// set address
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);

	// set ip
	if(inet_pton(AF_INET, inet_ntoa(*((struct in_addr*)hen->h_addr_list[0])), &servaddr.sin_addr) <= 0) {
		perror("ERROR: failed to process ip");
		exit(1);
	}

	// connect to the server, TCP/IP layer handshake
	if(connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0) {
		perror("ERROR: failed to connect to socket");
		exit(1);
	}

	// if using SSL
	if(SSLmode == 1) {

		OpenSSL_add_all_algorithms();
		SSL_library_init();
		SSL_load_error_strings();

		// indicates the application is a client
		method = TLSv1_2_client_method();

		// create a new context block
		if(!(ctx = SSL_CTX_new(method))) {
			perror("ERROR: fail to create the context");
			exit(1);
		}	

		// create new ssl object
		if(!(myssl = SSL_new(ctx))) {
			perror("ERROR: fail to create the SSL structure");
			exit(1);
		}

		// bind the socket to the ssl structure
		SSL_set_fd(myssl, sockfd);

		// connect to the server, SSL layer handshake
		int err;
		if((err = SSL_connect(myssl)) < 1) {
			perror("ERROR: SSL handshake not successful");
			exit(1);
		}
	}

// ---------------------------------------------------------------------------------

	while(1)
	{
		char request[1024], response[sizeof(char)+1];
		memset(request, 0, 1024);
		memset(response, 0, sizeof(char)+1);

		// make request
		char* request_format = "%s %c%s HTTP/1.1\r\nHost: %s\r\n\r\n";
		char* type;
		if(argv[2] != NULL && strcmp(argv[2], "-h") == 0)
		{
			type = "HEAD";
		} else {
			type = "GET";
		}
		sprintf(request, request_format, type, '/', pathname, ip_port[0]);

		// send request
		if(SSLmode == 1) {
			SSL_write(myssl, request, strlen(request)+1);
		} else {
			write(sockfd, request, strlen(request)+1);
		}

		char* end_of_header;
		char temp[2048];

		if(strcmp(type, "HEAD") == 0)
		{

			// print header to stdin
			if(SSLmode == 1) { // using ssl
				while(SSL_read(myssl, response, sizeof(char)) > 0)
				{
					fprintf(stdout, "%s", response);
					strcat(temp, response);
					if((end_of_header = strstr(temp, "\r\n\r\n")) != NULL)
					{
						break;
					}
					memset(response, 0, sizeof(char));
				}
			} else { // not using ssl
				while(read(sockfd, response, sizeof(char)) > 0)
				{
					fprintf(stdout, "%s", response);
					strcat(temp, response);
					if((end_of_header = strstr(temp, "\r\n\r\n")) != NULL)
					{
						break;
					}
					memset(response, 0, sizeof(char));
				}
			}
			return(0);
		} else if(strcmp(type, "GET") == 0) {

			// get a string of the header
			if(SSLmode == 1) { // using ssl
				while(SSL_read(myssl, response, sizeof(char)) > 0)
				{
					strcat(temp, response);
					if((end_of_header = strstr(temp, "\r\n\r\n")) != NULL)
					{
						break;
					}
					memset(response, 0, sizeof(char));
				}	
			} else { // not using ssl
				while(read(sockfd, response, sizeof(char)) > 0)
				{
					strcat(temp, response);
					if((end_of_header = strstr(temp, "\r\n\r\n")) != NULL)
					{
						break;
					}
					memset(response, 0, sizeof(char));
				}
			}

			// find content-length
			int length;
			char* content_header_remainder = strstr(temp, "Content-Length: ");
			if(content_header_remainder == NULL)
			{
				perror("ERROR: response does not have Content-Length\n");
				exit(1);
			}
			char* content_header = strchr(content_header_remainder, '\r');
			if(content_header != NULL) *content_header = '\0';
			if(sscanf(content_header_remainder, "Content-Length: %d", &length) == EOF)
			{
				perror("ERROR: unable to scan Content-Length\n");
				exit(1);
			}

			// print byte-by-byte into output.dat
			FILE* fp = fopen("output.dat", "w");
			int n = 0;
			if(SSLmode == 1) {
				while(SSL_read(myssl, response, sizeof(char)) > 0 && n < length)		
				{	
					fprintf(fp, "%s", response);
					n++;
				}
			} else {
				while(read(sockfd, response, sizeof(char)) > 0 && n < length)		
				{	
					fprintf(fp, "%s", response);
					n++;
				}
			}
			fclose(fp);
		}

// -----------------------------------------------------------------------------------		

		// close client
		close(sockfd);
		if(SSLmode == 1) {
			SSL_shutdown(myssl); // "ERROR: failed to shutdown gracefully"
			SSL_free(myssl);
			SSL_CTX_free(ctx);
		}
		exit(0);
	}
}