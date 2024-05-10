#include "gfserver-student.h"

// Modify this file to implement the interface specified in
 // gfserver.h.

// define statements
#define BUFSIZE 512
#define PATH_BUFFER_SIZE 512
#define GF_STATUS_OK_MSG "GETFILE OK "
#define GF_STATUS_NOT_FOUND_MSG "GETFILE FILE_NOT_FOUND \r\n\r\n"
#define GF_STATUS_ERROR_MSG "GETFILE ERROR \r\n\r\n"
#define GF_STATUS_INVALID_MSG "GETFILE INVALID\r\n\r\n"
#define GF_LINE_END "\r\n\r\n"


/* Define GetFile context data structure. */
struct gfcontext_t {
    // client context
    int socket_fd; // file desrciptor of the client socket
    size_t file_length; // length of the file in context
};

void gfs_abort(gfcontext_t **ctx){
    close((*ctx)->socket_fd);
}

ssize_t gfs_send(gfcontext_t **ctx, const void *data, size_t len){
    /* Keep sending data to client in context until the required length of bytes is sent. Returns the total bytes sent at the end. */

    // bytes tracker
    ssize_t total_bytes_sent = 0;

    // bytes sending loop
    while (total_bytes_sent < len) {
        // send data to the client
        int remaining_length = len - total_bytes_sent;
        int current_bytes_sent = send((*ctx)->socket_fd, total_bytes_sent + data, remaining_length, 0);

        // check current send
        if (current_bytes_sent <= 0) {
            // perror("fail to send byte");
            break;
        }

        // update total bytes sent
        total_bytes_sent += current_bytes_sent;
    }
    
    return total_bytes_sent;
}

ssize_t gfs_sendheader(gfcontext_t **ctx, gfstatus_t status, size_t file_len) {
    /*  Sends the header depending on the status. 
        If FILE_NOT_FOUND, send "GETFILE FILE_NOT_FOUND \r\n\r\n"; 
        If ERROR, send "GETFILE ERROR \r\n\r\n"; 
        If INVALID, send "GETFILE INVALID \r\n\r\n";
        If OK, send "GETFILE OK %zu \r\n\r\n" and set context file length.
        Returns the total bytes send at the end.
        */

    ssize_t total_bytes_sent = 0;
    char response[BUFSIZE];

    switch (status) {
        case GF_FILE_NOT_FOUND:
            snprintf(response, sizeof(response), "%s", GF_STATUS_NOT_FOUND_MSG);
            break;
        case GF_OK:
            (*ctx)->file_length = file_len;
            int bytes_written = snprintf(response, sizeof(response), "%s%zu%s", GF_STATUS_OK_MSG, file_len, GF_LINE_END);
            if (bytes_written < 0 || bytes_written >= sizeof(response)) {
                // Handle error: snprintf writing error or not enough space in response buffer
                return -1;
            }
            break;
        case GF_ERROR:
            snprintf(response, sizeof(response), "%s", GF_STATUS_ERROR_MSG);
            break;
        default:
            // Handle unknown status case
            return -1;
    }

    total_bytes_sent = send((*ctx)->socket_fd, response, strlen(response), 0);
    return total_bytes_sent;
}

/* Define GetFile server data stucture. */
struct gfserver_t {
    // Server fields
    unsigned short port; // port to connect
    int max_npending; // max number of server pending
    int client_socket; // socket file descriptor of the client
    int server_socket; // socket file descriptor of the server
    socklen_t client_length; // length of the client

    // File processing
    size_t bytes_received; // bytes received from the client
    char *request; // request received from the client
    char *buffer; // files buffer received from the client

    // Callbacks
    gfh_error_t (*handler)(gfcontext_t **, const char *, void*); // server handler
    void* handlerarg; // handler arguments
};

gfserver_t *gfserver_create(){
    gfserver_t *gfs = (gfserver_t *) malloc(sizeof(gfserver_t));

    // Set default NULL values
    memset(gfs, '\0', sizeof(gfserver_t));

    // initiate the getfile server fields
    gfs->bytes_received = 0;
    gfs->request = (char *) malloc(BUFSIZE);
    gfs->buffer = (char *) malloc(BUFSIZE);

    return gfs;
}

