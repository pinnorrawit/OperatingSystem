#include <stdio.h>      // Standard I/O functions
#include <stdlib.h>     // Standard library functions
#include <unistd.h>     // Unix standard functions (sleep, close)
#include <sys/socket.h> // Socket programming functions
#include <netinet/in.h> // Internet address structures
#include <arpa/inet.h>  // Internet address conversion functions
#include <time.h>       // Time functions
#include <string.h>     // String manipulation functions
#include <pthread.h>    // Thread functions

#define PORT 6013           // Server port number
#define MAX_CLIENTS 10      // Max clients in connection queue

// Client information structure for thread parameters
typedef struct {
    int client_socket;          // Client socket descriptor
    struct sockaddr_in client_addr; // Client address information
} client_info_t;

// Thread function to handle each client connection
void* handle_client(void* arg) {
    client_info_t* info = (client_info_t*)arg;  // Cast argument to client info
    int client_socket = info->client_socket;    // Extract client socket

    // Print client connection info
    printf("Client connected from %s:%d\n",
           inet_ntoa(info->client_addr.sin_addr),  // Convert IP to string
           ntohs(info->client_addr.sin_port));     // Convert port from network to host byte order

    time_t last_sent = 0;  // Track when we last sent data

    // Main loop: send time to client every second
    while (1) {
        time_t now = time(NULL);  // Get current time

        // Only send if at least 1 second has passed since last send
        if (now - last_sent >= 1) {
            struct tm *tm_info = localtime(&now);       // Convert to local time struct
            char time_str[50];                          // Buffer for time string

            // Format time as YYYY-MM-DD HH:MM:SS with newline
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S\n", tm_info);

            // Send formatted time string to client
            int bytes_sent = send(client_socket, time_str, strlen(time_str), 0);

            // Check if send failed (client disconnected)
            if (bytes_sent <= 0) {
                printf("Client %s:%d disconnected\n",
                       inet_ntoa(info->client_addr.sin_addr),
                       ntohs(info->client_addr.sin_port));
                break;
            }

            last_sent = now; // Update last sent time
        }

        // Small delay to prevent CPU spinning (10ms)
        usleep(10000);
    }

    close(client_socket);   // Close client connection
    free(info);             // Free allocated memory
    return NULL;            // Thread return value
}

int main() {
    // Create server socket (IPv4, TCP, default protocol)
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);

    // Configure server address structure
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;           // IPv4 address family
    server_addr.sin_addr.s_addr = INADDR_ANY;   // Accept connections from any IP
    server_addr.sin_port = htons(PORT);         // Set port (convert to network byte order)

    // Bind socket to specified address and port
    bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // Listen for incoming connections (max 10 clients in queue)
    listen(server_socket, MAX_CLIENTS);

    // Server startup message
    printf("Server started on port %d. Waiting for connections...\n", PORT);

    // Main server loop: accept and handle client connections
    while (1) {
        struct sockaddr_in client_addr;         // Client address structure
        socklen_t client_len = sizeof(client_addr); // Length of client address structure

        // Accept incoming client connection (blocking call)
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);

        // Allocate memory for client information structure
        client_info_t* client_info = malloc(sizeof(client_info_t));
        client_info->client_socket = client_socket;    // Store client socket
        client_info->client_addr = client_addr;        // Store client address

        pthread_t thread_id;  // Thread identifier

        // Create new thread to handle this client
        pthread_create(&thread_id, NULL, handle_client, client_info);

        // Detach thread (resources automatically reclaimed when thread exits)
        pthread_detach(thread_id);
    }

    return 0;  // Program exit (unreachable in this code)
}
