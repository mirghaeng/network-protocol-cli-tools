// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h> // For struct timeval

#define BUFFER_SIZE 1024
#define MAX_RETRIES 5
#define TIMEOUT_SEC 5 // Timeout for resending packets
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
    struct sockaddr_in client_addr;
    socklen_t addr_len;
    char command[10];
    char filename[256];
    size_t offset;
    size_t chunk_size;
    int server_socket;
} ClientRequest;

void *handle_request(void *arg)
{
    ClientRequest *request = (ClientRequest *)arg;

    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);

    if (strcmp(request->command, "CHECK") == 0) {
        fprintf(stderr, "Thread %lu) Server: Received CHECK request from client port: %d\n", pthread_self(), ntohs(request->client_addr.sin_port));

        // Check if file exists and get file size
        FILE *file = fopen(request->filename, "rb");
        if (!file) {
            snprintf(buffer, BUFFER_SIZE, "ERROR File not found");
            sendto(request->server_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&request->client_addr, request->addr_len);
            fprintf(stderr, "Thread %lu) File not found: %s\n", pthread_self(), request->filename);
            pthread_exit(NULL);
        }
        fseek(file, 0, SEEK_END);
        size_t file_size = ftell(file);
        fclose(file);

        // DEBUG
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &request->client_addr.sin_addr, client_ip, sizeof(client_ip));
        fprintf(stderr, "Thread %lu) CHECK Server: Sending to %s:%d\n", pthread_self(), client_ip, ntohs(request->client_addr.sin_port));

        // Send CHECK response
        snprintf(buffer, BUFFER_SIZE, "OK %zu", file_size);
        sendto(request->server_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&request->client_addr, request->addr_len);
        fprintf(stderr, "Thread %lu) CHECK request: OK %zu\n", pthread_self(), file_size);
    } else if (strcmp(request->command, "GET") == 0) {
        fprintf(stderr, "Thread %lu) GET request: processing (offset=%zu, chunk_size=%zu)...\n", pthread_self(), request->offset, request->chunk_size);

        // Check if file exists
        FILE *file = fopen(request->filename, "rb");
        if (!file) {
            snprintf(buffer, BUFFER_SIZE, "ERROR File not found");
            sendto(request->server_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&request->client_addr, request->addr_len);
            fprintf(stderr, "Thread %lu) File not found: %s\n", pthread_self(), request->filename);
            pthread_exit(NULL);
        }
        fseek(file, 0, SEEK_END);
        size_t file_size = ftell(file);
        rewind(file);

        if (request->offset >= file_size || request->offset + request->chunk_size > file_size) {
            fprintf(stderr, "Thread %lu) Invalid chunk_size: %zu, offset: %zu, file size: %zu\n", pthread_self(), request->chunk_size, request->offset, file_size);
            fprintf(stderr, "Thread %lu) offset + chunk_size = %zu\n", pthread_self(), request->offset + request->chunk_size);
            fclose(file);
            exit(EXIT_FAILURE);
        }

        // Seek to the offset
        fseek(file, request->offset, SEEK_SET);

        size_t bytes_remaining = request->chunk_size;
        size_t seq_num = 0; // Sequence number for packets
        while (bytes_remaining > 0) {
            bzero(buffer, BUFFER_SIZE);

            // Make data packet
            size_t bytes_to_read = (bytes_remaining > (BUFFER_SIZE - sizeof(seq_num))) ? (BUFFER_SIZE - sizeof(seq_num)) : bytes_remaining;
            size_t bytes_read = fread(buffer + sizeof(seq_num), 1, bytes_to_read, file);
            if (bytes_read <= 0) {
                if (feof(file)) {
                    fprintf(stderr, "Thread %lu) End of file reached (offset: %ld, remaining: %zu bytes)\n", pthread_self(), request->offset, bytes_remaining);
                } else if (ferror(file)) {
                    perror("Error reading from file");
                }
                break;
            }
            memcpy(buffer, &seq_num, sizeof(seq_num));

            // Send data packet
            ssize_t bytes_sent = sendto(request->server_socket, buffer, bytes_read + sizeof(seq_num), 0, (struct sockaddr *)&request->client_addr, request->addr_len);
            fprintf(stderr, "Thread %lu) [Wait] Sent data pkt to client (seq_num=%zu, bytes_sent=%zu).\n", pthread_self(), seq_num, bytes_sent);
            if (bytes_sent < 0) {
                perror("Error sending data to client");
                close(request->server_socket);
                pthread_exit(NULL);
            }

            // Wait for ACK
            pthread_mutex_lock(&ack_queue_lock);
            int ack_received = 0;
            while (!ack_received) {
                for (int i = 0; i < ack_queue_size; i++) {
                    if (ack_queue[i].seq_num == seq_num &&
                        memcmp(&ack_queue[i].client_addr, &request->client_addr, sizeof(request->client_addr)) == 0) {
                        ack_received = 1;
                        fprintf(stderr, "Thread %lu) [DONE] ACK (seq_num=%zu).\n", pthread_self(), ack_queue[i].seq_num);

                        // Remove ACK from queue
                        ack_queue_size--;
                        memmove(&ack_queue[i], &ack_queue[i + 1], (ack_queue_size - i) * sizeof(AckPacket));
                        break;
                    }
                }
                if (!ack_received) {
                    pthread_cond_wait(&ack_queue_cond, &ack_queue_lock);
                }
            }
            pthread_mutex_unlock(&ack_queue_lock);

            ssize_t payload_size = bytes_sent - sizeof(seq_num);
            if (payload_size > bytes_remaining) {
                fprintf(stderr, "Error: Received more payload data than expected! payload_size=%zd, bytes_remaining=%zd\n", payload_size, bytes_remaining);
                pthread_exit(NULL);
            }

            // Move to next packet
            seq_num++;
            bytes_remaining -= payload_size;
            fprintf(stderr, "Thread %lu) Progress: %zd bytes remaining\n", pthread_self(), bytes_remaining);
        }

        fclose(file);
    }

    free(request);
    pthread_exit((void *)0); // Success
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // init socket
    int server_socket = socket(AF_INET, SOCK_DGRAM, 0);
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
    } else {
        fprintf(stderr, "Ready to receive requests...\n");
    }

    while (1) {
        char buffer[BUFFER_SIZE];
        bzero(buffer, BUFFER_SIZE);

        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        ssize_t received = recvfrom(server_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
        if (received < 0) continue;

        buffer[received] = '\0';
        printf("Routing request: %s\n", buffer);

        // Filter ACK packets and enqueue them for the appropriate thread
        if (strncmp(buffer, "ACK", 3) == 0) {
            size_t seq_num;
            sscanf(buffer + 4, "%zu", &seq_num);

            pthread_mutex_lock(&ack_queue_lock);
            if (ack_queue_size < MAX_QUEUE_SIZE) {
                ack_queue[ack_queue_size].seq_num = seq_num;
                ack_queue[ack_queue_size].client_addr = client_addr;
                ack_queue[ack_queue_size].addr_len = addr_len;
                ack_queue_size++;
                pthread_cond_broadcast(&ack_queue_cond);
            }
            pthread_mutex_unlock(&ack_queue_lock);
            continue;
        }

        // Handle new request (format: CHECK <filename> or GET <filename> <offset> <chunk_size>)
        ClientRequest *request = malloc(sizeof(ClientRequest));
        request->client_addr = client_addr;
        request->addr_len = addr_len;
        request->server_socket = server_socket;
        if (sscanf(buffer, "%9s %255s %zu %zu", request->command, request->filename, &request->offset, &request->chunk_size) < 2) {
            fprintf(stderr, "Malformed request: %s\n", buffer);
            free(request);
            continue;
        }

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_request, (void *)request) != 0) {
            fprintf(stderr, "Failed to create thread for request: %s\n", buffer);
            free(request);
            continue;
        }
        pthread_detach(thread); // Automatically clean up thread resources
    }

    close(server_socket);
    return 0;
}