void gfserver_set_port(gfserver_t **gfs, unsigned short port){
    (*gfs)->port = port;
}

/*  Validates the request and returns the status. */
int validate_request(gfserver_t **gfs) {
    
    // initiate empty buffer
    memset((*gfs)->buffer, '\0', BUFSIZE);


    // set the form of request path and header
    int scheme_valid;
    int method_valid;
    int path_valid;

    // error handling
    char *scheme = strtok((*gfs)->buffer, " ");
    if (scheme != NULL) {
        scheme_valid = strcmp(scheme, "GETFILE");
    } else {
        scheme_valid = -1;
    }
    char *method = strtok(NULL, " ");
    if (method != NULL) {
        method_valid = strcmp(method, "GET");
    } else {
        method_valid = -1;
    }
    char *path = strtok(NULL, "\r\n\r\n");
    if (path != NULL) {
        path_valid = strcmp(path, "/");
    } else {
        path_valid = -1;
    }

    // check the request to see if any errors that invalidate the request
    if (!(scheme_valid == 0 && method_valid == 0 && path_valid == 0)) {
        send((*gfs)->client_socket, "GETFILE INVALID\r\n\r\n", 19, 0);
        return -1;
    }

    // create new context wihtin the server
    gfcontext_t *context = malloc(sizeof(gfcontext_t));

    // set client within context
    (*context).socket_fd = (*gfs)->client_socket;

    // apply handler to the context
    (*gfs)->handler(&context, strtok((*gfs)->buffer, "\r\n\r\n"), (*gfs)->handlerarg);

    // memory leak release
    free(context);
    return -1;
}

// Helper function to initialize the server socket
int initialize_server_socket(gfserver_t **gfs) {
    struct addrinfo hints, *server_info, *p;
    int server_socket_fd;
    int yes = 1;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    char port[10];
    snprintf(port, sizeof(port), "%d", (*gfs)->port);

    if (getaddrinfo(NULL, port, &hints, &server_info) != 0) {
        perror("getaddrinfo failed");
        return -1;
    }

    for (p = server_info; p != NULL; p = p->ai_next) {
        server_socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (server_socket_fd < 0) continue;

        setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(server_socket_fd, p->ai_addr, p->ai_addrlen) == 0) break;

        // close(server_socket_fd);
    }

    if (p == NULL) {
        fprintf(stderr, "Server: failed to bind\n");
        freeaddrinfo(server_info);
        return -1;
    }

    freeaddrinfo(server_info);

    if (listen(server_socket_fd, (*gfs)->max_npending) < 0) {
        perror("listen");
        // close(server_socket_fd);
        return -1;
    }

    return server_socket_fd;
}

/*  
    Process the request received from the client as follows and returns the status:
    1. receive request from the client and store them into a buffer
    2. validate the request and respond accordingly in the form of <scheme> <method> <path>\r\n\r\n
    3. send the context through handler 
*/
void process_request(int client_socket_fd, gfserver_t *gfs) {
    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    ssize_t bytes_rcvd = recv(client_socket_fd, buffer, BUFSIZE, 0);

    if (bytes_rcvd <= 0) return;

    char *scheme = strtok(buffer, " ");
    char *method = strtok(NULL, " ");
    char *path = strtok(NULL, "\r\n\r\n");

    if (strcmp(scheme, "GETFILE") != 0 || strcmp(method, "GET") != 0 || !path || strcmp(path, "/") != 0) {
        send(client_socket_fd, "GETFILE INVALID\r\n\r\n", 20, 0);
    } else {
        gfcontext_t *context = malloc(sizeof(gfcontext_t));
        context->socket_fd = client_socket_fd;
        gfs->handler(&context, path, gfs->handlerarg);
    }
}

