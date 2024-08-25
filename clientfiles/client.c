//client code
#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // bzero()
#include <sys/socket.h>
#include <unistd.h> // read(), write(), close()
#include <pthread.h>
#include <dirent.h> // Required for directory operations for files
#define MAX 80
#define PORT 8080
#define SA struct sockaddr

//Sends exit request to server
void *send_messages(void *sockfd_ptr) {
    int sockfd = *((int *)sockfd_ptr);
    char buffer[MAX];
    
    while (1) {
        bzero(buffer, sizeof(buffer));
        fgets(buffer, sizeof(buffer), stdin);
        
        // Prepend "CHAT" command for normal chat messages
        char chat_message[MAX + 5];
        snprintf(chat_message, sizeof(chat_message), "CHAT %s", buffer);
        write(sockfd, chat_message, strlen(chat_message));

        if (strncmp(buffer, "exit", 4) == 0) {
            printf("Exiting...\n");
            exit(0);
        }
    }
    return NULL;
}

void *send_file(void *sockfd_ptr) {
    int sockfd = *((int *)sockfd_ptr);
    char buffer[MAX];
    char file_name[MAX];
    char file_path[MAX];

    printf("Enter the name of the file to send: ");
    scanf("%s", file_name);

    snprintf(file_path, sizeof(file_path), "txtfiles/%s", file_name); // We open the client in clientfiles so we access txtfiles directly.
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        printf("File not found: %s\n", file_path);
        return NULL;
    }

    // Send "SEND_FILE" command with file name
    snprintf(buffer, sizeof(buffer), "SEND_FILE %s", file_name);
    write(sockfd, buffer, strlen(buffer));

    // Wait for acknowledgment
    bzero(buffer, sizeof(buffer));
    read(sockfd, buffer, sizeof(buffer));
    if (strncmp(buffer, "ACK", 3) != 0) {
        printf("Server did not acknowledge file name.\n");
        fclose(file);
        return NULL;
    }

    // Send file contents
    while (!feof(file)) {
        int bytes_read = fread(buffer, 1, sizeof(buffer), file);
        write(sockfd, buffer, bytes_read);
    }

    // Inform server of end of file transfer
    write(sockfd, "EOF", 3);
    fclose(file);

    printf("File sent successfully.\n");
    return NULL;
}

void *request_and_display_file_list(void *sockfd_ptr) {
    int sockfd = *((int *)sockfd_ptr);
    char buffer[MAX];

    // Send "LIST_FILES" command
    write(sockfd, "LIST_FILES", strlen("LIST_FILES"));

    // Receive and display file list
    while (1) {
        bzero(buffer, sizeof(buffer));
        int bytes_read = read(sockfd, buffer, sizeof(buffer));
        if (bytes_read <= 0) {
            printf("Server disconnected or error occurred...\n");
            exit(0);
        }

        if (strncmp(buffer, "END_OF_LIST", 11) == 0) {
            break;
        }

        printf("%s\n", buffer);
    }

    return NULL;
}

void *receive_file_from_server(void *sockfd_ptr) {
    int sockfd = *((int *)sockfd_ptr);
    char buffer[MAX];
    char file_name[MAX];

    printf("Enter the name of the file to receive: ");
    scanf("%s", file_name);

    // Send "RECV_FILE" command with file name
    snprintf(buffer, sizeof(buffer), "RECV_FILE %s", file_name);
    write(sockfd, buffer, strlen(buffer));

    // Wait for file data
    FILE *file = fopen(file_name, "wb");
    if (file == NULL) {
        printf("Could not create file.\n");
        return NULL;
    }

    while (1) {
        bzero(buffer, sizeof(buffer));
        int bytes_read = read(sockfd, buffer, sizeof(buffer));
        if (bytes_read <= 0 || strncmp(buffer, "EOF", 3) == 0) {
            break;
        }

        fwrite(buffer, 1, bytes_read, file);
    }

    fclose(file);
    printf("File received successfully.\n");
    return NULL;
}

// Function to receive messages from server
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
        printf("%s", buffer);
    }
    return NULL;
}


void start_chatroom_clientside(const char* server_ip, int port)
{
    int sockfd, connfd, choice;
    struct sockaddr_in servaddr;
    pthread_t recv_thread, send_thread, file_send_thread, file_request_thread, file_list_thread; //Threads needed for chatroom functions, sending files to server, and accessing files from server

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
    servaddr.sin_family = AF_INET; // IPV4 address
    servaddr.sin_addr.s_addr = inet_addr(server_ip);
    servaddr.sin_port = htons(port);

    // connect the client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else
        printf("connected to the server..\n");

    
     while (1) {
        printf("Select an option:\n");
        printf("1. Chat\n");
        printf("2. Send File\n");
        printf("3. List Files\n");
        printf("4. Receive File\n");
        printf("5. Exit\n");
        scanf("%d", &choice);

        while (getchar() != '\n'); //Prevents issues with newlines affecting inputs

        switch (choice) {
            case 1: // Chat functionality
                // Create threads for receiving and sending messages
                printf("Joined chatroom... Do CHAT + [Your Message] to send a message \n");
                pthread_create(&recv_thread, NULL, receive_messages, &sockfd);
                pthread_create(&send_thread, NULL, send_messages, &sockfd);
                pthread_join(recv_thread, NULL);
                pthread_join(send_thread, NULL);
                break;

            case 2: // Send a file to the server
                pthread_create(&file_send_thread, NULL, send_file, &sockfd);
                pthread_join(file_send_thread, NULL);
                break;

            case 3: // List files on the server
                pthread_create(&file_list_thread, NULL, request_and_display_file_list, &sockfd);
                pthread_join(file_list_thread, NULL);
                break;

            case 4: // Receive a file from the server
                pthread_create(&file_request_thread, NULL, receive_file_from_server, &sockfd);  // Assuming you have a `receive_file_from_server` function
                pthread_join(file_request_thread, NULL);
                break;

            case 5: // Exit the chatroom
                write(sockfd, "EXIT", strlen("EXIT"));
                close(sockfd);  // Close the socket
                exit(0);  // Exit the program

            default:
                printf("Invalid choice. Please try again.\n");
        }
    }
}

int main() {
    start_chatroom_clientside("127.0.0.1", PORT);
    return 0;
}