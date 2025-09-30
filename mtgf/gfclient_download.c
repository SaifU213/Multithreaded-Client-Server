#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "gfclient-student.h"
#include "steque.h"

#define MAX_THREADS 1024
#define PATH_BUFFER_SIZE 4096

#define USAGE                                                                  \
  "usage:\n"                                                                   \
  "  gfclient_download [options]\n"                                            \
  "options:\n"                                                                 \
  "  -h                  Show this help message\n"                             \
  "  -p [server_port]    Server port (Default: 29458)\n"                       \
  "  -t [nthreads]       Number of threads (Default 8 Max: 1024)\n"            \
  "  -w [workload_path]  Path to workload file (Default: workload.txt)\n"      \
  "  -s [server_addr]    Server address (Default: 127.0.0.1)\n"                \
  "  -n [num_requests]   Request download total (Default: 16)\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"server", required_argument, NULL, 's'},
    {"port", required_argument, NULL, 'p'},
    {"help", no_argument, NULL, 'h'},
    {"nthreads", required_argument, NULL, 't'},
    {"workload", required_argument, NULL, 'w'},
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

// Move variables up here
char *workload_path = "workload.txt";
char *server = "localhost";
unsigned short port = 29458;
int option_char = 0;
int nthreads = 8;

// int returncode = 0;
char *req_path = NULL;
char local_path[PATH_BUFFER_SIZE];
int nrequests = 14;

int requests_enqueued = 0;

/* Thread global variables */
// Request queue
steque_t queue;
// mutex
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
// condition variables
pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;

void *threadfunc(void *threadargument) {
  gfcrequest_t *gfr = NULL;
  FILE *file = NULL;
  // Check if empty
  while (1) {
    // Grab mutex
    pthread_mutex_lock(&mutex);

    // If empty release mutex
    while (steque_isempty(&queue) && !requests_enqueued) {
      pthread_cond_wait(&queue_not_empty, &mutex);
    }

    // exit thread after completion
    if (steque_isempty(&queue) && requests_enqueued) {
      pthread_mutex_unlock(&mutex);
      return (NULL);
    }
    // Dequeue and assign to thread
    char *req_path = (char *)steque_pop(&queue);

    // Release Mutex
    pthread_mutex_unlock(&mutex);

    int returncode = 0;
    // char *req_path = NULL;
    char local_path[PATH_BUFFER_SIZE];

    if (strlen(req_path) > PATH_BUFFER_SIZE) {
      fprintf(stderr, "Request path exceeded maximum of %d characters\n.",
              PATH_BUFFER_SIZE);
      exit(EXIT_FAILURE);
    }

    localPath(req_path, local_path);

    file = openFile(local_path);

    gfr = gfc_create();
    gfc_set_path(&gfr, req_path);

    gfc_set_port(&gfr, port);
    gfc_set_server(&gfr, server);
    gfc_set_writearg(&gfr, file);
    gfc_set_writefunc(&gfr, writecb);

    fprintf(stdout, "Requesting %s%s\n", server, req_path);

    if (0 > (returncode = gfc_perform(&gfr))) {
      fprintf(stdout, "gfc_perform returned an error %d\n", returncode);
      fclose(file);
      if (0 > unlink(local_path))
        fprintf(stderr, "warning: unlink failed on %s\n", local_path);
    } else {
      fclose(file);
    }

    if (gfc_get_status(&gfr) != GF_OK) {
      if (0 > unlink(local_path)) {
        fprintf(stderr, "warning: unlink failed on %s\n", local_path);
      }
    }

    fprintf(stdout, "Status: %s\n", gfc_strstatus(gfc_get_status(&gfr)));
    fprintf(stdout, "Received %zu of %zu bytes\n", gfc_get_bytesreceived(&gfr),
            gfc_get_filelen(&gfr));

    gfc_cleanup(&gfr);
    free(req_path);
    /*
     * note that when you move the above logic into your worker thread, you will
     * need to coordinate with the boss thread here to effect a clean shutdown.
     */
  }
  return (NULL);
}

/* Main ========================================================= */
int main(int argc, char **argv) {
  /* COMMAND LINE OPTIONS ============================================= */
  /* char *workload_path = "workload.txt";
  char *server = "localhost";
  unsigned short port = 29458;
  int option_char = 0;
  int nthreads = 8;

  int returncode = 0;
  char *req_path = NULL;
  char local_path[PATH_BUFFER_SIZE];
  int nrequests = 14;

  gfcrequest_t *gfr = NULL;
  FILE *file = NULL; */

  setbuf(stdout, NULL); // disable caching

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:n:hs:t:r:w:", gLongOptions,
                                    NULL)) != -1) {
    switch (option_char) {

    case 'w': // workload-path
      workload_path = optarg;
      break;
    case 's': // server
      server = optarg;
      break;
    case 'r': // nrequests
    case 'n': // nrequests
      nrequests = atoi(optarg);
      break;
    case 'p': // port
      port = atoi(optarg);
      break;
    case 't': // nthreads
      nthreads = atoi(optarg);
      break;
    default:
      Usage();
      exit(1);

    case 'h': // help
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

  // add your threadpool creation here

  /* Initialize thread management */
  // initialize global resource such as request queue, mutexes, and condition
  // variables

  // Initialize queue before thread and thredfunc
  steque_init(&queue);

  // Create threads
  pthread_t threads[nthreads];

  for (int i = 0; i < nthreads; i++) {
    pthread_create(&threads[i], NULL, threadfunc, NULL);
  }

  // We want a thread per file
  // Create thread that absorbes the code below

  /* Build your queue of requests here */
  for (int i = 0; i < nrequests; i++) {
    /* Note that when you have a worker thread pool, you will need to move this
     * logic into the worker threads */
    // Grab mutex
    pthread_mutex_lock(&mutex);
    req_path = workload_get_path();
    // Duplicates into a new memory block
    char *req_path_copy = strdup(req_path);
    steque_enqueue(&queue, req_path_copy);
    // Wake up thread
    pthread_cond_signal(&queue_not_empty);
    // Release Mutex
    pthread_mutex_unlock(&mutex);
  }

  pthread_mutex_lock(&mutex);
  requests_enqueued = 1;
  pthread_cond_broadcast(&queue_not_empty);
  pthread_mutex_unlock(&mutex);

  // Wait on threads to finish using join
  for (int i = 0; i < nthreads; i++) {
    pthread_join(threads[i], NULL);
  }

  gfc_global_cleanup(); /* use for any global cleanup for AFTER your thread
                         pool has terminated. */

  return 0;
}