void gfserver_serve(gfserver_t **gfs){
    /*  Based on Beej's Ch5 Implementations 
        Step 1: Create necessary data structure needed for the server to connect to the client e.g. address, server socket, client socket
        Step 2: Connect the server continuously as desired: generate the server socket, bind the server socket
        Step 3: Listen to the server socket
        Step 4: Reciving and send file in chunks to the client socket
        Step 4.1: Generate the client socket by accpeting connection with the server socket with client address continously
        Step 4.2: Process the request coming from the client socket by receiving it 
        Step 4.3: Validate the request status and send the desired context by handler 
        */

    // /* Socket Code Here */

    // create data structure for server connection 
    // based on Beej's "5.1 getaddrinfo() — Prepare to launch!""
    int server_socket;
    struct addrinfo hints, *res, *p;
    memset(&hints, '\0' , sizeof(hints));
    hints.ai_family = AF_UNSPEC; // could have used AF_INET or AF_INET6 to force version. Here it works for both 
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    int reuse_trigger = 1; // for resuable address

    // Convert port to string
    char port_str[10];
    sprintf(port_str, "%u", (*gfs)->port);
    int address_status = getaddrinfo(NULL, port_str, &hints, &res);
    if (address_status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(address_status));
        exit(1);
    }

    // loop to connect
    for(p = res; p != NULL; p = p->ai_next) {

        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket < 0) {
            perror("fail to create server socket");
            exit(1);
        }
        int reuse_status = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse_trigger, sizeof(int));
        if (reuse_status < 0) {
            perror("fail to reuse server address");
            exit(1);
        }
        int bind_status = bind(server_socket, p->ai_addr, p->ai_addrlen);
        if (bind_status < 0) {
            continue;
        }
        
        break; // connected
    }

    freeaddrinfo(res); // free memory leak

    // start listening
    if (!server_socket) {
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
    }
    int listen_status = listen(server_socket, (*gfs)->max_npending);
	if (listen_status < 0) {
        perror("fail to start listening");
        exit(1);
    }
    
    // client + receive/send file in chunks 
    struct sockaddr_storage client_address;   
    socklen_t client_length;
    size_t bytes_received= 0;
    char buffer[BUFSIZE];
    int client_socket;
    
    // infinite loop: server should not terminate after sending its first response; rather, it should prepare to serve another request
    while(true) {


        // connect to the client
        client_length = sizeof(client_address);
        client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_length);
        if (client_socket < 0) {
            perror("fail to connect server socket");
            exit(1);
        }

        // new context 
        gfcontext_t *context = malloc(sizeof(gfcontext_t)); // Allocate context outside the loop
        if (!context) {
            perror("fail to allocate memory for context");
            exit(1);
        }
        

        context->socket_fd = client_socket;

        // process the request 
        while (true) {
            memset(buffer, '\0', BUFSIZE);
            bytes_received = recv(client_socket, buffer, BUFSIZE,0);

            // check receiving error
            if (bytes_received <= 0) {
                break;
            }
                 
            // request form: <scheme> <method> <path>\r\n\r\n
            char *scheme = strtok(buffer," "); // request scheme
            char *method = strtok(NULL," "); // request method
            char *path = strtok(NULL, GF_LINE_END); // request path 

            // checking possible errors that invalidate the request 
            if(strcmp(scheme, "GETFILE") != 0 || strcmp(method, "GET") != 0 || (path != NULL && strcmp(path, "/") != 0)){
                send(client_socket, GF_STATUS_INVALID_MSG, strlen(GF_STATUS_INVALID_MSG), 0);
                break;
            }

            
            (*gfs)->handler(&context, path, (*gfs)->handlerarg);
            
            break;
        }
        free(context);
    }
    free(*gfs);
}

void gfserver_set_handlerarg(gfserver_t **gfs, void* arg){
    (*gfs)->handlerarg = arg;
}

void gfserver_set_handler(gfserver_t **gfs, gfh_error_t (*handler)(gfcontext_t **, const char *, void*)){
    (*gfs)->handler = handler;
}

void gfserver_set_maxpending(gfserver_t **gfs, int max_npending){
    (*gfs)->max_npending = max_npending;
}

