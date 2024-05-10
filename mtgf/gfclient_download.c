#include <stdlib.h>

#include "gfclient-student.h"

/* Additional packages and define */
#include <pthread.h>
#include "steque.h"
#define BUFSIZE 512
/* End */

#define MAX_THREADS 1024
#define PATH_BUFFER_SIZE 512

#define USAGE                                                             \
  "usage:\n"                                                              \
  "  gfclient_download [options]\n"                                       \
  "options:\n"                                                            \
  "  -h                  Show this help message\n"                        \
  "  -s [server_addr]    Server address (Default: 127.0.0.1)\n"           \
  "  -p [server_port]    Server port (Default: 39474)\n"                  \
  "  -w [workload_path]  Path to workload file (Default: workload.txt)\n" \
  "  -t [nthreads]       Number of threads (Default 8 Max: 1024)\n"       \
  "  -n [num_requests]   Request download total (Default: 16)\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"help", no_argument, NULL, 'h'},
    {"server", required_argument, NULL, 's'},
    {"port", required_argument, NULL, 'p'},
    {"workload", required_argument, NULL, 'w'},
    {"nthreads", required_argument, NULL, 't'},
    {"nrequests", required_argument, NULL, 'n'},
    {NULL, 0, NULL, 0}};

static void Usage() { fprintf(stderr, "%s", USAGE); }

static void localPath(char *req_path, char *local_path) {
  static int counter = 0;

  sprintf(local_path, "%s-%06d", &req_path[1], counter++);
}

static FILE *openFile(char *path) {
  char *cur, *prev;
  FILE *ans;

  /* Make the directory if it isn't there */
  prev = path;
  while (NULL != (cur = strchr(prev + 1, '/'))) {
    *cur = '\0';

    if (0 > mkdir(&path[0], S_IRWXU)) {
      if (errno != EEXIST) {
        perror("Unable to create directory");
        exit(EXIT_FAILURE);
      }
    }

    *cur = '/';
    prev = cur;
  }

  if (NULL == (ans = fopen(&path[0], "w"))) {
    perror("Unable to open file");
    exit(EXIT_FAILURE);
  }

  return ans;
}

/* Callbacks ========================================================= */
static void writecb(void *data, size_t data_len, void *arg) {
  FILE *file = (FILE *)arg;
  fwrite(data, 1, data_len, file);
}

// global varibles
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER; 
steque_t* work_queue;
int nrequests_done = 0; 
unsigned short port = 39474;
int returncode = 0;
int nthreads = 8;
int nrequests = 14;
char *server = "localhost";

/**
 * Processes a main request to download a file from a server and save it locally.
 * 
 * This function handles the entire process of requesting a file from a specified server,
 * including setting up the request parameters (server address, file path, port, and callbacks),
 * initiating the request, and handling the response. It uses the Getfile library for the request
 * and response mechanism. If the request is successful, the file is saved locally. In case of
 * an error during the request or if the server response is not GF_OK, the local file is
 * unlinked (deleted). The function also prints relevant request and response information to stdout,
 * including the server and file path requested, the status of the request, and the amount of data
 * received.
 * 
 * @source main skeleton codes
 */
void main_request_process(char* filepath) {
  // Define variables for request handling and local file managemen
  gfcrequest_t* gfr;
  char local_path[BUFSIZE]; // Buffer to hold the local file path
  FILE *file = NULL; // File pointer for the local file

  // Convert the server filepath to a local path equivalent
  localPath(filepath, local_path);

  // Open the local file for writing the downloaded content
  file = openFile(local_path);

   // Create and initialize the GFC request
  gfr = gfc_create();
  gfc_set_server(&gfr, server); // Set the server addres
  gfc_set_path(&gfr, filepath); // Set the path of the file to request
  gfc_set_port(&gfr, port); // Set the server port
  gfc_set_writefunc(&gfr, writecb); // Set the callback function for writing data to the file
  gfc_set_writearg(&gfr, file); // Set the file pointer as the argument for the callback

  // Log the request details
  fprintf(stdout, "Requesting %s%s\n", server, filepath);

  // Perform the request and check for errors
  if (0 > (returncode = gfc_perform(&gfr))) {
     // If there was an error, log it and clean up
    fprintf(stdout, "gfc_perform returned an error %d\n", returncode);
    fclose(file); // Close the file
    // Attempt to delete the local file if there was an error
    if (0 > unlink(local_path))
      fprintf(stderr, "warning: unlink failed on %s\n", local_path);
  } else {
    // Close the file if the request was successful
    fclose(file);
  }

  // Check the status of the request and handle errors
  if (gfc_get_status(&gfr) != GF_OK) {
    // If the request was not successful, attempt to delete the local file
    if (0 > unlink(local_path))
      fprintf(stderr, "warning: unlink failed on %s\n", local_path);
  }

  // Log the status and the amount of data received
  fprintf(stdout, "Status: %s\n", gfc_strstatus(gfc_get_status(&gfr)));
  fprintf(stdout, "Received %zu of %zu bytes\n", gfc_get_bytesreceived(&gfr), gfc_get_filelen(&gfr));

  // Clean up the GFC request object
  gfc_cleanup(&gfr);

  /*
  * note that when you move the above logic into your worker thread, you will
  * need to coordinate with the boss thread here to effect a clean shutdown.
  */
}

/*
 * Entry point for worker threads responsible for handling requests.
 * Continuously check for and processes requests from a shared work queue, 
 * coordinating with other threads through synchronization primitives 
 * such as mutexes and condition variables
 */
