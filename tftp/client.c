// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/time.h> // For struct timeval

#define BUFFER_SIZE 1024
#define MAX_RETRIES 5
#define TIMEOUT_SEC 5 // Timeout for blocking to receive data
#define MAX_QUEUE_SIZE 5

pthread_mutex_t shared_socket_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    size_t seq_num;
    struct sockaddr_in client_addr;
    socklen_t addr_len;
} AckPacket;

AckPacket ack_queue[MAX_QUEUE_SIZE];
int ack_queue_size = 0;
pthread_mutex_t ack_queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ack_queue_cond = PTHREAD_COND_INITIALIZER;

typedef struct {
    char *server_ip;
    int server_port;
    char *filename;
    size_t offset;
    size_t size;
    char *output;
    // pthread_mutex_t *lock; // Protect shared resources
    int sock_fd;
} DownloadTask;

void *download_chunk(void *arg) {
    DownloadTask *task = (DownloadTask *)arg;

    // Create shared client socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
    {
        perror("Socket creation failed\n");
        pthread_exit(NULL);
    }
    task->sock_fd = sock;

    // Construct server address info
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(task->server_port);
    inet_pton(AF_INET, task->server_ip, &server_addr.sin_addr);

    // Make GET request    
    char request[BUFFER_SIZE];
    snprintf(request, BUFFER_SIZE, "GET %s %zu %zu", task->filename, task->offset, task->size);
    fprintf(stderr, "%s\n", request);
    if (sendto(sock, request, strlen(request), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Send request failed.");
        pthread_exit(NULL);
    }

    // Retrieve GET response
    ssize_t bytes_remaining = task->size;
    char *output_ptr = task->output;
    int retry_count = 0;
    while (bytes_remaining > 0) {
        fprintf(stderr, "Get bytes...\n");

        pthread_mutex_lock(&shared_socket_lock);
        struct timeval timeout = {TIMEOUT_SEC, 0};
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            perror("setsockopt failed");
            exit(EXIT_FAILURE);
        }
        char buffer[BUFFER_SIZE];
        bzero(buffer, BUFFER_SIZE);
        socklen_t addr_len = sizeof(server_addr); // or &(socklen_t){sizeof(server_addr)} in the parameters of recvfrom()
        ssize_t bytes_received = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &addr_len);
        pthread_mutex_unlock(&shared_socket_lock);

        if (bytes_received <= 0) {
            retry_count++;
            fprintf(stderr, "Thread %lu) Retry %d/%d for chunk offset %zu (remaining: %zu bytes)\n", pthread_self(), retry_count, MAX_RETRIES, task->offset, bytes_remaining);
            if (retry_count > MAX_RETRIES) {
                fprintf(stderr, "Thread %lu) Failed to receive chunk after %d retries. Exiting thread.\n", pthread_self(), MAX_RETRIES);
                pthread_exit((void *)1); // Failure
            }
            continue;
        }

        // Extract seq_num
        size_t seq_num;
        memcpy(&seq_num, buffer, sizeof(seq_num));

        // Make ACK
        char ack_packet[BUFFER_SIZE];
        snprintf(ack_packet, BUFFER_SIZE, "ACK %zu", seq_num);

        pthread_mutex_lock(&shared_socket_lock);
        ssize_t bytes_sent = sendto(sock, ack_packet, strlen(ack_packet), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        pthread_mutex_unlock(&shared_socket_lock);

        if (bytes_sent < 0) {
            perror("Sending ACK failed");
            pthread_exit((void *)1); // Failure
        }

        retry_count = 0;

        ssize_t payload_size = bytes_received - sizeof(seq_num);
        if (payload_size > bytes_remaining) {
            fprintf(stderr, "Error: Received more payload data than expected! payload_size=%zd, bytes_remaining=%zd\n", payload_size, bytes_remaining);
            pthread_exit((void *)1); // Failure
        }
        if (output_ptr + payload_size > task->output + task->size) {
            fprintf(stderr, "Error: Attempting to write beyond the allocated buffer!\n");
            pthread_exit((void *)1); // Failure
        }

        fprintf(stderr, "Thread %lu) Received data pkt (seq_num=%zu, payload=%zu).\n", pthread_self(), seq_num, payload_size);
        fprintf(stderr, "Thread %lu) Sent ACK (seq_num=%zu) to %s:%d.\n", pthread_self(), seq_num, inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));

        // Write received data to the shared output buffer
        // pthread_mutex_lock(task->lock);
        memcpy(output_ptr, buffer + sizeof(seq_num), payload_size);
        // pthread_mutex_unlock(task->lock);
        // since output_ptr & task->output points to distinct nonoverlapping parts of file_data for each thread, then output_ptr is unique per thread

        bytes_remaining -= payload_size;
        output_ptr += payload_size;
        fprintf(stderr, "Thread %lu) Progress: %zd bytes remaining\n", pthread_self(), bytes_remaining);
    }

    pthread_exit((void *)0); // Success
}

int main(int argc, char *argv[]) {

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server-info.txt> <num-chunks> <filename>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_info_file = argv[1];
    int num_connections = atoi(argv[2]);
    char *filename = argv[3];

    // File exists
    FILE *file = fopen(server_info_file, "r");
    if (!file) {
        perror("Server info file open failed");
        exit(EXIT_FAILURE);
    }

    // Parse info
    char servers[10][256];
    int ports[10], server_count = 0;
    while (fscanf(file, "%s %d", servers[server_count], &ports[server_count]) != EOF) {
        server_count++;
    }
    fclose(file);

    // Assume the first server for file size check
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ports[0]);
    inet_pton(AF_INET, servers[0], &server_addr.sin_addr);

    // Send CHECK request
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "CHECK %s", filename);
    fprintf(stderr, "%s\n", buffer);
    if (sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("CHECK request failed.");
        close(sock);
        pthread_exit(NULL);
    }
    bzero(buffer, BUFFER_SIZE);

    // Receive response
    ssize_t received = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &(socklen_t){sizeof(server_addr)});
    if (received <= 0) {
        perror("Failed to get file size");
        close(sock);
        exit(EXIT_FAILURE);
    }
    size_t file_size;
    sscanf(buffer, "OK %zu", &file_size);
    fprintf(stderr, "%s\n", buffer);
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
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
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
        // tasks[i].lock = &lock;

        // Create the thread
        if (pthread_create(&threads[i], NULL, download_chunk, (void *)&tasks[i]) != 0) {
            perror("Error creating thread");
            free(file_data);
            exit(EXIT_FAILURE);
        }
    }

    // Wait for tasks to finish
    int thread_status;
    for (int i = 0; i < num_connections; i++) {
        pthread_join(threads[i], (void **)&thread_status);
        if (thread_status != 0)
        {
            fprintf(stderr, "Thread %d failed to download its chunk\n", i);
        }
    }

    // Close all sockets
    for (int i = 0; i < num_connections; i++) {
        if (tasks[i].sock_fd > 0) {
            close(tasks[i].sock_fd);
        }
        pthread_detach(threads[i]); // Automatically clean up thread resources
    }

    FILE *output_file = fopen("output.dat", "wb");
    fwrite(file_data, 1, file_size, output_file);
    fclose(output_file);

    free(file_data);
    return 0;
}
