// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1048576 // todo: benchmark with 1024 (1KB), 4096 (4KB), 8192 (8KB), 16384 (16KB), 65536 (64KB), 131072 (128KB), (256KB), 1048576 (1MB)etc on 16BG RAM

typedef struct {
    char *server_ip;
    int server_port;
    char *filename;
    size_t offset;
    size_t size;
    char *output;
} DownloadTask;

void *download_chunk(void *arg) {
    DownloadTask *task = (DownloadTask *)arg;

    // Create client socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("Socket creation failed\n");
        pthread_exit(NULL);
    }

    // Construct server address info
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(task->server_port);
    inet_pton(AF_INET, task->server_ip, &server_addr.sin_addr);

    // Connect to server
    int retries = 3;
    while (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1 && retries > 0) {
        perror("Connection failed, retrying...");
        retries--;
        sleep(1);
    }
    if (retries == 0) {
        fprintf(stderr, "Unable to connect to %s:%d\n", task->server_ip, task->server_port);
        close(sock);
        pthread_exit(NULL);
    }

    fprintf(stdout, "Connected to %s:%d.\n", task->server_ip, task->server_port);

    // Make GET request
    char request[BUFFER_SIZE];
    snprintf(request, BUFFER_SIZE, "GET %s %zu %zu", task->filename, task->offset, task->size);
    if (write(sock, request, strlen(request)) == -1)
    {
        perror("Write failed.");
        close(sock);
        pthread_exit(NULL);
    }

    // Retrieve GET response
    ssize_t bytes_remaining = task->size;
    char *output_ptr = task->output;
    while (bytes_remaining > 0) {
        size_t bytes_to_read = (bytes_remaining > BUFFER_SIZE) ? BUFFER_SIZE : bytes_remaining;
        ssize_t bytes_received = recv(sock, output_ptr, bytes_to_read, 0);

        fprintf(stderr, "Before recv: bytes_remaining=%zd\n", bytes_remaining);

        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                fprintf(stderr, "Server closed connection prematurely. Bytes remaining: %zd\n", bytes_remaining);
            } else {
                perror("Error reading from socket");
            }
            close(sock);
            pthread_exit((void *)1); // Failure
        }

        // Defensive check
        if (bytes_received > bytes_remaining) {
            fprintf(stderr, "Error: Received more data than expected! bytes_received=%zd, bytes_remaining=%zd\n", bytes_received, bytes_remaining);
            pthread_exit(NULL);
        }

        bytes_remaining -= bytes_received;
        output_ptr += bytes_received;

        fprintf(stderr, "Received %zd bytes, %zd bytes remaining\n", bytes_received, bytes_remaining);
    }

    close(sock);
    // pthread_exit((void *)0); // Success
}

int main(int argc, char *argv[]) {

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server-info.txt> <num-connections> <filename>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_info_file = argv[1];
    int num_connections = atoi(argv[2]);
    char *filename = argv[3];

    FILE *file = fopen(server_info_file, "r");
    if (!file) {
        perror("Server info file open failed");
        exit(EXIT_FAILURE);
    }

    char servers[10][256];
    int ports[10], server_count = 0;

    while (fscanf(file, "%s %d", servers[server_count], &ports[server_count]) != EOF) {
        server_count++;
    }
    fclose(file);

    // Assume the first server for file size check
    char buffer[BUFFER_SIZE];
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ports[0]);
    inet_pton(AF_INET, servers[0], &server_addr.sin_addr);

    connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    snprintf(buffer, BUFFER_SIZE, "CHECK %s", filename);
    write(sock, buffer, strlen(buffer));
    bzero(buffer, BUFFER_SIZE);

    read(sock, buffer, BUFFER_SIZE);
    size_t file_size;
    sscanf(buffer, "OK %zu", &file_size);
    fprintf(stderr, "server CHECK response: %s\n", buffer);
    close(sock);

    if (file_size <= 0) {
        fprintf(stderr, "Error: Invalid file size (%zu)\n", file_size);
        exit(EXIT_FAILURE);
    }
    if (num_connections > file_size) {
        fprintf(stderr, "Warning: More connections than file size. Reducing connections.\n");
        num_connections = file_size;
    }

    // Set chunk size (minimum: 1)
    size_t chunk_size = file_size / num_connections;
    if (chunk_size < 1) {
        chunk_size = 1;
    }
    fprintf(stderr, "file_size: %zu, chunk_size: %zu, num_connections: %d\n", file_size, chunk_size, num_connections);

    pthread_t threads[num_connections];
    DownloadTask tasks[num_connections];
    // char *file_data = malloc(file_size);
    char *file_data = calloc(file_size, sizeof(char));
    if (!file_data) {
        perror("Failed to allocate file_data");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_connections; i++) {
        tasks[i].server_ip = servers[i % server_count];
        tasks[i].server_port = ports[i % server_count];
        tasks[i].filename = filename;
        tasks[i].offset = i * chunk_size;

        // Adjust the last chunk to handle any remaining bytes
        tasks[i].size = (i == num_connections - 1) ? file_size - (chunk_size * i) : chunk_size;

        // Validate the chunk size & offset
        if (tasks[i].size <= 0 || tasks[i].offset + tasks[i].size > file_size) {
            fprintf(stderr, "Error: Invalid chunk calculation assigned to thread %d (Offset: %zu, Size: %zu)\n", i, tasks[i].offset, tasks[i].size);
            free(file_data);
            exit(EXIT_FAILURE);
        }

        fprintf(stderr, "Thread %d: Assigned chunk - Offset: %zu, Size: %zu\n", i, tasks[i].offset, tasks[i].size);

        tasks[i].output = file_data + tasks[i].offset;

        // Create the thread
        if (pthread_create(&threads[i], NULL, download_chunk, (void *)&tasks[i]) != 0) {
            perror("Error creating thread");
            free(file_data);
            exit(EXIT_FAILURE);
        }
    }

    int thread_status;
    for (int i = 0; i < num_connections; i++) {
        pthread_join(threads[i], (void **)&thread_status);
        if (thread_status != 0)
        {
            fprintf(stderr, "Thread %d failed to download its chunk\n", i);
        }
    }

    FILE *output_file = fopen("output.dat", "wb");
    fwrite(file_data, 1, file_size, output_file);
    fclose(output_file);

    free(file_data);
    return 0;
}
