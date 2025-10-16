#include <stdio.h>      // Standard input/output functions (printf)
#include <stdlib.h>     // Standard library functions (exit, malloc)
#include <unistd.h>     // POSIX OS API (close, read, write)
#include <sys/socket.h> // Socket programming interfaces
#include <netinet/in.h> // Internet address families and structures
#include <arpa/inet.h>  // IP address conversion functions
#include <string.h>     // String manipulation functions

#define PORT 6013           // Server port number to connect to
#define BUFFER_SIZE 60      // Size of data reception buffer

int main() {
    // Create TCP socket for IPv4 communication
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    
    // Configure server address structure
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;           // Use IPv4 address family
    serv_addr.sin_port = htons(PORT);         // Set port number (convert to network byte order)
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);  // Convert IP string to binary format (localhost)
    
    // Establish connection to the server
    connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    
    // Buffer to store received data from server
    char buffer[BUFFER_SIZE];
    
    // Infinite loop to continuously receive data
    while (1) {
        // Receive data from server (blocks until data arrives)
        int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        
        // If data was successfully received
        if (bytes_received > 0) {
            // Print received data to standard output
            printf("%s", buffer);
        }
    }
    
    return 0;  // Program termination (theoretically unreachable due to infinite loop)
}
