#include <stdlib.h>

#include "gfserver-student.h"

#define USAGE                                                                                     \
  "usage:\n"                                                                                      \
  "  gfserver_main [options]\n"                                                                   \
  "options:\n"                                                                                    \
  "  -h                  Show this help message.\n"                                               \
  "  -t [nthreads]       Number of threads (Default: 16)\n"                                       \
  "  -m [content_file]   Content file mapping keys to content files (Default: content.txt\n"      \
  "  -p [listen_port]    Listen port (Default: 39474)\n"                                          \
  "  -d [delay]          Delay in content_get, default 0, range 0-5000000 "                       \
  "(microseconds)\n "

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"content", required_argument, NULL, 'm'},
    {"port", required_argument, NULL, 'p'},
    {"nthreads", required_argument, NULL, 't'},
    {"delay", required_argument, NULL, 'd'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

extern unsigned long int content_delay;

extern gfh_error_t gfs_handler(gfcontext_t **ctx, const char *path, void *arg);

static void _sig_handler(int signo) {
  if ((SIGINT == signo) || (SIGTERM == signo)) {
    exit(signo);
  }
}

// global varibles to use
char *content_map = "content.txt";
gfserver_t *gfs = NULL;
int nthreads = 16;
unsigned short port = 39474;
int option_char = 0;
pthread_mutex_t mutex =  PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond =  PTHREAD_COND_INITIALIZER;
steque_t* work_queue;

/*
 * Worker thread routine to handle file sending requests from a queue.
 * This function continuously processes requests from a global queue, sending
 * files to clients. It waits on a condition variable if the queue is empty. For
 * each request, it attempts to open the requested file, read its contents, and
 * send it to the client. It handles file not found and error scenarios by sending
 * appropriate headers. It ensures thread safety by locking and unlocking a mutex
 * around queue operations.
 */
void *thread_handle_req(void *arg) {
  // Explicitly mark unused parameter to avoid compiler warnings.
  (void)arg;

  // Initialize buffer for file reading.
  char buffer[BUFSIZE];

  // Declare variables for file information and request handling.
  struct stat file_info;
  steque_request *request;

  // Enter an infinite loop to continuously process requests.
  while (true) {
    // Lock the mutex to safely access the shared queue.
    if (pthread_mutex_lock(&mutex) != 0)
      exit(12); // Exit if mutex lock fails.

    // Wait for the queue to be non-empty.
    while (steque_isempty(work_queue))
      pthread_cond_wait(&cond, &mutex);

    // Pop a request from the queue.
    request = steque_pop(work_queue);

    // Unlock the mutex after accessing the queue.
    if (pthread_mutex_unlock(&mutex) != 0)
      exit(29); // Exit if mutex unlock fails.

    // Signal other threads that they may proceed to handle requests.
    pthread_cond_signal(&cond);

    // Attempt to get the file descriptor for the requested file.
    int file_descriptor = content_get(request->filepath);
    if (file_descriptor == -1) {
      // Send file not found header if file cannot be opened.
      gfs_sendheader(&request->context, GF_FILE_NOT_FOUND, 0);
      //free(req);
      continue;
    }

    // Get file statistics; check for errors.
    if (fstat(file_descriptor, &file_info) == -1) {
      // Send error header if file stats cannot be obtained.
      gfs_sendheader(&request->context, GF_ERROR, 0);
      close(file_descriptor); // Close the file descriptor.
      //free(req); // Free request memory.
      continue;
    }

    // Send OK header with file size if file is successfully opened and stats obtained.
    gfs_sendheader(&request->context, GF_OK, file_info.st_size);

    // Initialize variables for sending file content.
    size_t total_bts_sent = 0;
    ssize_t bts_read = 0;
    // Continue until entire file is sent.
    while (total_bts_sent < (size_t)file_info.st_size) { 
      // Clear buffer and read a chunk of the file.
      memset(buffer, '\0', BUFSIZE);
      bts_read = pread(file_descriptor, buffer, BUFSIZE, total_bts_sent);
      if (bts_read <= 0) break; // Break loop if read error or end of file.

      // Send the read chunk to client.
      ssize_t bts_sent = gfs_send(&request->context, buffer, bts_read);
      total_bts_sent += bts_sent; // Update total bytes sent.
    }

    // Cleanup: free the request memory.
    free(request);
  }
  free(request);
  // Function signature requires return statement; return NULL for pthread compatibility.
  return NULL;
}


/*
 * Initializes a mutex and creates a specified number of worker threads.
 * This function initializes a global mutex used for synchronization across threads.
 * It then dynamically allocates memory for storing thread identifiers and creates
 * 'nthreads' worker threads, each executing the 'thread_handle_req' function.
 * If any step fails, the function exits with a specific error code, default 1.
 */
void set_pthreads(size_t nthreads) {
  // Initialize mutex
  int res = pthread_mutex_init(&mutex, NULL);
  if (res != 0) {
    exit(1); // Specific exit code for mutex initialization failure
  }

  // Dynamically allocate memory for thread identifiers
  pthread_t* threads = malloc(nthreads * sizeof(pthread_t));
  if (threads == NULL) {
    // Clean up resources and exit if memory allocation fails
    pthread_mutex_destroy(&mutex);
    exit(EXIT_FAILURE); // Use standard exit code for allocation failure
  }

  // Create worker threads
  for (size_t i = 0; i < nthreads; i++) {
    res = pthread_create(&threads[i], NULL, thread_handle_req, NULL);
    if (res != 0) {
      // Clean up allocated memory and initialized mutex before exiting
      free(threads);
      pthread_mutex_destroy(&mutex);
      exit(1); // Specific exit code for thread creation failure
    }
  }

  // Free the allocated memory for thread identifiers
  // Assuming threads are either joined or detached later in the program
  free(threads);
}

/* Main ========================================================= */
int main(int argc, char **argv) {
  // commenting out to use globally
  // char *content_map = "content.txt";
  // gfserver_t *gfs = NULL;
  // int nthreads = 16;
  // unsigned short port = 39474;
  // int option_char = 0;

  setbuf(stdout, NULL);

  if (SIG_ERR == signal(SIGINT, _sig_handler)) {
    fprintf(stderr, "Can't catch SIGINT...exiting.\n");
    exit(EXIT_FAILURE);
  }

  if (SIG_ERR == signal(SIGTERM, _sig_handler)) {
    fprintf(stderr, "Can't catch SIGTERM...exiting.\n");
    exit(EXIT_FAILURE);
  }

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:d:rhm:t:", gLongOptions,
                                    NULL)) != -1) {
    switch (option_char) {
      case 'h':  /* help */
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;
	  case 'p':  /* listen-port */
        port = atoi(optarg);
        break;
      case 'd':  /* delay */
        content_delay = (unsigned long int)atoi(optarg);
        break;
      case 't':  /* nthreads */
        nthreads = atoi(optarg);
        break;
      case 'm':  /* file-path */
        content_map = optarg;
        break;
      default:
        fprintf(stderr, "%s", USAGE);
        exit(1);


    }
  }

  /* not useful, but it ensures the initial code builds without warnings */
  if (nthreads < 1) {
    nthreads = 1;
  }

  if (content_delay > 5000000) {
    fprintf(stderr, "Content delay must be less than 5000000 (microseconds)\n");
    exit(__LINE__);
  }

  content_init(content_map);

  /* Initialize thread management */
  work_queue = (steque_t*)malloc(sizeof(*work_queue));
  steque_init(work_queue);
  set_pthreads(nthreads);

  /*Initializing server*/
  gfs = gfserver_create();

  //Setting options
  gfserver_set_port(&gfs, port);
  gfserver_set_maxpending(&gfs, 24);
  gfserver_set_handler(&gfs, gfs_handler);
  gfserver_set_handlerarg(&gfs, NULL);  // doesn't have to be NULL!

  /*Loops forever*/
  gfserver_serve(&gfs);
}
