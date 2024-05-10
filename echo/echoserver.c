#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFSIZE 1024

#define USAGE                                                        \
    "usage:\n"                                                         \
    "  echoserver [options]\n"                                         \
    "options:\n"                                                       \
    "  -p                  Port (Default: 37482)\n"                    \
    "  -m                  Maximum pending connections (default: 5)\n" \
    "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"port",          required_argument,      NULL,           'p'},
    {"help",          no_argument,            NULL,           'h'},
    {"maxnpending",   required_argument,      NULL,           'm'},
    {NULL,            0,                      NULL,             0}
};


int main(int argc, char **argv) {
    int option_char;
    int portno = 37482; /* port to listen on */
    int maxnpending = 5;
  
    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "p:m:hx", gLongOptions, NULL)) != -1) {
        switch (option_char) {
        case 'm': // server
            maxnpending = atoi(optarg);
            break; 
        case 'h': // help
            fprintf(stdout, "%s ", USAGE);
            exit(0);
            break;
        case 'p': // listen-port
            portno = atoi(optarg);
            break;                                        
        default:
            fprintf(stderr, "%s ", USAGE);
            exit(1);
        }
    }

    setbuf(stdout, NULL); // disable buffering

    if ((portno < 1025) || (portno > 65535)) {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }
    if (maxnpending < 1) {
        fprintf(stderr, "%s @ %d: invalid pending count (%d)\n", __FILE__, __LINE__, maxnpending);
        exit(1);
    }


    /* Socket Code Here */

    // Create a socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Cannot create socket");
        exit(1);
    }

    // Initialize server address structure
    struct sockaddr_in server_address;
    int nAddressSize=sizeof(struct sockaddr_in);
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(portno);
    server_address.sin_addr.s_addr = INADDR_ANY;
    
    // Set SO_REUSEADDR on a socket to true
    int yes = 1;
    int reuse_status = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (reuse_status < 0) {
        perror("setsockopt");
        exit(1);
    }

    // Bind the socket to the server address
    int bind_status = bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address));
    if (bind_status < 0) {
        perror("Bind failed");
        exit(1);
    }

    // Listen for incoming connections
    int listen_status = listen(server_socket, maxnpending);
    if (listen_status < 0) {
        perror("Listen failed");
        exit(1);
    }


    // Continously accepting connection
    while (1) {
        // Accept a connection
        int client_socket;
        //struct sockaddr_in client_addr;
        //socklen_t client_len = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr*) &server_address, (socklen_t *)&nAddressSize);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        char buffer[BUFSIZE];
        memset(buffer, 0, BUFSIZE);
        
        // Read message from client
        int read_status = read(client_socket, buffer, BUFSIZE - 1);
        if (read_status < 0) {
            perror("Read failed");
            continue;
        }

        printf(buffer);

        // Echo message back to client
        int write_status = write(client_socket, buffer, strlen(buffer));
        if (write_status < 0) {
            perror("Write failed");
            continue;
        }

        // Close the connection
        int close_status = close(client_socket);
        if (close_status < 0) {
            perror("Close failed");
            continue;
        }
    }

    // Close the listening socket
    close(server_socket);

}
