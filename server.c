// Based on code from https://www.geeksforgeeks.org/tcp-server-client-implementation-in-c/

// List of changes:
//      Added comments primarily for own understanding
//      renamed some funcs and vars in how I interpreted roles
//      Implemented server side multithreading
// Planned changes:
//      Implement multithreading (client side)
//      Encrypt message with caesar cipher
//      Allow for file transfers

#include <stdio.h> 
#include <netdb.h> 
#include <netinet/in.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <unistd.h> // read(), write(), close()
#include <pthread.h>
#define MAX 80 
#define PORT 8080 
#define SA struct sockaddr 

// Structure to hold client information
typedef struct {
    int client_id;
    int conn;
    struct sockaddr_in client_addr;
} client_info;

// Global variable to assign unique client IDs
int client_counter = 0;
pthread_mutex_t client_counter_lock;

// Function designed for chat as medium between client and server. In this case it takes in the message from the server and sends it to the client.
//      Takes in connection parameter conn
//      Creates buffer and zeroes it. Buffer then takes in a message (read()) from the client through the connection (conn)
//      prints it out for user readability
//      zeroes out buffer again to take in msg to send back to client. Waits for server to respond.
//      Writes msg back to client
//      Checks if there is an exit message
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
        read(conn, buff, sizeof(buff)); 
        // Print buffer which contains the client contents 
        printf("From client %d: %s\t To client %d: ", client_id, buff, client_id); 
        bzero(buff, MAX); 
        n = 0; 
        // Copy server message in the buffer 
        while ((buff[n++] = getchar()) != '\n') 
            ; 
   
        // Send that buffer to client 
        write(conn, buff, sizeof(buff)); 
   
        // If msg contains "exit" then server exit and chat ended. 
        if (strncmp("exit", buff, 4) == 0) { 
            printf("Client %d disconnected...\n", client_id);
            break; 
        } 
    } 
    // Close the connection
    close(conn);
    free(client);
    return NULL;
} 
   
// Main driver function
//    Meaning of certain variables:
//      AF_INET: Refers to the types of addresses the socket can take. In this case it refers to IPV4 addresses.
//      SOCK_STREAM: The socket is a TCP socket.
//      INADDR_ANY: Used to bind to all local network address on the machine
int main() 
{ 
    int sockfd, conn, len; 
    struct sockaddr_in servaddr, cli; 

    // Initialize the client ID counter lock
    pthread_mutex_init(&client_counter_lock, NULL);
   
    // Socket creation and verification 
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
        printf("socket creation failed...\n"); 
        exit(0); 
    } 
    else
        printf("Socket successfully created..\n"); 
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
        pthread_mutex_lock(&client_counter_lock);
        client->client_id = client_counter++;
        pthread_mutex_unlock(&client_counter_lock);

        client->conn = conn;
        client->client_addr = cli;

        pthread_create(&thread_id, NULL, medium, client);
        pthread_detach(thread_id);
    }
   
    // Close the socket (will not be reached unless the server is shut down)
    close(sockfd); 
    pthread_mutex_destroy(&client_counter_lock);
    return 0;
}