void *thread_handle_req() {
  char *req_obj;

  while (1) {
    pthread_mutex_lock(&mutex);

    // Wait for work to be available or for all requests to be handled
    while (steque_isempty(work_queue) && nrequests_done < nrequests) {
      pthread_cond_wait(&cond, &mutex);
    }

    // Check if all requests have been processed
    if (nrequests_done >= nrequests) {
      pthread_mutex_unlock(&mutex);
      break; // Exit loop and thread
    }

    // Pop a request object from the queue if available
    if (!steque_isempty(work_queue)) {
      req_obj = steque_pop(work_queue);
      nrequests_done += 1; // Mark this request as being processed
    }

    pthread_mutex_unlock(&mutex);

    // Signal or broadcast that a request has been taken for processing
    pthread_cond_broadcast(&cond);

    if (req_obj) {
      main_request_process(req_obj); // Process the request
    }
  }

  return NULL; // Exit thread
}

/*
 * Creates and initializes a specified number of worker threads. 
 * Handle individual tasks, such as processing requests in a server application
 */
void create_worker_threads(pthread_t* threads){
    for (int i = 0; i < nthreads; i++) {
        // create thread
        int res = pthread_create(&threads[i], NULL, thread_handle_req, NULL);
        if (res != 0) {
            fprintf(stderr, "Failed to create thread %d: return code %d\n", i, res); // checks for thread creation error
            exit(EXIT_FAILURE);
        }
    }
}

/* Enqueues a predefined number of requests into a work queue. @source main skeleton code */
void enqueue_requests() {
  /* Build your queue of requests here */
  for (int i = 0; i < nrequests; i++) {
    /* Note that when you have a worker thread pool, you will need to move this
    * logic into the worker threads */
    char* req_path = workload_get_path();

    if (strlen(req_path) > PATH_BUFFER_SIZE) {
      fprintf(stderr, "Request path exceeded maximum of %d characters\n", PATH_BUFFER_SIZE);
      exit(EXIT_FAILURE);
    }

    pthread_mutex_lock(&mutex);
    steque_enqueue(work_queue, req_path);
    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&cond);
  }
}

/* Waits for all enqueued requests to be processed. */
void wait_for_requests_completion() {
  // boss waiting
  pthread_mutex_lock(&mutex);

  // conditional wait while nonempty queue
  while (nrequests_done != nrequests) {
    pthread_cond_wait(&cond, &mutex);
  }
  pthread_mutex_unlock(&mutex);
  pthread_cond_broadcast(&cond);
}

/* Waits for all worker threads to complete their execution. */
void join_worker_threads(pthread_t* threads) {
  // worker thread
  for (int j = 0; j < nthreads; j++) {
    if (pthread_join(threads[j], NULL) != 0) {
      fprintf(stderr, "Failed to join thread %d\n", j);
      exit(EXIT_FAILURE);
    }
  }
}

/* Main ========================================================= */
int main(int argc, char **argv) {
  /* COMMAND LINE OPTIONS ============================================= */
  char *workload_path = "workload.txt";
  // char *server = "localhost";  // comment out for global access 
  int option_char = 0;
  // unsigned short port = 39474; // comment out for global access
  // char *req_path = NULL;


  // int returncode = 0; // comment out for global access
  // int nthreads = 8; // comment out for global access
  // char local_path[PATH_BUFFER_SIZE];
  // int nrequests = 14; // comment out for global access

  // gfcrequest_t *gfr = NULL;
  // FILE *file = NULL;

  setbuf(stdout, NULL);  // disable caching

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:n:hs:t:r:w:", gLongOptions,
                                    NULL)) != -1) {
    switch (option_char) {

      case 's':  // server
        server = optarg;
        break;
      case 'w':  // workload-path
        workload_path = optarg;
        break;
      case 'r': // nrequests
      case 'n': // nrequests
        nrequests = atoi(optarg);
        break;
      case 't':  // nthreads
        nthreads = atoi(optarg);
        break;
      case 'p':  // port
        port = atoi(optarg);
        break;
      default:
        Usage();
        exit(1);


      case 'h':  // help
        Usage();
        exit(0);
    }
  }

  if (EXIT_SUCCESS != workload_init(workload_path)) {
    fprintf(stderr, "Unable to load workload file %s.\n", workload_path);
    exit(EXIT_FAILURE);
  }
  if (port > 65331) {
    fprintf(stderr, "Invalid port number\n");
    exit(EXIT_FAILURE);
  }
  if (nthreads < 1 || nthreads > MAX_THREADS) {
    fprintf(stderr, "Invalid amount of threads\n");
    exit(EXIT_FAILURE);
  }
  gfc_global_init();

  /* start of threadpool creation */
  
  // create and initiate work queue 
  work_queue = (steque_t *)malloc(sizeof(steque_t));
  steque_init(work_queue);

  // create worker threads
  pthread_t* threads = (pthread_t *)malloc(nthreads*sizeof(pthread_t));
  create_worker_threads(threads);
  /* end of threadpool creation */

  // Initialize threads, mutex, condition variable, and work queue beforehand
  enqueue_requests();
  wait_for_requests_completion();
  join_worker_threads(threads);

  /*  use for any global cleanup for AFTER your thread
      pool has terminated. */
  gfc_global_cleanup(); // clean global variables             
  free(threads); // free malloc pointers
  free(work_queue);
  return 0;
}
