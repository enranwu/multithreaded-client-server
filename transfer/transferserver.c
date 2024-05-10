#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <getopt.h>
#include <sys/socket.h>

#define BUFSIZE 512

#define USAGE                                                \
    "usage:\n"                                               \
    "  transferserver [options]\n"                           \
    "options:\n"                                             \
    "  -f                  Filename (Default: 6200.txt)\n"   \
    "  -p                  Port (Default: 17485)\n"          \
    "  -h                  Show this help message\n"         \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"filename", required_argument, NULL, 'f'},
    {"help", no_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}};

int main(int argc, char **argv)
{
    int option_char;
    int portno = 17485;             /* port to listen on */
    char *filename = "6200.txt"; /* file to transfer */

    setbuf(stdout, NULL); // disable buffering

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "p:hf:x", gLongOptions, NULL)) != -1) {
        switch (option_char) {
        case 'p': // listen-port
            portno = atoi(optarg);
            break;
        case 'f': // file to transfer
            filename = optarg;
            break;
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
        default:
            fprintf(stderr, "%s", USAGE);
            exit(1);
        }
    }


    if ((portno < 1025) || (portno > 65535)) {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }
    
    if (NULL == filename) {
        fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
        exit(1);
    }

    /* Socket Code Here */

    // Create a socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Cannot create socket");
        exit(1);
    }

    // Set SO_REUSEADDR on a socket to true
    int yes = 1;
    int reuse_status = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (reuse_status < 0) {
        perror("setsockopt");
        exit(1);
    }

    // Initialize server address structure
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(portno);
    server_address.sin_addr.s_addr = INADDR_ANY;
    

    // Bind the socket to the server address
    int bind_status = bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address));
    if (bind_status < 0) {
        perror("Bind failed");
        exit(1);
    }

    // Listen for incoming connections
    int maxnpending = 5;
    int listen_status = listen(server_socket, maxnpending);
    struct sockaddr_in client_address;
    int client_address_size = sizeof(client_address);
    if (listen_status < 0) {
        perror("Listen failed");
        exit(1);
    }

    // Initiate file message holder
    char buffer[BUFSIZE];
    int total_bytes;

    // Continously accepting connection
    while (1) {
        // Accept a connection
        int client_socket;
        //socklen_t client_len = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr*) &client_address, (socklen_t *) &client_address_size);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        // Reset the bytes and buffer
        total_bytes = 0;
        memset(buffer, 0, BUFSIZE);

        // Open a file 
        FILE *file = fopen(filename, "r+");
        if (file < 0) {
            perror("Open file failed");
            exit(1);
        }

        // Reading file and sending data in a loop
        int bytes_read;

        while ((bytes_read = fread(buffer, 1, BUFSIZE, file)) > 0) {
            if (bytes_read < 1) {
                perror("Read file failed");
                continue;
            }
            printf("bytes sent here: %d", bytes_read);
            int total_sent = 0;
            while (total_sent < bytes_read) {
                int bytes_sent = send(client_socket, buffer + total_sent, bytes_read - total_sent, 0);
                total_sent += bytes_sent;
                total_bytes += bytes_sent;
            }
        }

        // printf("Total bytes send: %d", total_bytes);     

        // Display the file message
        

        // // Echo message back to client
        // ssize_t n = write(client_socket, buffer, strlen(buffer));
        // if (n < 0) {
        //     perror("Write failed");
        //     // close(client_socket);
        //     continue;
        // }

        // Close the connection
        fclose(file);
        int close_status = close(client_socket);
        if (close_status < 0) {
            perror("Close failed");
            continue;
        }
    }

    // Close the listening socket
    // close(server_socket);

    // int listenfd, connfd;
    // struct sockaddr_in serveraddr, clientaddr;
    // socklen_t clientlen = sizeof(clientaddr);

    // /* File handling */
    // int filefd = open(filename, O_RDONLY);
    // if (filefd < 0)
    //     error("ERROR opening file");

    // /* Listening for connections */
    // listenfd = ... // Server socket setup code
    // ...

    // while (1) {
    //     /* Accepting a connection */
    //     connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
    //     if (connfd < 0)
    //         error("ERROR on accept");

    //     /* Reading file and sending data in a loop */
    //     char buf[BUFSIZE];

    //     while ((bytes_read = read(filefd, buf, BUFSIZE)) > 0) {
    //         total_sent = 0;
    //         while (total_sent < bytes_read) {
    //             bytes_sent = send(connfd, buf + total_sent, bytes_read - total_sent, 0);
    //             if (bytes_sent < 0)
    //                 error("ERROR sending to socket");
    //             total_sent += bytes_sent;
    //         }
    //     }

    //     if (bytes_read < 0) {
    //         error("ERROR reading from file");
    //     }

    //     /* Close connection */
    //     close(connfd);
    // }

    return 0;
    
    /* Socket Code End */
}
