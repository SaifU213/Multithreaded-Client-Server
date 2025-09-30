#include "gfserver-student.h"
#include "gfserver.h"
#include "steque.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#define BUFSIZE 512

#define USAGE                                                                  \
  "usage:\n"                                                                   \
  "  gfserver_main [options]\n"                                                \
  "options:\n"                                                                 \
  "  -h                  Show this help message.\n"                            \
  "  -m [content_file]   Content file mapping keys to content files "          \
  "(Default: content.txt\n"                                                    \
  "  -t [nthreads]       Number of threads (Default: 16)\n"                    \
  "  -d [delay]          Delay in content_get, default 0, range 0-5000000 "    \
  "  -p [listen_port]    Listen port (Default: 29458)\n"                       \
  "(microseconds)\n "

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"nthreads", required_argument, NULL, 't'},
    {"content", required_argument, NULL, 'm'},
    {"port", required_argument, NULL, 'p'},
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

/* Thread global variables */
// Request queue
steque_t queue;
// mutex
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
// condition variables
pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;

void *threadfunc(void *threadargument) {
  while (1) {
    pthread_mutex_lock(&mutex);
    while (steque_isempty(&queue)) {
      pthread_cond_wait(&queue_not_empty, &mutex);
    }
    // Dequeue and assign to thread
    struct handler_item *item = (struct handler_item *)steque_pop(&queue);

    pthread_mutex_unlock(&mutex);

    gfcontext_t *ctx = item->ctx;
    char *path = item->path;

    int fildes = content_get(path);
    if (fildes < 0) {
      gfs_sendheader(&ctx, GF_FILE_NOT_FOUND, 0);
      fprintf(stderr, "File not found %s\n", gai_strerror(fildes));
      // gfs_abort(&ctx);
      free(item->path);
      free(item);
      continue;
    }

    // Get file size
    struct stat finfo;
    int fstat_ok;
    fstat_ok = fstat(fildes, &finfo);
    if (fstat_ok < 0) {
      gfs_sendheader(&ctx, GF_ERROR, 0);
      fprintf(stderr, "File empty: %s\n", gai_strerror(fstat_ok));
      // gfs_abort(&ctx);
      close(fildes);
      free(item->path);
      free(item);
      continue;
    }

    int len = finfo.st_size;

    gfs_sendheader(&ctx, GF_OK, len);

    ssize_t bytes_read = 0;
    ssize_t actual_sent = 0;
    ssize_t total_sent = 0;
    char buffer[BUFSIZE];

    while (total_sent < len) {
      memset(buffer, 0, sizeof(buffer));
      bytes_read = pread(fildes, buffer, BUFSIZE, total_sent);
      if (bytes_read <= 0) {
        break;
      }
      actual_sent = gfs_send(&ctx, buffer, bytes_read);
      total_sent += actual_sent;
    }
    // not yet implemented.
    // close(fildes);
    free(item->path);
    free(item);
  }
  return NULL;
}

/* Main ========================================================= */
int main(int argc, char **argv) {
  char *content_map = "content.txt";
  int nthreads = 16;
  gfserver_t *gfs = NULL;
  int option_char = 0;
  unsigned short port = 29458;

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
    case 'h': /* help */
      fprintf(stdout, "%s", USAGE);
      exit(0);
      break;
    case 'p': /* listen-port */
      port = atoi(optarg);
      break;
    case 'd': /* delay */
      content_delay = (unsigned long int)atoi(optarg);
      break;
    case 't': /* nthreads */
      nthreads = atoi(optarg);
      break;
    case 'm': /* file-path */
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

  // Initialize queue before thread and thredfunc
  steque_init(&queue);

  // Create threads
  pthread_t threads[nthreads];

  for (int i = 0; i < nthreads; i++) {
    pthread_create(&threads[i], NULL, threadfunc, NULL);
  }

  /*Initializing server*/
  gfs = gfserver_create();

  // Setting options
  gfserver_set_port(&gfs, port);
  gfserver_set_maxpending(&gfs, 24);
  gfserver_set_handler(&gfs, gfs_handler);
  gfserver_set_handlerarg(&gfs, NULL); // doesn't have to be NULL!
  // gfserver_set_handlerarg(&gfs, (void *)item); // doesn't have to be NULL!

  /*Loops forever*/
  gfserver_serve(&gfs);
}
