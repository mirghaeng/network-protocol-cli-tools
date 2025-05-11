#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define BUFFER_SIZE 1024

typedef enum { INFO, WAIT, CHAT } Mode;

typedef struct {
    char id[32];
    int client_socket;
    int listener_socket;
} ClientInfo;
static ClientInfo *global_client_info = NULL;

typedef struct {
    char id[32];  // Peer ID (for CHAT mode)
    int socket;   // Peer socket (for CHAT mode)
    Mode mode;    // Current mode (INFO, WAIT, CHAT)
} PeerInfo;
static PeerInfo *global_peer_info = NULL;

void print_prompt() {
    printf("%s> ", global_client_info->id);
    fflush(stdout);
}

void handle_sigint(int sig) {
    if (global_peer_info == NULL || global_client_info == NULL) {
        printf("\nSignal received, but client not fully initialized.\n");
        return;
    }

    // in INFO mode
    if (global_peer_info->mode == INFO) {
        printf("\nCtrl+C ignored in INFO mode.\n");
        print_prompt();
        return;
    }

    // in CHAT mode
    if (global_peer_info->mode == CHAT) {
        send(global_peer_info->socket, "LEAVE", 5, 0);

    // in WAIT mode
    } else if (global_peer_info->mode == WAIT) {
        send(global_client_info->client_socket, "/stop_wait", 10, 0);
        close(global_client_info->listener_socket);
        global_client_info->listener_socket = -1;
        global_peer_info->mode = INFO;
    }
}

void *receive_peer_messages(void *arg) {
    PeerInfo *peer_info = (PeerInfo *)arg;
    char buffer[BUFFER_SIZE];

    while (1) {
        if (peer_info->socket < 0) {
            pthread_exit(NULL);
        }

        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(peer_info->socket, buffer, BUFFER_SIZE, 0);

        // connection is closed
        if (bytes_received == 0) {
            printf("\nLeft conversation with %s.\n", peer_info->id);
            print_prompt();
            close(global_peer_info->socket);
            global_peer_info->socket = -1;
            global_peer_info->mode = INFO;
            pthread_exit(NULL);

        // error receiving message
        } else if (bytes_received < 0) {
            perror("\nError receiving message");
            print_prompt();
            close(global_peer_info->socket);
            global_peer_info->socket = -1;
            global_peer_info->mode = INFO;
            pthread_exit(NULL);
        }
        
        if (strncmp(buffer, "LEAVE", 5) == 0) {
            printf("\nThe other client has left conversation.\n");
            print_prompt();
            close(global_peer_info->socket);
            global_peer_info->socket = -1;
            global_peer_info->mode = INFO;
            pthread_exit(NULL);

        } else {
            printf("\n%s: %s\n", peer_info->id, buffer);
            print_prompt();
        }
    }
}

// non-blocking for accept() in /wait in case user wants to /quit while waiting
void *wait_for_connection(void *arg) {
    ClientInfo *client_info = (ClientInfo *)arg;
    struct sockaddr_in peer_addr;
    socklen_t peer_size = sizeof(peer_addr);

    int new_socket = accept(client_info->listener_socket, (struct sockaddr *)&peer_addr, &peer_size);
    if (new_socket < 0) {
        printf("Stopping wait: No incoming connection.\n");
        return NULL;
    }

    char peer_id[32];
    memset(peer_id, 0, sizeof(peer_id));
    recv(new_socket, peer_id, sizeof(peer_id), 0);
    if (strlen(peer_id) == 0) {
        printf("Invalid peer ID. Closing connection.\n");
        close(new_socket);
        return NULL;
    }

    printf("Connection from %s.\n", peer_id);
    print_prompt();

    global_peer_info->socket = new_socket;
    strcpy(global_peer_info->id, peer_id);
    global_peer_info->mode = CHAT;

    pthread_t receive_thread;
    pthread_create(&receive_thread, NULL, receive_peer_messages, (void *)global_peer_info);

    pthread_join(receive_thread, NULL);

    return NULL;
}

void handle_connect(char *peer_id, char *peer_ip, int peer_port) {
    int peer_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (peer_socket < 0) {
        perror("Failed to create socket.\n");
    }

    struct sockaddr_in peer_addr;
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(peer_port);
    inet_pton(AF_INET, peer_ip, &peer_addr.sin_addr);

    if (connect(peer_socket, (struct sockaddr *)&peer_addr, sizeof(peer_addr)) == 0) {              
        printf("Connected to %s.\n", peer_id);
        print_prompt();

        send(peer_socket, global_client_info->id, strlen(global_client_info->id), 0);

        global_peer_info->socket = peer_socket;
        strcpy(global_peer_info->id, peer_id);
        global_peer_info->mode = CHAT;

        pthread_t receive_thread;
        pthread_create(&receive_thread, NULL, receive_peer_messages, (void *)global_peer_info);
        
        pthread_join(receive_thread, NULL);
    } else {
        perror("Failed to connect");
        close(global_peer_info->socket);
        global_peer_info->socket = -1;
    }
}

