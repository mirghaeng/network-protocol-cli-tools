#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define BUFFER_SIZE 1024

void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <url> [-h]\nExample: %s https://www.example.com:443/index.html -h\n", prog_name, prog_name);
    exit(EXIT_FAILURE);
}
int parse_url(const char *url, char **scheme, char **ip, int *port, char **path) {
    *scheme = strdup("https");  // Default to https
    *port = 443;  // Default to port 443

    const char *url_start = url;
    if (strncmp(url, "http://", 7) == 0) {
        *scheme = strdup("http");
        *port = 80;
        url_start += 7;
    } else if (strncmp(url, "https://", 8) == 0) {
        *scheme = strdup("https");
        *port = 443;
        url_start += 8;
    } else {
        
    }

    char *slash = strchr(url_start, '/');
    if (!slash) {
        fprintf(stderr, "Invalid URL format. Path must be specified.\n");
        return -1;
    }

    *path = strdup(slash + 1);

    char *colon = strchr(url_start, ':');
    if (colon && colon < slash) {
        *colon = '\0';
        *ip = strdup(url_start);
        *port = atoi(colon + 1);
        if (*port <= 0) {
            fprintf(stderr, "Invalid port number.\n");
            free(*scheme);
            free(*path);
            return -1;
        }
    } else {
        *ip = strndup(url_start, slash - url_start);
    }

    // Adjust scheme based on port if not explicitly provided
    if (*port == 443 && strcmp(*scheme, "http") == 0) {
        fprintf(stderr, "Error: Port 443 is reserved for HTTPS. Use the correct scheme.\n");
        free(*scheme);
        free(*ip);
        free(*path);
        return -1;
    } else if (*port == 80 && strcmp(*scheme, "https") == 0) {
        fprintf(stderr, "Error: Port 80 is reserved for HTTP. Use the correct scheme.\n");
        free(*scheme);
        free(*ip);
        free(*path);
        return -1;
    }

    return 0;
}
void initialize_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}
void cleanup_openssl() {
    EVP_cleanup();
}
SSL_CTX *create_ssl_context() {
    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    // Use default CA bundle
    if (!SSL_CTX_set_default_verify_paths(ctx)) {
        fprintf(stderr, "Failed to use default verify paths.\n");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}
void add_missing_certificate(SSL_CTX *ctx, STACK_OF(X509) *chain) {
    if (chain == NULL) return;

    for (int i = 1; i < sk_X509_num(chain); i++) { // Skip server cert (index 0)
        FILE *fp = fopen("missing-cert.pem", "w");
        if (fp == NULL) {
            perror("Failed to open file for writing missing certificate.");
            continue;
        }
        PEM_write_X509(fp, sk_X509_value(chain, i));
        fclose(fp);

        if (!SSL_CTX_load_verify_locations(ctx, "missing-cert.pem", NULL)) {
            fprintf(stderr, "Failed to load missing certificate.\n");
            ERR_print_errors_fp(stderr);
            continue;
        }
        printf("Added missing certificate to the chain.\n");
    }
}
int validate_certificate(const char *ip, SSL_CTX *ctx, SSL *ssl) {
    X509 *cert = SSL_get_peer_certificate(ssl);
    if (!cert) {
        fprintf(stderr, "No certificate provided by the server.\n");
        return EXIT_FAILURE;
    }
    X509_free(cert);

    // Retry verification
    long verify_result = SSL_get_verify_result(ssl);
    if (verify_result != X509_V_OK) {
        fprintf(stderr, "Initial verification failed: %s\n",
                X509_verify_cert_error_string(verify_result));

        // Retrieve the certificate chain and add missing certificates
        STACK_OF(X509) *chain = SSL_get_peer_cert_chain(ssl);
        add_missing_certificate(ctx, chain);

        // Retry verification
        verify_result = SSL_get_verify_result(ssl);
        if (verify_result != X509_V_OK) {
            fprintf(stderr, "Retry verification failed: %s\n",
                    X509_verify_cert_error_string(verify_result));
            SSL_CTX_free(ctx);
            return EXIT_FAILURE;
        }
    }
}
int fetch_via_https(const char *ip, const char *request, int sock, int header_only) {
    SSL_CTX *ctx;
    SSL *ssl;
    initialize_openssl();
    ctx = create_ssl_context();
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);  // Link SSL to the socket

    // server sends its certificate (and any intermediate certificates in the chain)
    // openssl stores this certificate for use during verification
    if (SSL_connect(ssl) <= 0) {
        fprintf(stderr, "SSL handshake failed\n");
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        return EXIT_FAILURE;
    }

    if (validate_certificate(ip, ctx, ssl) == -1) {
        fprintf(stderr, "Certification validation failed\n");
        ERR_print_errors_fp(stderr);
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        return EXIT_FAILURE;
    }

    if (SSL_write(ssl, request, strlen(request)) <= 0) {
        fprintf(stderr, "SSL write failed\n");
        ERR_print_errors_fp(stderr);
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        return EXIT_FAILURE;
    }

    // Open file for binary data if not header-only
    FILE *output_file = NULL;
    if (!header_only) {
        output_file = fopen("output.dat", "wb");
        if (!output_file) {
            perror("fopen");
            return EXIT_FAILURE;
        }
    }

    // Receive response and print only headers if -h is specified
    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);
    ssize_t bytes_received;
    int header_done = 0;

    while ((bytes_received = SSL_read(ssl, buffer, sizeof(buffer))) > 0) {
        if (header_only) {
            for (size_t i = 0; i < bytes_received; i++) {
                putchar(buffer[i]);
                if (i > 3 && buffer[i] == '\n' && buffer[i - 1] == '\r' && buffer[i - 2] == '\n' && buffer[i - 3] == '\r') {
                    header_done = 1;
                    break;
                }
            }
            if (header_done) break;
        } else {
            size_t elements_written = fwrite(buffer, 1, bytes_received, output_file);
            fflush(output_file); // Ensure that data written to the file is flushed to disk
        }
    }

    if (output_file) fclose(output_file);
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    cleanup_openssl();
}
int fetch_via_http(const char *request, int sock, int header_only) {
    if (send(sock, request, strlen(request), 0) < 0) {
        perror("send");
        return EXIT_FAILURE;
    }

    // Open file for binary data if not header-only
    FILE *output_file = NULL;
    if (!header_only) {
        output_file = fopen("output.dat", "wb");
        if (!output_file) {
            perror("fopen");
            return EXIT_FAILURE;
        }
    }

    // Receive response and print only headers if -h is specified
    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);
    ssize_t bytes_received;
    int header_done = 0;

    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        if (header_only) {
            for (size_t i = 0; i < bytes_received; i++) {
                putchar(buffer[i]);
                if (i > 3 && buffer[i] == '\n' && buffer[i - 1] == '\r' && buffer[i - 2] == '\n' && buffer[i - 3] == '\r') {
                    header_done = 1;
                    break;
                }
            }
            if (header_done) break;
        } else {
            size_t elements_written = fwrite(buffer, 1, bytes_received, output_file);
            fflush(output_file); // Ensure that data written to the file is flushed to disk
        }
        bzero(buffer, BUFFER_SIZE);
    }
    if (bytes_received < 0) perror("recv");

    if (output_file) fclose(output_file);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
    }

    const char *url = argv[1];
    int header_only = 0;
    if (argc == 3 && strcmp(argv[2], "-h") == 0) {
        header_only = 1;
    }

    char *scheme = NULL;
    char *ip = NULL;
    int port = 0;
    char *path = NULL;
    if (parse_url(url, &scheme, &ip, &port, &path) < 0) {
        return EXIT_FAILURE;
    }

    // Resolve hostname using getaddrinfo
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char port_buffer[6];
    snprintf(port_buffer, sizeof(port_buffer), "%d", port);
    if (getaddrinfo(ip, port_buffer, &hints, &res) != 0) {
        fprintf(stderr, "Failed to resolve hostname: %s\n", ip);
        free(scheme);
        free(ip);
        free(path);
        return EXIT_FAILURE;
    }

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        free(scheme);
        free(ip);
        free(path);
        return EXIT_FAILURE;
    }

    // Connect to server
    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        freeaddrinfo(res);
        free(scheme);
        free(ip);
        free(path);
        close(sock);
        return EXIT_FAILURE;
    }
    freeaddrinfo(res);

    // Build HTTP request
    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request),
        "%s /%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
        header_only ? "HEAD" : "GET", path, ip);
    // Add the Connection: close header to the HTTP request. This tells the server to close the connection after the response is sent.

    // Send request
    if (strcmp(scheme, "https") == 0) {
        if (fetch_via_https(ip, request, sock, header_only) == -1) {
            free(scheme);
            free(ip);
            free(path);
            close(sock);
            return EXIT_FAILURE;
        }
    } else {
        if (fetch_via_http(request, sock, header_only) == -1) {
            free(scheme);
            free(ip);
            free(path);
            close(sock);
            return EXIT_FAILURE;
        }
    }

    free(scheme);
    free(ip);
    free(path);
    close(sock);
    return EXIT_SUCCESS;
}
