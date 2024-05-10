#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <getopt.h>
#include <sys/socket.h>

#define BUFSIZE 512

#define USAGE                                                \
  "usage:\n"                                                 \
  "  transferclient [options]\n"                             \
  "options:\n"                                               \
  "  -p                  Port (Default: 17485)\n"            \
  "  -s                  Server (Default: localhost)\n"      \
  "  -h                  Show this help message\n"           \
  "  -o                  Output file (Default cs6200.txt)\n" 

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"server", required_argument, NULL, 's'},
    {"output", required_argument, NULL, 'o'},
    {"help", no_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}};

/* Main ========================================================= */
int main(int argc, char **argv)
{
    int option_char = 0;
    char *hostname = "localhost";
    unsigned short portno = 17485;
    char *filename = "cs6200.txt";

    setbuf(stdout, NULL);

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "s:p:o:hx", gLongOptions, NULL)) != -1) {
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
        case 'o': // filename
            filename = optarg;
            break;
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
        }
    }

    if (NULL == hostname) {
        fprintf(stderr, "%s @ %d: invalid host name\n", __FILE__, __LINE__);
        exit(1);
    }

    if (NULL == filename) {
        fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
        exit(1);
    }

    if ((portno < 1025) || (portno > 65535)) {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
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

    // Initiate and configure server
    //struct hostent *server;
    //server = gethostbyname(hostname);
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address)); // Initializes the servaddr structure to zero using memset.
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(portno); // Port number from earlier
    // bcopy((char *)server->h_addr, (char *)server_address.sin_addr.s_addr, server->h_length);

    // Connect to server
    int connection_status = connect(network_socket, (struct sockaddr *) &server_address, sizeof(server_address));
    if (connection_status < 0) {
        perror("Connect failed with the remote socket");
        exit(1);
    }

    // Buffer for data receiving
    char buffer[BUFSIZE];
    int bytes_received;
    int total_bytes;

    // Create and open a file to save buffer
    #include <fcntl.h>
    mode_t mode = S_IRUSR | S_IWUSR;
    int file_status = open(filename, O_WRONLY | O_CREAT, mode);
    if (file_status < 0) {
        perror("Failed to create a file");
        exit(1);
    }

    bzero(buffer, BUFSIZE);
    // Receiving data in a loop
    while ((bytes_received = recv(network_socket, buffer, BUFSIZE, 0)) > 0) {
        
        total_bytes += bytes_received; 
        if (bytes_received < 1) {
            perror("ERROR receiving from socket");
            break;
        } else {
            // strlen(buffer)
            write(file_status, buffer, bytes_received);
            bzero(buffer, BUFSIZE);
        }
    }

    printf("Total bytes received %d", total_bytes);

    /* Close file and socket */
    close(file_status);
    close(network_socket);

    return 0;
    /* Socket Code End */
}
