
#include "gfclient.h"
#include "gfclient-student.h"
#include <stdlib.h>

#define BUFSIZE 1024

// Modify this file to implement the interface specified in
// gfclient.h.

struct gfcrequest_t {
  void (*schemefunc)(void *scheme_buffer, size_t scheme_buffer_length,
                     void *handlerarg);
  void *schemearg;
  void (*writefunc)(void *, size_t, void *);
  void *writearg;

  const char *server;
  unsigned short port;
  const char *path;
  int client_sockfd;

  size_t filelen;
  size_t bytesreceived;
  gfstatus_t status;
};

// optional function for cleaup processing.
void gfc_cleanup(gfcrequest_t **gfr) {
  free(*gfr);
  *gfr = NULL;
}

size_t gfc_get_filelen(gfcrequest_t **gfr) {
  return (*gfr)->filelen;
} // DONE***

size_t gfc_get_bytesreceived(gfcrequest_t **gfr) {
  return (*gfr)->bytesreceived;
} // DONE

gfcrequest_t *gfc_create() {
  gfcrequest_t *gfr = (gfcrequest_t *)malloc(sizeof(gfcrequest_t));
  memset(gfr, 0, sizeof(gfcrequest_t));
  gfr->status = GF_INVALID;
  gfr->filelen = 0;
  gfr->bytesreceived = 0;
  return gfr;
} // DONE

gfstatus_t gfc_get_status(gfcrequest_t **gfr) {
  return (*gfr)->status;
} // DONE*

void gfc_global_init() {}

void gfc_global_cleanup() {}

int gfc_perform(gfcrequest_t **gfr) {
  /*Socket Code Here */
  // int sockfd;
  int status;
  struct addrinfo server_addr;
  struct addrinfo *serverinfo;

  // Zeroing bits to initialize; Also struct addrinfo server_addr = {0}; works
  // Format of socket and the structure it wants. I.e. how it wants to send data
  memset(&server_addr, 0, sizeof server_addr);
  server_addr.ai_family = AF_UNSPEC;
  server_addr.ai_socktype = SOCK_STREAM;

  // Change int port to buff
  char portbuf[6];
  snprintf(portbuf, sizeof(portbuf), "%hu", (*gfr)->port);

  // getaddrinfo
  status = getaddrinfo((*gfr)->server, portbuf, &server_addr, &serverinfo);
  if (status != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    exit(1);
  }

  // sockets
  (*gfr)->client_sockfd = socket(serverinfo->ai_family, serverinfo->ai_socktype,
                                 serverinfo->ai_protocol);
  if ((*gfr)->client_sockfd < 0) {
    perror("socket");
    exit(1);
  }

  // connect
  if (connect((*gfr)->client_sockfd, serverinfo->ai_addr,
              serverinfo->ai_addrlen) < 0) {
    perror("connect");
    exit(1);
  }
  freeaddrinfo(serverinfo);

  // build request
  char request[BUFSIZE];
  sprintf(request, "GETFILE GET %s\r\n\r\n", (*gfr)->path);

  // Send request
  ssize_t total_sent = 0;
  size_t bytesleft = strlen(request);
  ssize_t actual_sent;
  // partial send requst scheme
  while (total_sent < bytesleft) {
    actual_sent = send((*gfr)->client_sockfd, request, strlen(request), 0);
    if (actual_sent <= 0) {
      // Check for missing bytes sent
      perror("send");
      break;
    }
    total_sent += actual_sent;
    bytesleft -= actual_sent;
  }

  // receive data
  char buffer[BUFSIZE];
  ssize_t rcvd;
  int scheme_parsed = 0;
  size_t filelen = 0;
  (*gfr)->bytesreceived = 0;
  (*gfr)->status = GF_INVALID;

  while ((rcvd = recv((*gfr)->client_sockfd, buffer, sizeof(buffer) - 1, 0)) >
         0) {

    if (!scheme_parsed) {

      char *scheme;
      char *status = NULL;

      scheme = strtok(buffer, " ");
      if ((scheme == NULL) || strcmp(scheme, "GETFILE") != 0) {
        (*gfr)->status = GF_INVALID;
        scheme_parsed = 1;
        continue;
      }

      status = strtok(NULL, " ");
      if (status == NULL) {
        (*gfr)->status = GF_INVALID;
        scheme_parsed = 1;
        continue;
      }

      char *filelen_str = strtok(NULL, "\r\n\r\n");
      if (filelen_str) {
        filelen = strtoul(filelen_str, NULL, 10);
      }

      if (strcmp(status, "OK") == 0) {
        (*gfr)->status = GF_OK;
        (*gfr)->filelen = filelen;
      } else if (strcmp(status, "FILE_NOT_FOUND") == 0) {
        (*gfr)->status = GF_FILE_NOT_FOUND;
      } else {
        (*gfr)->status = GF_ERROR;
      }

      // ignore any leftover body for now
    } else {
      // write file chunks directly
      if ((*gfr)->writefunc && (*gfr)->status == GF_OK) {
        (*gfr)->writefunc(buffer, rcvd, (*gfr)->writearg);
      }
      (*gfr)->bytesreceived += rcvd;
    }

    // stop if we got everything
    if (scheme_parsed && (*gfr)->status == GF_OK &&
        (*gfr)->bytesreceived >= filelen) {
      break;
    }
  }

  close((*gfr)->client_sockfd);
  if ((*gfr)->status == GF_ERROR || (*gfr)->status == GF_FILE_NOT_FOUND ||
      ((*gfr)->status == GF_OK && (*gfr)->bytesreceived == (*gfr)->filelen)) {
    return 0;
  }

  return -1;
}

void gfc_set_port(gfcrequest_t **gfr, unsigned short port) {
  (*gfr)->port = port;
} // DONE

void gfc_set_server(gfcrequest_t **gfr, const char *server) {
  (*gfr)->server = server;
} // DONE

void gfc_set_schemefunc(gfcrequest_t **gfr,
                        void (*schemefunc)(void *, size_t, void *)) {
  (*gfr)->schemefunc = schemefunc;
} // DONE

void gfc_set_schemearg(gfcrequest_t **gfr, void *schemearg) {
  (*gfr)->schemearg = schemearg;
} // DONE

void gfc_set_path(gfcrequest_t **gfr, const char *path) {
  (*gfr)->path = path;
} // DONE

void gfc_set_writefunc(gfcrequest_t **gfr,
                       void (*writefunc)(void *, size_t, void *)) {
  (*gfr)->writefunc = writefunc;
} // DONE

void gfc_set_writearg(gfcrequest_t **gfr, void *writearg) {
  (*gfr)->writearg = writearg;
} // DONE

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
} // LEAVE ALONE
