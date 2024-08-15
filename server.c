#include <stdio.h> 
#include <netdb.h> 
#include <netinet/in.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <unistd.h> // read(), write(), close()
#include <pthread.h>
#include <arpa/inet.h> // inet_ntop
#define MAX 80 
#define PORT 8080 
#define SA struct sockaddr 

// Structure to hold client information
typedef struct {
    int client_id;
    int conn;
    struct sockaddr_in client_addr;
} client_info;

// Global variables to manage clients
client_info* clients[100];  // Array to hold pointers to client information structures
int client_count = 0;
pthread_mutex_t clients_lock;

// Function to broadcast a message to all connected clients
void broadcast_message(char* message, int sender_conn) {
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < client_count; i++) {
        if (clients[i]->conn != sender_conn) {  // Don't send the message back to the sender
            write(clients[i]->conn, message, strlen(message));
        }
    }
    pthread_mutex_unlock(&clients_lock);
}

// Function to handle communication with a client
void *medium(void* client_info_ptr) 
{ 
    char buff[MAX]; 
    int n; 

    client_info *client = (client_info *)client_info_ptr;
    int conn = client->conn;
    int client_id = client->client_id;

    printf("Handling client with ID: %d\n", client_id);
    
    // Infinite loop for chat 
     for (;;) { 
        bzero(buff, MAX); 
   
        // Read the message from client and copy it in buffer 
        int bytes_read = read(conn, buff, sizeof(buff)); 
        if (bytes_read <= 0) {
            printf("Client %d disconnected or error occurred...\n", client_id);
            break;
        }

        // Print buffer which contains the client contents 
        printf("From client %d: %s\n", client_id, buff);

        // Broadcast the message to all other clients
        broadcast_message(buff, conn);
   
        // If msg contains "exit" then server exit and chat ended. 
        if (strncmp("exit", buff, 4) == 0) { 
            printf("Client %d has left the chat...\n", client_id);
            break; 
        } 
    } 
    // Remove the client from the list
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < client_count; i++) {
        if (clients[i]->conn == conn) {
            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_lock);

    // Close the connection
    close(conn);
    free(client);
    return NULL;
} 
   
// Main driver function
int main() 
{ 
    int sockfd, conn, len; 
    struct sockaddr_in servaddr, cli; 
    int opt = 1;

    // Initialize the clients array lock
    pthread_mutex_init(&clients_lock, NULL);
   
    // Socket creation and verification 
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
        printf("socket creation failed...\n"); 
        exit(0); 
    } 
    else
        printf("Socket successfully created..\n"); 

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    bzero(&servaddr, sizeof(servaddr)); 
   
    // Assign IP, PORT 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(PORT); 
   
    // Binding newly created socket to given IP and verification 
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) { 
        printf("socket bind failed...\n"); 
        exit(0); 
    } 
    else
        printf("Socket successfully binded..\n"); 
   
    // Now server is ready to listen and verification 
    if ((listen(sockfd, 5)) != 0) { 
        printf("Listen failed...\n"); 
        exit(0); 
    } 
    else
        printf("Server listening..\n"); 
    len = sizeof(cli); 
   
    // Infinite loop to accept multiple client connections
    while (1) {
        // Accept the data packet from client and verification 
        conn = accept(sockfd, (SA*)&cli, &len); 
        if (conn < 0) { 
            printf("server accept failed...\n"); 
            exit(0); 
        } 
        else
            printf("server accepted the client...\n"); 
   
        // Create a new thread to handle this client connection
        pthread_t thread_id;

        // Allocate memory for client information
        client_info *client = malloc(sizeof(client_info));
        if (!client) {
            printf("Failed to allocate memory for client information\n");
            close(conn);
            continue;
        }

        // Assign a unique ID to this client
        pthread_mutex_lock(&clients_lock);
        client->client_id = client_count;
        client->conn = conn;
        client->client_addr = cli;

        clients[client_count++] = client;
        pthread_mutex_unlock(&clients_lock);

        pthread_create(&thread_id, NULL, medium, client);
        pthread_detach(thread_id);
    }
   
    // Close the socket (will not be reached unless the server is shut down)
    close(sockfd); 
    pthread_mutex_destroy(&clients_lock);
    return 0;
}