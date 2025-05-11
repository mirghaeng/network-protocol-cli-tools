// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1048576 // 1MB

void handle_client(int client_socket)
{
    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);

    // Receive the request
    size_t bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0 || bytes_read >= BUFFER_SIZE) {
        perror("recv failed or buffer overflow\n");
        close(client_socket);
        return;
    }
    buffer[bytes_read] = '\0';

    // Parse the request (format: CHECK <filename> or GET <filename> <offset> <chunk_size>)
    char command[10], filename[256];
    size_t offset, chunk_size;
    int params = sscanf(buffer, "%9s %255s %zu %zu", command, filename, &offset, &chunk_size);
    bzero(buffer, BUFFER_SIZE);

    if (strcmp(command, "CHECK") == 0) {
        if (params < 2) {
            fprintf(stderr, "Invalid CHECK request: params=%d\n", params);
            write(client_socket, buffer, strlen(buffer));
            close(client_socket);
            return;
        }
    } else if (strcmp(command, "GET") == 0) {
        if (params < 4 || chunk_size <= 0) {
            fprintf(stderr, "Invalid GET request: params=%d, chunk_size=%zu\n", params, chunk_size);
            write(client_socket, buffer, strlen(buffer));
            close(client_socket);
            return;
        }
    } else {
        fprintf(stderr, "Unknown command: %s\n", command);
        write(client_socket, buffer, strlen(buffer));
        close(client_socket);
        return;
    }

    fprintf(stderr, "1) passed received request check\n");

    FILE *file = fopen(filename, "rb");
    if (!file) {
        snprintf(buffer, BUFFER_SIZE, "ERROR File not found");
        write(client_socket, buffer, strlen(buffer));
        close(client_socket);
        return;
    }

    fprintf(stderr, "2) passed opening file\n");

    if (strcmp(command, "CHECK") == 0) {
        fprintf(stderr, "CHECK request: processing...\n");

        // Check if file exists and get file size
        FILE *file = fopen(filename, "rb");
        if (file) {
            fseek(file, 0, SEEK_END);
            size_t file_size = ftell(file);
            fclose(file);

            snprintf(buffer, BUFFER_SIZE, "OK %zu", file_size);
            write(client_socket, buffer, strlen(buffer));

            fprintf(stderr, "CHECK request: OK %zu\n", file_size);
        } else {
            snprintf(buffer, BUFFER_SIZE, "ERROR File not found");
            fprintf(stderr, "ERROR File not found");
        }
    } else if (strcmp(command, "GET") == 0) {
        fprintf(stderr, "GET request: processing...\n");

        // Check if file exists and get size
        FILE *file = fopen(filename, "rb");
        if (file) {
            fseek(file, 0, SEEK_END);
            size_t file_size = ftell(file);
            rewind(file);

            if (offset >= file_size || offset + chunk_size > file_size) {
                fprintf(stderr, "Invalid chunk_size: %zu, offset: %zu, file size: %zu\n", chunk_size, offset, file_size);
                fprintf(stderr, "offset + chunk_size = %zu\n", offset + chunk_size);
                fclose(file);
                exit(EXIT_FAILURE);
            }

            // Seek to the offset
            fseek(file, offset, SEEK_SET);

            size_t bytes_remaining = chunk_size;
            while (bytes_remaining > 0) {
                bzero(buffer, BUFFER_SIZE);
                size_t bytes_to_read = (bytes_remaining > BUFFER_SIZE) ? BUFFER_SIZE : bytes_remaining;
                size_t bytes_read = fread(buffer, 1, bytes_to_read, file);

                if (bytes_read == 0) {
                    if (feof(file)) {
                        fprintf(stderr, "End of file reached (offset: %ld, remaining: %zu bytes)\n", offset, bytes_remaining);
                    } else if (ferror(file)) {
                        perror("Error reading from file");
                    }
                    break;
                }

                size_t bytes_to_send = bytes_read;
                size_t bytes_sent_total = 0;
                while (bytes_to_send > 0) {
                    size_t bytes_sent = write(client_socket, buffer + bytes_sent_total, bytes_to_send);
                    if (bytes_sent <= 0) {
                        perror("Error sending data to client");
                        close(client_socket);
                        return;
                    } else {
                        bytes_to_send -= bytes_sent;
                        bytes_sent_total += bytes_sent;

                        sleep(1);
                    }
                }

                bytes_remaining -= bytes_read;
                fprintf(stderr, "Sent chunk (offset: %zu, chunk_size: %zu) - progress: %zu / %zu\n", offset, chunk_size, chunk_size - bytes_remaining, chunk_size); // or use log_file instead of stderr
            }

            fclose(file);
        } else {
            snprintf(buffer, BUFFER_SIZE, "ERROR File not found");
        }
    }

    fprintf(stderr, "3) passed sending response\n");

    close(client_socket);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // init socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // init address
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // listen socket
    if (listen(server_socket, 5) == -1) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    while (1) {

        // accept socket
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket == -1) {
            perror("Accept failed");
            continue;
        }

        handle_client(client_socket);
    }

    close(server_socket);
    return 0;
}
