/*
 *  This file is for use by students to define anything they wish.  It is used by the gf server implementation
 */
#ifndef __GF_SERVER_STUDENT_H__
#define __GF_SERVER_STUDENT_H__

#include "gf-student.h"
#include "gfserver.h"
#include "content.h"

/* Additonal includes and defines*/
#include <stdlib.h>
#include <pthread.h>
#include "steque.h"
#include <stdbool.h>
#define BUFSIZE 512

// steque_request data structure inspired by steque item and enqueue
typedef struct steque_request {
	const char *filepath;
	void* arg;
    gfcontext_t *context;
} steque_request;

void init_threads(size_t numthreads);
void cleanup_threads();

#endif // __GF_SERVER_STUDENT_H__
