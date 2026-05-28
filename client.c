#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 2048
#define NAME_LEN 32

// Secondary thread to handle incoming messages
void* listen_handler(void* socket_desc) {
    int sock = *(int*)socket_desc;
    char buffer[BUFFER_SIZE];
    int recv_size;

    while ((recv_size = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[recv_size] = '\0';
        
        // ANSI escape codes \r\033[K clear the current terminal line. 
        // This prevents incoming messages from mangling the text you are currently typing.
        printf("\r\033[K%s", buffer); 
        printf("You: "); 
        fflush(stdout); // Force prompt to render immediately
    }

    printf("\nServer connection lost or username rejected.\n");
    exit(0); // Kill the entire client process if the server drops us
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <server_IP> <port>\n", argv[0]);
        return 1;
    }

    char* ip = argv[1];
    int port = atoi(argv[2]);
    int sock;
    struct sockaddr_in server_addr;
    char username[NAME_LEN];
    char buffer[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return 1;
    }

    printf("Enter your username: ");
    fgets(username, NAME_LEN, stdin);
    username[strcspn(username, "\n")] = '\0'; 
    
    // Send username immediately upon connection
    send(sock, username, strlen(username), 0);

    // Spin up the listening thread
    pthread_t listen_thread;
    pthread_create(&listen_thread, NULL, listen_handler, (void*)&sock);

    // Main thread loops to handle typing
    while (1) {
        printf("You: ");
        if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
            if (strncmp(buffer, "/quit", 5) == 0) {
                break; // Let the user exit cleanly
            }
            send(sock, buffer, strlen(buffer), 0);
        }
    }

    close(sock);
    return 0;
}
