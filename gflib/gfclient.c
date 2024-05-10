
#include <stdlib.h>

#include "gfclient-student.h"

 // Modify this file to implement the interface specified in
 // gfclient.h.

// define buffsize
#define BUFSIZE 512
#define PATH_BUFFER_SIZE 512

// optional function for cleaup processing.
void gfc_cleanup(gfcrequest_t **gfr) {
  free(*gfr);
  *gfr = NULL;
}

/* Define GetFile Client Request Data Structure. */ 
struct gfcrequest_t {
  // Socket & Request
  int socket_fd; // socket file descriptor
  const char *server;  // server to connect
  unsigned short port; // file transfer port
  const char *path; // request path
  char *request; // request sent to the server
  char *response; // response received from the server
  size_t bytes_received; // number of bytes received from the server
  size_t file_length; // length of the file received from the server
  gfstatus_t status; // status of the request

  // Callbacks
  void (*headerfunc)(void *, size_t, void *); // header function
  void *headerarg; // header arguments
  void (*writefunc)(void *, size_t, void *); // write function
  void *writearg; // write arguments
};

gfcrequest_t *gfc_create() {
  // Initiate getfile request struct
  gfcrequest_t *gfr = malloc(sizeof(gfcrequest_t));

  // Sanity check
  if (gfr == NULL) {
    return NULL;
  }

  // Set default NULL values
  memset(gfr, '\0', sizeof(gfcrequest_t));

  // Initiate gfr fields
  gfr->file_length = 0;
  gfr->status = GF_INVALID;
  gfr->bytes_received = 0;
  gfr->headerfunc = NULL;
  gfr->headerarg = NULL;
  gfr->writefunc = NULL;
  gfr->writearg = NULL;

  return gfr;
}

size_t gfc_get_filelen(gfcrequest_t **gfr) {
  return (*gfr)->file_length;
}

size_t gfc_get_bytesreceived(gfcrequest_t **gfr) {
  return (*gfr)->bytes_received;
}

gfstatus_t gfc_get_status(gfcrequest_t **gfr) {
  return (*gfr)->status;
}

void gfc_global_init() {}

void gfc_global_cleanup() {}

// Additional helper functions 

/* Validates the status of the request. */
int validate_status(gfcrequest_t **gfr, const char *header_status) {
  if (strcmp(header_status, "OK") == 0) {
    (*gfr)->status = GF_OK;
    return 1;
  } else if (strcmp(header_status, "FILE_NOT_FOUND") == 0) {
    (*gfr)->status = GF_FILE_NOT_FOUND;
    return -1;
  } else if (strcmp(header_status, "INVALID") == 0) {
    (*gfr)->status = GF_INVALID;
    return -1;
  } else {
    (*gfr)->status = GF_ERROR;
    return -1;
  }
}

/* Parses the empty header as requested. */
int empty_header_parser(gfcrequest_t **gfr, const char *buffer, int current_bytes_received, size_t expected_len, int sscanf_result, bool header_received) {
  // check if no data is transferred
  if (current_bytes_received == 0) {
    (*gfr)->status = GF_ERROR;
    return -1;
  }

  char header_status[16]; // e.g. OK, FILE_NOT_FOUND, INVALID
  char *header_start = strstr(buffer, "GETFILE");
  char *header_end_temp = strstr(buffer, "\r\n\r\n");
  // char *header_end = strtok(buffer, "\r\n\r\n");

  // check if header is in the right form
  if (!(header_start && header_end_temp)) {
    (*gfr)->status = GF_INVALID;
    return -1;
  }
  
  // check file processing status and validate status along the way
  sscanf_result = sscanf(header_end_temp, "GETFILE %s %zu\r\n\r\n", header_status, &expected_len);
  if (sscanf_result != 2) {
    sscanf_result = sscanf(header_end_temp, "GETFILE %s\r\n\r\n", header_status);
  }
  if (sscanf_result == 2 || sscanf_result == 1) {
    header_received = true;
    validate_status(gfr, header_status);
    if (gfc_get_status(gfr) != GF_OK) {
      return 1;
    }
  }
  return 1;
}

/* Parses the given header. */
int header_parser(gfcrequest_t **gfr, size_t current_bytes_received, size_t expected_len) {
  // check early transfer closure
  if (current_bytes_received == 0 && (*gfr)->bytes_received < expected_len) {
    (*gfr)->status = GF_OK;
    return 1;
  }
  return -1;
}

