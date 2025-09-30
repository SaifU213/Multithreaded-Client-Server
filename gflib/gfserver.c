#include "gfserver-student.h"
#include <stdlib.h>
#include <string.h>

#define BUFSIZE 1024

// Modify this file to implement the interface specified in
// gfserver.h.

// Initial structure
struct gfserver_t {
  int sockfd;
  unsigned short port;
  int max_npending;
  void *arg;
  gfh_error_t (*handler)(gfcontext_t **, const char *,
                         void *); // Handler funtion POINTER/ Callback
};

// Context
/* Think of gfcontext_t as the folder for a single request. It tracks the
 * socket, the requested path, how many bytes you’ve sent, whether the transfer
 * finished or was aborted, and any other state you need for that one request.
 * For Part 1, the handler is already provided; you won’t write your own; so
 * your job is just to build and pass a valid context. */
struct gfcontext_t {
  int client_sockfd;
  int bytes_sent;
  size_t file_len;
};

void gfs_abort(gfcontext_t **ctx) { close((*ctx)->client_sockfd); } // DONE

ssize_t gfs_send(gfcontext_t **ctx, const void *data, size_t len) {
  // not yet implemented
  // send
  ssize_t total_sent = 0;
  size_t bytesleft = len;
  ssize_t actual_sent;

  // partial send
  while (total_sent < len) {
    actual_sent = send((*ctx)->client_sockfd, data + total_sent, bytesleft, 0);
    if (actual_sent <= 0) {
      // Check for missing bytes sent
      perror("send");
      break;
    }
    total_sent += actual_sent;
    bytesleft -= actual_sent;
  }

  // printf("send");
  // retuns total_sent ssize_t
  return total_sent;
} // DONE MAYBE

ssize_t gfs_sendheader(gfcontext_t **ctx, gfstatus_t status, size_t file_len) {
  // not yet implemented
  char buffer[BUFSIZE];
  ssize_t bytes_sent;

  switch (status) {
  case (GF_FILE_NOT_FOUND):
    bytes_sent = (send((*ctx)->client_sockfd, "GETFILE FILE_NOT_FOUND\r\n\r\n",
                       strlen("GETFILE FILE_NOT_FOUND\r\n\r\n"), 0));
    if (bytes_sent < 0) {
      perror("send");
      break;
    }
    break;
  case (GF_ERROR):
    bytes_sent = (send((*ctx)->client_sockfd, "GETFILE ERROR\r\n\r\n",
                       strlen("GETFILE ERROR\r\n\r\n"), 0));
    if (bytes_sent < 0) {
      perror("send");
      break;
    }
    break;
  case (GF_INVALID):
    bytes_sent = (send((*ctx)->client_sockfd, "GETFILE INVALID\r\n\r\n",
                       strlen("GETFILE INVALID\r\n\r\n"), 0));
    if (bytes_sent < 0) {
      perror("send");
      break;
    }
    break;
  case (GF_OK):
    (*ctx)->file_len = file_len;
    sprintf(buffer, "GETFILE OK %zu\r\n\r\n", file_len);
    bytes_sent = (send((*ctx)->client_sockfd, buffer, strlen(buffer), 0));
    if (bytes_sent < 0) {
      perror("send");
      break;
    }
    break;
  }
  // printf("sendheader");
  return bytes_sent;
}

gfserver_t *gfserver_create() {
  // Creates the handeler and dynamically allocates memory
  // to be used across the program
  gfserver_t *server_handler = (gfserver_t *)malloc(sizeof(gfserver_t));
  return server_handler;
} // DONE

/* Sets the port of the server */
void gfserver_set_port(gfserver_t **gfs, unsigned short port) {
  (*gfs)->port = port;
} // DONE

/* Sets the arguments for the handler */
void gfserver_set_handlerarg(gfserver_t **gfs, void *arg) {
  (*gfs)->arg = arg;
} // DONE

/* Serves the content */
void gfserver_serve(gfserver_t **gfs) {
  // getaddrinfo
  struct sockaddr_storage client_addr;
  socklen_t addr_size;
  int server_sockfd, client_sockfd;
  int status;
  struct addrinfo server_addr;
  struct addrinfo *serverinfo;

  // Zeroing bits to initialize; Also struct addrinfo server_addr = {0}; works
  memset(&server_addr, 0, sizeof server_addr);
  server_addr.ai_family = AF_INET;
  server_addr.ai_socktype = SOCK_STREAM;
  server_addr.ai_flags = AI_PASSIVE;

  // Change int port to buff
  char portbuf[6];
  snprintf(portbuf, sizeof(portbuf), "%hu", (*gfs)->port);

  if ((status = getaddrinfo(NULL, portbuf, &server_addr, &serverinfo)) != 0) {
    // printf("Error while getting Address\n");
    fprintf(stderr, "gai error: %s\n", gai_strerror(status));
  };

  if (((server_sockfd) = socket(serverinfo->ai_family, serverinfo->ai_socktype,
                                serverinfo->ai_protocol)) < 0) {
    // printf("Error while creating socket\n");
    perror("socket");
  };

  int yes = 1;
  // reuse port
  setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

  // bind
  if ((bind(server_sockfd, serverinfo->ai_addr, serverinfo->ai_addrlen) < 0)) {
    // printf("Error while binding socket\n");
    perror("bind");
    exit(1);
  };

  // listen
  // First socket - server_sockfd accepts connections
  if (listen(server_sockfd, (*gfs)->max_npending) < 0) {
    // printf("Error while binding socket\n");
    perror("listen");
    exit(1);
  };

  char header[BUFSIZE];
  // accept
  // connect
  while (1) {
    gfcontext_t *client_context = malloc(sizeof(gfcontext_t));

    // Second socket - client_sockfd to facilitate send and recv
    addr_size = sizeof client_addr;
    if ((client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr,
                                &addr_size)) < 0) {
      perror("accept");
      break;
    }
    (*client_context).client_sockfd = client_sockfd;

    while (1) {
      int bytes_recv;
      (bytes_recv = recv(client_sockfd, header, BUFSIZE, 0));
      if (bytes_recv < 0) {
        perror("recv");
        // free(client_context);
        break;
      }

      // Header
      // https://stackoverflow.com/questions/15080951/tokenizing-a-string-in-c-using-strtok
      char *scheme = strtok(header, " ");
      char *method = strtok(NULL, " ");
      char *path = strtok(NULL, "\r\n\r\n");

      if (strcmp(scheme, "GETFILE") != 0 || strcmp(method, "GET") != 0) {
        send(client_sockfd, "GETFILE INVALID \r\n\r\n",
             strlen("GETFILE INVALID \r\n\r\n"), 0);
        close(client_sockfd);
        // free(client_context);
        break;
      }
      // Sends the data to the handler function
      (*gfs)->handler(&client_context, path, (*gfs)->arg);
      // close
      close(client_sockfd);
      free(client_context);
      break;
      // printf("Processed Request");
    }
  }
  free(*gfs);
  freeaddrinfo(serverinfo);
}

/* Sets and initializes the handler */
void gfserver_set_handler(gfserver_t **gfs,
                          gfh_error_t (*handler)(gfcontext_t **, const char *,
                                                 void *)) {
  (*gfs)->handler = handler;
} // DONE

/* Sets the maximum number of connections at once */
void gfserver_set_maxpending(gfserver_t **gfs, int max_npending) {
  (*gfs)->max_npending = max_npending;
} // DONE
