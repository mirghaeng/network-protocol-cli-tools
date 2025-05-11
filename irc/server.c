#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_CLIENTS 100
#define MAX_WAITING 100
#define BUFFER_SIZE 1024

typedef struct {
    char id[32];
    int socket;
    int port;
    char in_chat;
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t client_mutex;

char waiting_clients[MAX_WAITING][32];
int waiting_count = 0;

void broadcast(const char *message, int exclude_socket) {
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < client_count; ++i) {
        if (clients[i].socket != exclude_socket) {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&client_mutex);
}

void *client_handler(void *arg) {
    int client_socket = *(int *)arg;
    char buffer[BUFFER_SIZE];
    char client_id[32] = {0};

    // Get client ID
    recv(client_socket, client_id, sizeof(client_id), 0);
    client_id[strcspn(client_id, "\n")] = '\0'; // Remove any newline

    // Check if ID already exists
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].id, client_id) == 0) {
            pthread_mutex_unlock(&client_mutex);
            send(client_socket, "Client ID is already taken.\n", 29, 0);
            close(client_socket);
            pthread_exit(NULL);
        }
    }

    // Add client to client list
    strcpy(clients[client_count].id, client_id);
    clients[client_count].socket = client_socket;
    clients[client_count].port = -1;
    clients[client_count].in_chat = 'n';
    client_count++;
    pthread_mutex_unlock(&client_mutex);

    sprintf(buffer, "Client %s connected.\n", client_id);
    send(client_socket, buffer, strlen(buffer), 0);
    printf("%s", buffer);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        buffer[strcspn(buffer, "\n")] = '\0'; // Remove any newline

        // Bad prompt
        if (bytes_received <= 0) {
            printf("Client %s disconnected.\n", client_id);
            close(client_socket);

            // Remove client from client list
            pthread_mutex_lock(&client_mutex);
            for (int i = 0; i < client_count; ++i) {
                if (clients[i].socket == client_socket) {
                    clients[i] = clients[client_count - 1];
                    client_count--;
                    break;
                }
            }
            pthread_mutex_unlock(&client_mutex);
            break;
        }

        printf("Command from %s: %s\n", client_id, buffer);

        if (strncmp(buffer, "/list", 5) == 0) {
            printf("waiting_count: %d\n", waiting_count);
            if (waiting_count == 0) {
                char list_response[BUFFER_SIZE] = "No clients are waiting.\n";
                send(client_socket, list_response, strlen(list_response), 0);
            } else {
                char list_response[BUFFER_SIZE] = "Waiting clients:\n";

                // Get waiting client list
                pthread_mutex_lock(&client_mutex);
                for (int i = 0; i < waiting_count; ++i) {
                    strcat(list_response, waiting_clients[i]);
                    strcat(list_response, "\n");
                }
                pthread_mutex_unlock(&client_mutex);

                send(client_socket, list_response, strlen(list_response), 0);
            }
        } else if (strncmp(buffer, "/wait", 5) == 0) {

            // Check if client is already waiting
            pthread_mutex_lock(&client_mutex);
            int already_waiting = 0;
            for (int i = 0; i < waiting_count; ++i) {
                if (strcmp(waiting_clients[i], client_id) == 0) {
                    already_waiting = 1;
                    break;
                }
            }

            // Switch to WAIT mode if not already waiting
            if (!already_waiting) {
                memset(buffer, 0, BUFFER_SIZE);
                recv(client_socket, buffer, BUFFER_SIZE, 0);
                int listening_port = atoi(buffer);

                if (listening_port > 0) {
                    strcpy(waiting_clients[waiting_count], client_id);
                    clients[waiting_count].port = listening_port;
                    clients[waiting_count].in_chat = 'y';
                    waiting_count++;
                    printf("waiting_count: %d\n", waiting_count);
                    send(client_socket, "Waiting for connection. (Ctrl+C to stop waiting)\n", 50, 0);
                } else {
                    send(client_socket, "Invalid port number.\n", 22, 0);
                }
            } else {
                send(client_socket, "You are already in WAIT mode.\n", 30, 0);
            }
            pthread_mutex_unlock(&client_mutex);
        } else if (strncmp(buffer, "/connect", 8) == 0) {
            char target_id[32];
            sscanf(buffer, "/connect %s", target_id);

            // Check if client is waiting
            pthread_mutex_lock(&client_mutex);
            int target_index = -1;
            for (int i = 0; i < waiting_count; i++) {
                if (strcmp(waiting_clients[i], target_id) == 0) {
                    target_index = i;
                    break;
                }
            }

            if (target_index == -1) {
                send(client_socket, "Client is not waiting.\n", 24, 0);
                pthread_mutex_unlock(&client_mutex);
            } else {
                char target_ip[INET_ADDRSTRLEN] = "127.0.0.1";;
                // strcpy(target_ip, clients[target_index].ip);
                int target_port = clients[target_index].port;

                // Remove client from waiting client list
                for (int i = target_index; i < waiting_count - 1; i++) {
                    strcpy(waiting_clients[i], waiting_clients[i + 1]);
                }
                waiting_count--;
                pthread_mutex_unlock(&client_mutex);

                // Send CONNECT info to requesting client
                memset(buffer, 0, BUFFER_SIZE);
                snprintf(buffer, BUFFER_SIZE, "CONNECT %s %s %d\n", target_id, target_ip, target_port);
                send(client_socket, buffer, strlen(buffer), 0);

                // // Send REQUEST request to waiting client for y or n
                // memset(buffer, 0, BUFFER_SIZE);
                // snprintf(buffer, BUFFER_SIZE, "REQUEST %s wants to connect.\n", client_id);
                // send(clients[target_index].socket, buffer, strlen(buffer), 0);
                // printf("waiting port: %d\n", clients[target_index].socket);
            }
        } else if (strncmp(buffer, "/stop_wait", 10) == 0) {
            // Find client from waiting client list
            pthread_mutex_lock(&client_mutex);
            int target_index;
            for (int i = 0; i < waiting_count; i++) {
                if (strcmp(waiting_clients[i], client_id) == 0) {
                    target_index = i;
                    break;
                }
            }

            // Remove client from waiting client list
            for (int i = target_index; i < waiting_count - 1; i++) {
                strcpy(waiting_clients[i], waiting_clients[i + 1]);
            }
            waiting_count--;
            pthread_mutex_unlock(&client_mutex);
            printf("waiting_count: %d\n", waiting_count);

            // Send stop wait response
            memset(buffer, 0, BUFFER_SIZE);
            send(client_socket, "\nStopped waiting.\n", 18, 0);
        } else if (strncmp(buffer, "/quit", 5) == 0) {
            memset(buffer, 0, BUFFER_SIZE);
            sprintf(buffer, "Client %s has left.\n", client_id);
            close(client_socket);
            break;
        } else {
            send(client_socket, "Bad prompt.\n", 13, 0);
        }
    }

    free(arg);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int server_socket, new_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(atoi(argv[1]));

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is running on port %s\n", argv[1]);

    pthread_mutex_init(&client_mutex, NULL);

    while (1) {
        addr_size = sizeof(client_addr);
        new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);
        if (new_socket == -1) {
            perror("Accept failed");
            continue;
        }

        pthread_t thread;
        int *new_sock_ptr = malloc(sizeof(int));
        *new_sock_ptr = new_socket;

        if (pthread_create(&thread, NULL, client_handler, (void *)new_sock_ptr) != 0) {
            perror("Thread creation failed");
            continue;
        }

        pthread_detach(thread);
    }

    close(server_socket);
    pthread_mutex_destroy(&client_mutex);
    return 0;
}
