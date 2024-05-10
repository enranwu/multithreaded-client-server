#include "gfserver-student.h"
#include "gfserver.h"
#include "workload.h"
#include "content.h"


extern pthread_mutex_t mutex;
extern steque_t* work_queue;
extern pthread_cond_t cond;

//
//  The purpose of this function is to handle a get request
//
//  The ctx is a pointer to the "context" operation and it contains connection state
//  The path is the path being retrieved
//  The arg allows the registration of context that is passed into this routine.
//  Note: you don't need to use arg. The test code uses it in some cases, but
//        not in others.
//
gfh_error_t gfs_handler(gfcontext_t **ctx, const char *path, void* arg) {
	// Allocate and initialize the request structure
	steque_request* req = (steque_request*)malloc(sizeof(*req));
	if (req == NULL) {
		return gfh_failure; // Early return on allocation failure
	}
    
    req->context = *ctx;
    req->filepath = path;
    req->arg = arg;
    
    // Lock mutex before modifying the queue
    if (pthread_mutex_lock(&mutex) != 0) {
        free(req); // Ensure to free allocated memory on error
        return gfh_failure;
    }
    
    // Enqueue the request
    steque_enqueue(work_queue, req);
    
    // Unlock mutex after modification
    if (pthread_mutex_unlock(&mutex) != 0) {
        // No need to free req here, as it's already enqueued
        return gfh_failure;
    }
    
    // Signal a worker thread that there is new work
    pthread_cond_signal(&cond);
    
    // Set context to NULL as per specification to avoid misuse
    *ctx = NULL;
    
    return gfh_success;
}

