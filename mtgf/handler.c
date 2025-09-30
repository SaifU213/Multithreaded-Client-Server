#include "content.h"
#include "gfserver-student.h"
#include "gfserver.h"
#include "steque.h"
#include "workload.h"
#include <pthread.h>
#include <stdlib.h>

//
//  The purpose of this function is to handle a get request
//
//  The ctx is a pointer to the "context" operation and it contains connection
//  state The path is the path being retrieved The arg allows the registration
//  of context that is passed into this routine. Note: you don't need to use
//  arg. The test code uses it in some cases, but
//        not in others.

extern steque_t queue;
extern pthread_mutex_t mutex;
extern pthread_cond_t queue_not_empty;

gfh_error_t gfs_handler(gfcontext_t **ctx, const char *path, void *arg) {

  /* Note that when you have a worker thread pool, you will need to move this
   * logic into the worker threads */
  // Grab mutex
  struct handler_item *item = malloc(sizeof(struct handler_item));
  if (item == NULL) {
    // handle allocation failure
    return gfh_failure;
  }
  item->ctx = (*ctx);
  item->path = strdup(path);

  pthread_mutex_lock(&mutex);
  steque_enqueue(&queue, (void *)item);
  // Wake up thread
  pthread_cond_signal(&queue_not_empty);

  // Ownership problem
  // ADDED LATER
  (*ctx) = NULL;

  // Release Mutex
  pthread_mutex_unlock(&mutex);

  return gfh_success;
}