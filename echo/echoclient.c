#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <getopt.h>

/* Be prepared accept a response of this length */
#define BUFSIZE 1024

#define USAGE                                                                       \
    "usage:\n"                                                                      \
    "  echoclient [options]\n"                                                      \
    "options:\n"                                                                    \
    "  -p                  Port (Default: 37482)\n"                                  \
    "  -s                  Server (Default: localhost)\n"                           \
    "  -m                  Message to send to server (Default: \"Hello Spring!!\")\n" \
    "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"server", required_argument, NULL, 's'},
    {"port", required_argument, NULL, 'p'},
    {"message", required_argument, NULL, 'm'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

/* Main ========================================================= */
int main(int argc, char **argv)
{
    int option_char = 0;
    unsigned short portno = 37482;
    char *hostname = "localhost";
    char *message = "Hello Spring!!";

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "s:p:m:hx", gLongOptions, NULL)) != -1) {
        switch (option_char) {
        case 's': // server
            hostname = optarg;
            break;
        case 'p': // listen-port
            portno = atoi(optarg);
            break;
        default:
            fprintf(stderr, "%s", USAGE);
            exit(1);
        case 'm': // message
            message = optarg;
            break;
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
        }
    }

    setbuf(stdout, NULL); // disable buffering

    if ((portno < 1025) || (portno > 65535)) {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }

    if (NULL == message) {
        fprintf(stderr, "%s @ %d: invalid message\n", __FILE__, __LINE__);
        exit(1);
    }

    if (NULL == hostname) {
        fprintf(stderr, "%s @ %d: invalid host name\n", __FILE__, __LINE__);
        exit(1);
    }

    /* Socket Code Here */

    // Create socket
    int network_socket;
    network_socket = socket(AF_INET, SOCK_STREAM, 0); //AF_INET: Specifies the IPv4 Internet protocols. SOCK_STREAM: Indicates a TCP socket (as opposed to SOCK_DGRAM for UDP).
    if (network_socket < 0) {
        perror("Cannot create socket");
        exit(1);
    }

    // Configure server address
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address)); // Initializes the servaddr structure to zero using memset.
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(portno); // Port number from earlier
    server_address.sin_addr.s_addr = INADDR_ANY;

    // Connect to server
    int connection_status = connect(network_socket, (struct sockaddr *) &server_address, sizeof(server_address));

    // Check for error with the connection
    if (connection_status < 0) {
        perror("Connect failed with the remote socket");
        exit(1);
    }

    // Send message
    int send_status = send(network_socket, message, strlen(message), 0);
    if (send_status < 0) {
        perror("Send failed");
        exit(1);
    }

    // Receive response
    char server_response[BUFSIZE];
    int n = recv(network_socket, &server_response, BUFSIZE, 0);
    if (n < 0) {
        perror("Receive failed");
        exit(1);
    }

    server_response[n] = '\0'; // Null-terminate the string
    printf(server_response);

    // Close socket
    close(network_socket);

    }
