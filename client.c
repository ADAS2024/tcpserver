#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // bzero()
#include <sys/socket.h>
#include <unistd.h> // read(), write(), close()
#include <pthread.h>
#define MAX 80
#define PORT 8080
#define SA struct sockaddr


// Function is used to recieve messages from server.
void *receive_messages(void *sockfd_ptr) {
    int sockfd = *((int *)sockfd_ptr);
    char buffer[MAX];
    while (1) {
        bzero(buffer, sizeof(buffer));
        int n = read(sockfd, buffer, sizeof(buffer));
        if (n <= 0) {
            printf("Server disconnected\n");
            exit(0);
        }
        printf("Server: %s", buffer);
    }
    return NULL;
}

// Used to send messages to server. Takes in the connection socket and writes message from user to the socket which is then sent to server.
void *send_messages(void *sockfd_ptr) {
    int sockfd = *((int *)sockfd_ptr);
    char buffer[MAX];
    while (1) {
        bzero(buffer, sizeof(buffer));
        fgets(buffer, sizeof(buffer), stdin);
        write(sockfd, buffer, strlen(buffer));
        if (strncmp("exit", buffer, 4) == 0) {
            printf("Exiting...\n");
            exit(0);
        }
    }
    return NULL;
}
 
int main()
{
    int sockfd, connfd;
    struct sockaddr_in servaddr;
    pthread_t send_thread, recv_thread;
 
    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");

    bzero(&servaddr, sizeof(servaddr));
 
    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);
 
    // connect the client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr))
        != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else
        printf("connected to the server..\n");
 
    // create threads to send and recieve messages to and from server
    pthread_create(&recv_thread, NULL, receive_messages, &sockfd)
    pthread_create(&send_thread, NULL, send_messages, &sockfd)

    // we need to wait for thread to finish each operation with server, so we use join rather than detach
    pthread_join(recv_thread, NULL)
    pthread_join(send_thread, NULL)
 
    // close the socket
    close(sockfd);
    return 0;
}