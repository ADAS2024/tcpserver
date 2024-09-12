//server code
#include <stdio.h> 
#include <netdb.h> 
#include <netinet/in.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <unistd.h> // read(), write(), close()
#include <pthread.h>
#include <arpa/inet.h> 
#include <dirent.h> // Required for directory operations for files
#include "../cipher.h" // Cipher functions
#define MAX 80 
#define PORT 8080 
#define SA struct sockaddr 

// Structure to hold client information
typedef struct {
    int client_id;
    int conn;
    struct sockaddr_in client_addr;
} client_info;

client_info* clients[100];  
int client_count = 0;
pthread_mutex_t clients_lock;


void send_file_list(int conn) {
    struct dirent *de;
    char buffer[MAX];

    DIR *dr = opendir("txtfiles");

    if (dr == NULL) {
        printf("Could not open directory\n");
        return;
    }

    // Send file names
    while ((de = readdir(dr)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }
        snprintf(buffer, sizeof(buffer), "%s\n", de->d_name);
        write(conn, buffer, strlen(buffer));
    }

    // Indicate end of file list
    snprintf(buffer, sizeof(buffer), "END_OF_LIST\n");
    write(conn, buffer, strlen(buffer));

    closedir(dr);
}


// Function to broadcast a message to all connected clients
void broadcast_message(char* message, int sender_conn, int sender_id) {
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < client_count; i++) {
        if (clients[i]->conn != sender_conn) {  // Don't send the message back to the sender
            char formatted_message[MAX];
            snprintf(formatted_message, sizeof(formatted_message), "Client %d: %s", sender_id, message);
            write(clients[i]->conn, formatted_message, strlen(formatted_message));
        }
    }
    pthread_mutex_unlock(&clients_lock);
}

void receive_clientsent_file(int conn, char *file_name) {
    char buffer[MAX];
    char file_path[MAX];

    snprintf(file_path, sizeof(file_path), "txtfiles/%s", file_name);
    FILE *file = fopen(file_path, "wb");
    if (file == NULL) {
        printf("Could not create file on server.\n");
        return;
    }

    write(conn, "ACK", 3);

    while (1) {
        bzero(buffer, sizeof(buffer));
        int bytes_read = read(conn, buffer, sizeof(buffer));
        if (bytes_read <= 0 || strncmp(buffer, "EOF", 3) == 0) {
            break;
        }

        fwrite(buffer, 1, bytes_read, file);
    }

    fclose(file);
    printf("File %s received successfully.\n", file_name);
}

void *send_requested_file(int conn, char *file_name) {
    char buffer[MAX];
    char file_path[MAX];

    snprintf(file_path, sizeof(file_path), "txtfiles/%s", file_name);
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        printf("File not found: %s\n", file_path);
        return NULL;
    }

    while (!feof(file)) {
        int bytes_read = fread(buffer, 1, sizeof(buffer), file);
        if (bytes_read > 0) {
            int bytes_sent = write(conn, buffer, bytes_read);
            printf("Sent %d bytes to client.\n", bytes_sent);
            if (bytes_sent < 0) {
                perror("Failed to send data");
                break;
            }
        } else if (ferror(file)) {
            perror("Failed to read file");
            break;
        }
    }

    // Inform client that file transfer is complete
    write(conn, "EOF", 3);
    fclose(file);
    printf("File %s sent successfully.\n", file_name);
    return NULL;
}


void *medium(void* client_info_ptr) 
{ 
    char buff[MAX]; 
    int n; 

    client_info *client = (client_info *)client_info_ptr;
    int conn = client->conn;
    int client_id = client->client_id;

    printf("Handling client with ID: %d\n", client_id);
    
    // Infinite loop for communication 
    for (;;) { 
        bzero(buff, MAX); 
        int bytes_read = read(conn, buff, sizeof(buff)); 
        if (bytes_read <= 0) {
            printf("Client %d disconnected or error occurred...\n", client_id);
            break;
        }

        // TODO: implement ability to broadcast encoded messages server side.
        if (strncmp(buff, "CHAT ", 5) == 0) { // OR ENCODE/CIPHER (for implementation server side)
            printf("From client %d: %s", client_id, buff); // Seems to have prepended 5 bytes before the CHAT. Will need to check out   
            //(NOTE: POSSIBLE FIX: CLIENT PREPENDED CHAT BEFORE MSG, SO IT CAME AS CHAT CHAT, fixed clientside will test soon)
            broadcast_message(buff + 5, conn, client_id); // Shouldn't have to increment bytes by 10 here, should only be 5 (NOTE: see above)
        }

        else if (strncmp(buff, "ENCODE ", 7) == 0) { 
            encrypt(buff, 3);
            printf("From client %d: %s", client_id, buff); 
            broadcast_message(buff + 7, conn, client_id); // Shouldn't have to increment bytes by 10 here, should only be 5 (NOTE: see above)
        }
        
        else if (strncmp(buff, "LIST_FILES", 10) == 0) {
            send_file_list(conn);
        }

        else if (strncmp(buff, "SEND_FILE", 9) == 0) { // client sends file to server
            char *file_name = buff + 10;
            receive_clientsent_file(conn, file_name);
        }

        else if (strncmp(buff, "RECV_FILE", 9) == 0) { // server sends file to client
            char *file_name = buff + 10;
            send_requested_file(conn, file_name);
        }

        else if (strncmp(buff, "EXIT", 4) == 0) {
            printf("Client %d has left the chat...\n", client_id);
            break; 
        }

        else {
            printf("Unknown command from client %d: %s\n", client_id, buff);
        }
    }
         
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < client_count; i++) {
        if (clients[i]->conn == conn) {
            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_lock);

    close(conn);
    free(client);
    return NULL;
}


// Main driver function
void start_chatroom_serverside(int port) 
{ 
    int sockfd, conn, len; 
    struct sockaddr_in servaddr, cli; 
    int opt = 1;

    pthread_mutex_init(&clients_lock, NULL);
   
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
   
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(PORT); 
   
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) { 
        printf("socket bind failed...\n"); 
        exit(0); 
    } 
    else
        printf("Socket successfully binded..\n"); 
   
    if ((listen(sockfd, 5)) != 0) { 
        printf("Listen failed...\n"); 
        exit(0); 
    } 
    else
        printf("Server listening..\n"); 
    len = sizeof(cli); 
   
    while (1) {
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
    }



int main() {
    start_chatroom_serverside(PORT);
    return 0;
}