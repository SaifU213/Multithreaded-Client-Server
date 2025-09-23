#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// To
#include <stddef.h>

#define BUFSIZE 1024

#define USAGE                                                                  \
  "usage:\n"                                                                   \
  "  echoserver [options]\n"                                                   \
  "options:\n"                                                                 \
  "  -m                  Maximum pending connections (default: 5)\n"           \
  "  -p                  Port (Default: 39483)\n"                              \
  "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"help", no_argument, NULL, 'h'},
    {"maxnpending", required_argument, NULL, 'm'},
    {"port", required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}};

int main(int argc, char **argv) {
  int portno = 39483; /* port to listen on */
  int option_char;
  int maxnpending = 5;

  // Parse and set command line arguments
  while ((option_char =
              getopt_long(argc, argv, "p:m:hx", gLongOptions, NULL)) != -1) {
    switch (option_char) {
    case 'h': // help
      fprintf(stdout, "%s ", USAGE);
      exit(0);
      break;
    case 'p': // listen-port
      portno = atoi(optarg);
      break;
    case 'm': // server_addr
      maxnpending = atoi(optarg);
      break;
    default:
      fprintf(stderr, "%s ", USAGE);
      exit(1);
    }
  }

  setbuf(stdout, NULL); // disable buffering

  if ((portno < 1025) || (portno > 65535)) {
    fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__,
            portno);
    exit(1);
  }
  if (maxnpending < 1) {
    fprintf(stderr, "%s @ %d: invalid pending count (%d)\n", __FILE__, __LINE__,
            maxnpending);
    exit(1);
  }

  /* Socket Code Here */

  //**
  // = socket(AF_UNSPEC, SOCK_STREAM, 0);

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
  snprintf(portbuf, sizeof(portbuf), "%hu", portno);

  // socket
  if ((status = getaddrinfo(NULL, portbuf, &server_addr, &serverinfo)) != 0) {
    // printf("Error while getting Address\n");
    fprintf(stderr, "gai error: %s\n", gai_strerror(status));
    exit(1);
  };

  if ((server_sockfd = socket(serverinfo->ai_family, serverinfo->ai_socktype,
                              serverinfo->ai_protocol)) < 0) {
    // printf("Error while creating socket\n");
    perror("socket");
    exit(1);
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
  if (listen(server_sockfd, maxnpending) < 0) {
    // printf("Error while binding socket\n");
    perror("listen");
    exit(1);
  };

  // accept
  // connect
  while (1) {
    addr_size = sizeof client_addr;
    // Second socket - client_sockfd to facilitate send and recv
    if ((client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr,
                                &addr_size)) < 0) {
      perror("accept");
      exit(1);
    }

    char buffer[BUFSIZE];
    int msg_len;

    // recieve
    if ((msg_len = recv(client_sockfd, buffer, BUFSIZE, 0)) <= 0) {
      perror("recv");
      exit(1);
    }
    // send
    if ((send(client_sockfd, buffer, msg_len, 0)) != msg_len) {
      perror("send");
      exit(1);
    }
    // close
    close(client_sockfd);
  }

  freeaddrinfo(serverinfo);
  return 0;
}
