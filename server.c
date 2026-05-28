#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048
#define NAME_LEN 32

typedef struct ClientNode {
    int socket_fd;
    char username[NAME_LEN];
    char ip_address[INET_ADDRSTRLEN];
    struct ClientNode* next;
} ClientNode;

ClientNode* head = NULL;
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;

// Broadcasts a message to all clients except the sender
void broadcast_message(const char* message, int sender_fd) {
    pthread_mutex_lock(&list_mutex);
    ClientNode* current = head;
    while (current != NULL) {
        if (current->socket_fd != sender_fd) {
            send(current->socket_fd, message, strlen(message), 0);
        }
        current = current->next;
    }
    pthread_mutex_unlock(&list_mutex);
}

// Removes a client from the linked list and frees memory
void remove_client(int socket_fd) {
    pthread_mutex_lock(&list_mutex);
    ClientNode* current = head;
    ClientNode* prev = NULL;
    
    while (current != NULL) {
        if (current->socket_fd == socket_fd) {
            if (prev == NULL) {
                head = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            break;
        }
        prev = current;
        current = current->next;
    }
    pthread_mutex_unlock(&list_mutex);
}

void* handle_client(void* arg) {
    ClientNode* cli = (ClientNode*)arg;
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE + NAME_LEN + 10];

    // Detach thread so resources are freed automatically when it exits
    pthread_detach(pthread_self()); 

    // 1. Receive the requested username
    int recv_size = recv(cli->socket_fd, buffer, NAME_LEN - 1, 0);
    if (recv_size <= 0) {
        close(cli->socket_fd);
        free(cli);
        return NULL;
    }
    
    buffer[recv_size] = '\0';
    buffer[strcspn(buffer, "\n")] = '\0'; // Strip the newline

    // 2. Lock list and check if username is unique
    pthread_mutex_lock(&list_mutex);
    ClientNode* current = head;
    int unique = 1;
    while (current != NULL) {
        if (strcmp(current->username, buffer) == 0) {
            unique = 0;
            break;
        }
        current = current->next;
    }

    // Reject if taken
    if (!unique) {
        pthread_mutex_unlock(&list_mutex);
        char* reject = "ERROR: Username already taken. Disconnecting...\n";
        send(cli->socket_fd, reject, strlen(reject), 0);
        close(cli->socket_fd);
        free(cli);
        return NULL;
    }

    // Add to linked list
    strcpy(cli->username, buffer);
    cli->next = head;
    head = cli;
    pthread_mutex_unlock(&list_mutex);

    printf("Client connected: %s (%s)\n", cli->username, cli->ip_address);
    
    // 3. Announce arrival
    snprintf(message, sizeof(message), "\n*** %s joined the chat ***\n", cli->username);
    broadcast_message(message, cli->socket_fd);

    // 4. Main listening loop for this client
    while ((recv_size = recv(cli->socket_fd, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[recv_size] = '\0';
        buffer[strcspn(buffer, "\n")] = '\0'; 
        
        if (strlen(buffer) == 0) continue; // Ignore empty enters

        snprintf(message, sizeof(message), "[%s]: %s\n", cli->username, buffer);
        broadcast_message(message, cli->socket_fd);
    }

    // 5. Cleanup on disconnect
    printf("Client disconnected: %s\n", cli->username);
    snprintf(message, sizeof(message), "\n*** %s left the chat ***\n", cli->username);
    broadcast_message(message, cli->socket_fd);
    
    close(cli->socket_fd);
    remove_client(cli->socket_fd);

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    
    // Fixes the annoying "Address already in use" error when restarting the server quickly
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); 

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    listen(server_socket, 10);
    printf("Server listening on port %d...\n", port);

    // Main accept loop
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) continue;

        ClientNode* new_client = (ClientNode*)malloc(sizeof(ClientNode));
        new_client->socket_fd = client_socket;
        inet_ntop(AF_INET, &(client_addr.sin_addr), new_client->ip_address, INET_ADDRSTRLEN);
        
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void*)new_client);
    }

    close(server_socket);
    return 0;
}