int gfc_perform(gfcrequest_t **gfr) {
  // Based on Beej's Guide ch.5 implementation 
  // Steps: 
  // 1. Create a socket and connect to the server
  // 2. Send the Getfile request
  // 3. Receive the response header and call the header callback
  // 4. If the status is OK, receive the file content and call the write callback
  // 5. Handle different response statuses (OK, FILE_NOT_FOUND, ERROR, INVALID)
  // 6. Close the connection and cleanup

  // Create a client socket
  (*gfr)->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if ((*gfr)->socket_fd < 0) {
    perror("create client socket failed");
    exit(1);
  }

  // Create server structure
  struct addrinfo hints, *res, *p;
  memset(&hints, '\0', sizeof(hints));
  hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 if want to force version
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;

  // Set SO_REUSEADDR on a socket to true
  int reuse_trigger = 1;
  int reuse_status = setsockopt((*gfr)->socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_trigger, sizeof(reuse_trigger));
  if (reuse_status < 0) {
      perror("reuse port failed");
      exit(1);
  }

  // Convert port to string
  char port_str[10];
  sprintf(port_str, "%u", (*gfr)->port);
  int addr_status = getaddrinfo((*gfr)->server, port_str, &hints, &res);
  if (addr_status != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addr_status));
    return 2;
  }

  // Server connection loop
  for (p = res; p != NULL; p = p->ai_next) {
    int connection_status = connect((*gfr)->socket_fd, p->ai_addr, p->ai_addrlen);

    // Create a socket to connect
    if (connection_status == -1) {
      perror("reconnecting");
      close(((*gfr)->socket_fd));
      (*gfr)->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
      if ((*gfr)->socket_fd < 0) {
        perror("create socket failed");
        exit(1);
      }
      continue;
    }
    break;
  }

  // Clear the res memory
  freeaddrinfo(res);

  // Build the getfile request
  (*gfr)->request = (char *) malloc(BUFSIZE);
  sprintf((*gfr)->request, "GETFILE GET %s\r\n\r\n", (*gfr)->path);

  // Send loop for the getfile request
  ssize_t bytes_request = strlen((*gfr)->request);
  ssize_t total_bytes_sent = 0;
  while (total_bytes_sent < bytes_request) {
    ssize_t current_bytes_sent = send((*gfr)->socket_fd, (*gfr)->request, bytes_request, 0);
    if (current_bytes_sent < 0) {
      perror("send request to server failed");
      exit(1);
    }
    total_bytes_sent += current_bytes_sent;
  }

  // for memory leak reduce
  free((*gfr)->request); 

  printf("receiving getfile response");

  // File handling declarations for receiving 
  char buffer[BUFSIZE]; // buffer containing received data
  char content[BUFSIZE]; // buffer containing received content
  char header[BUFSIZE]; // buffer containing received header
  int current_bytes_received; // current bytes received
  bool header_received = false; // indicator of whether header is received or not
  int sscanf_result = 0; // request file processing result indicator
  size_t expected_len; // expected length of the input

  // Receive loop for the getfile response
  while (true) {
    
    // clear the buffers at the loop start
    memset(buffer, '\0', BUFSIZE);
    memset(content, '\0', BUFSIZE);
    memset(header, '\0', BUFSIZE);

    // receive data
    current_bytes_received = recv((*gfr)->socket_fd, buffer, BUFSIZE, 0);

    // populate the content by current bytes received
    memcpy(content, buffer, current_bytes_received);

    // update the response
    (*gfr)->response = buffer;
    
    // process header if not already received
    if (!header_received) {
      // header parsing
      // check if no data is transferred
      if (current_bytes_received == 0) {
        (*gfr)->status = GF_ERROR;
        break;
      }

      // general form of a response is <scheme> <status> <length>\r\n\r\n<content>
      char header_status[16]; // e.g. OK, FILE_NOT_FOUND, INVALID
      char *header_start = strstr(buffer, "GETFILE"); // scheme
      char *header_end_temp = strstr(buffer, "\r\n\r\n");
      char *header_end = strtok(buffer, "\r\n\r\n");

      // check if header is in the right form
      if (!(header_start && header_end_temp && header_end)) {
        (*gfr)->status = GF_INVALID;
        break;
      }
      
      // check file processing status and validate status along the way
      // Attempt to parse the header for a status and expected length using the "GETFILE %s %zu" format
      sscanf_result = sscanf(header_end, "GETFILE %s %zu\r\n\r\n", header_status, &expected_len);
      // If the first attempt fails, try to parse the header for just the status without the expected length
      if (sscanf_result != 2) {
        sscanf_result = sscanf(header_end, "GETFILE %s\r\n\r\n", header_status);
      }

      // Check if the header was successfully received either with both status and length, or just the status
      if (sscanf_result == 2 || sscanf_result == 1) {
        header_received = true; // Indicate that the header has been successfully received

        // Validate the status obtained from the header
        int valid_result = validate_status(gfr, header_status);

        // If the status is valid and we received the expected length, set the file length
        if (valid_result == 1) {
          (*gfr)->file_length = expected_len;
        } else {
          // If the status is not valid, break from the loop (or current processing block)
          break;
        }
        
        // Check the overall status of the GFC request; break if it's not GF_OK
        if (gfc_get_status(gfr) != GF_OK) {
          break;
        }
      }

      // populate header in the right format
      sprintf(header, "GETFILE %s %zu\r\n\r\n", header_status, gfc_get_filelen(gfr));

      size_t header_size = strlen(header); // size of the header

      // process any content in the current bytes
      int bytes_processing = current_bytes_received - header_size;
      (*gfr)->bytes_received += bytes_processing;

      // write content with the header
      if ((*gfr)->writefunc && gfc_get_status(gfr) == GF_OK) {
        (*gfr)->writefunc(content + header_size, bytes_processing, (*gfr)->writearg);
      }
      
      // process content if header already received
    } else if (header_received) {
      // check early transfer closure
      if (current_bytes_received == 0 && gfc_get_bytesreceived(gfr) < expected_len) {
        (*gfr)->status = GF_OK;
        break;
      }

      // write content without header
      (*gfr)->writefunc(content, current_bytes_received, (*gfr)->writearg);
      (*gfr)->bytes_received += current_bytes_received;
    }

    // check if successfully received the full file
    if (gfc_get_bytesreceived(gfr) == expected_len) {
      break;
    }
  }

  // debugger
  printf("Current getfile status is %d, bytes received is %lu, file length is %lu, sscanf_result is %d", gfc_get_status(gfr), gfc_get_bytesreceived(gfr), gfc_get_filelen(gfr), sscanf_result);

  // return the appropriate status code after performing the tasks
  if (gfc_get_status(gfr) == GF_ERROR || gfc_get_status(gfr) == GF_FILE_NOT_FOUND) {
    return 0;
  } else if ((gfc_get_status(gfr) == GF_OK) && (gfc_get_bytesreceived(gfr) == gfc_get_filelen(gfr))) {
    return 0;
  } else {
    return -1;
  }
}