void *receive_server_messages(void *arg) {
    ClientInfo *client_info = (ClientInfo *)arg;
    int sc = client_info->client_socket;
    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(sc, buffer, BUFFER_SIZE, 0);

        if (bytes_received <= 0) {
            printf("Server disconnected. Exiting client.\n");
            close(sc);
            exit(0);
        }

        buffer[bytes_received] = '\0';

        if (strncmp(buffer, "Waiting for connection. (Ctrl+C to stop waiting)\n", 50) == 0) {
            printf("%s", buffer);
            print_prompt();
        } else if (strncmp(buffer, "CONNECT", 7) == 0) {
            char peer_id[32], peer_ip[INET_ADDRSTRLEN];
            int peer_port;
            sscanf(buffer, "CONNECT %s %s %d", peer_id, peer_ip, &peer_port);
            handle_connect(peer_id, peer_ip, peer_port);
        } else if (strncmp(buffer, "Client ID is already taken.\n", 29) == 0) {
            printf("%s", buffer);
            close(sc);
            exit(0);
        } else {
            printf("%s", buffer);
            print_prompt();
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <client-id>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    ClientInfo client_info;
    strcpy(client_info.id, argv[1]);
    PeerInfo peer_info = { .socket = -1, .mode = INFO };

    global_client_info = &client_info;
    global_peer_info = &peer_info;

    // Register signal handler using sigaction()
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    client_info.client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_info.client_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1234);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(client_info.client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    send(client_info.client_socket, client_info.id, strlen(client_info.id), 0);

    pthread_t server_thread;
    if (pthread_create(&server_thread, NULL, receive_server_messages, &client_info) != 0) {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        fflush(stdout);
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0';
        if (strlen(buffer) == 0) continue;

        if (strncmp(buffer, "/quit", 5) == 0) {
            if (global_peer_info->mode == CHAT) {
                send(global_peer_info->socket, "LEAVE", 5, 0);
                close(global_peer_info->socket);
            } else if (global_peer_info->mode == WAIT) {
                close(global_client_info->listener_socket);
            }
            
            close(client_info.client_socket);
            exit(0);
        }

        // Send CHAT message
        if (global_peer_info->mode == CHAT) {
            send(global_peer_info->socket, buffer, strlen(buffer), 0);
            print_prompt();

        // Send INFO message
        } else {
            if (strncmp(buffer, "/wait", 5) == 0) {
                if (global_peer_info->mode == WAIT) {
                    printf("Already in WAIT mode.\n");
                    continue;
                }

                int sent_bytes = send(client_info.client_socket, buffer, strlen(buffer), 0);
                if (sent_bytes < 0) {
                    perror("Error sending message to server");
                }

                global_peer_info->mode = WAIT;

                struct sockaddr_in server_addr, peer_addr;
                socklen_t peer_size = sizeof(peer_addr);
                client_info.listener_socket = socket(AF_INET, SOCK_STREAM, 0);
                server_addr.sin_family = AF_INET;
                server_addr.sin_addr.s_addr = INADDR_ANY;
                server_addr.sin_port = htons(0);  // Bind to any available port

                if (bind(client_info.listener_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                    perror("Failed to bind");
                    continue;
                }
                if (listen(client_info.listener_socket, 1) < 0) {
                    perror("Failed to listen");
                    continue;
                }

                getsockname(client_info.listener_socket, (struct sockaddr *)&server_addr, &peer_size);
                int assigned_port = ntohs(server_addr.sin_port);

                char port_msg[32];
                sprintf(port_msg, "%d", assigned_port);
                send(client_info.client_socket, port_msg, strlen(port_msg), 0);

                pthread_t wait_thread;
                pthread_create(&wait_thread, NULL, wait_for_connection, (void *)&client_info);
            } else if (strncmp(buffer, "/connect", 8) == 0) {
                if (global_peer_info->mode == WAIT) {
                    printf("Error, cannot connect in wait mode\n");
                    print_prompt();
                    continue;
                }

                send(client_info.client_socket, buffer, strlen(buffer), 0);
            } else {
                int sent_bytes = send(client_info.client_socket, buffer, strlen(buffer), 0);
                if (sent_bytes < 0) {
                    perror("Error sending message to server");
                }
            }
        }
    }

    close(client_info.client_socket);
    return 0;
}