void gfc_set_port(gfcrequest_t **gfr, unsigned short port) {
  (*gfr)->port = port;
}

void gfc_set_headerarg(gfcrequest_t **gfr, void *headerarg) {
  (*gfr)->headerarg = headerarg;
}

void gfc_set_server(gfcrequest_t **gfr, const char *server) {
  (*gfr)->server = server;
}

void gfc_set_headerfunc(gfcrequest_t **gfr, void (*headerfunc)(void *, size_t, void *)) {
  (*gfr)->headerfunc = headerfunc;
}

void gfc_set_path(gfcrequest_t **gfr, const char *path) {
  (*gfr)->path = path;
}

void gfc_set_writearg(gfcrequest_t **gfr, void *writearg) {
  (*gfr)->writearg = writearg;
}

void gfc_set_writefunc(gfcrequest_t **gfr, void (*writefunc)(void *, size_t, void *)) {
  (*gfr)->writefunc = writefunc;
}

const char *gfc_strstatus(gfstatus_t status) {
  const char *strstatus = "UNKNOWN";

  switch (status) {

    case GF_OK: {
      strstatus = "OK";
    } break;

    case GF_FILE_NOT_FOUND: {
      strstatus = "FILE_NOT_FOUND";
    } break;

   case GF_INVALID: {
      strstatus = "INVALID";
    } break;
   
   case GF_ERROR: {
      strstatus = "ERROR";
    } break;

  }

  return strstatus;
}
