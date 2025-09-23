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

#define BUFSIZE 512

#define USAGE                                                                  \
  "usage:\n"                                                                   \
  "  transferclient [options]\n"                                               \
  "options:\n"                                                                 \
  "  -s                  Server (Default: localhost)\n"                        \
  "  -p                  Port (Default: 29345)\n"                              \
  "  -o                  Output file (Default cs6200.txt)\n"                   \
  "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {{"output", required_argument, NULL, 'o'},
                                       {"server", required_argument, NULL, 's'},
                                       {"help", no_argument, NULL, 'h'},
                                       {"port", required_argument, NULL, 'p'},
                                       {NULL, 0, NULL, 0}};

/* Main ========================================================= */
int main(int argc, char **argv) {
  int option_char = 0;
  unsigned short portno = 29345;
  char *hostname = "localhost";
  char *filename = "cs6200.txt";

  setbuf(stdout, NULL);

  // Parse and set command line arguments
  while ((option_char =
              getopt_long(argc, argv, "s:p:o:hx", gLongOptions, NULL)) != -1) {
    switch (option_char) {
    case 'p': // listen-port
      portno = atoi(optarg);
      break;
    case 's': // server
      hostname = optarg;
      break;
    default:
      fprintf(stderr, "%s", USAGE);
      exit(1);
    case 'o': // filename
      filename = optarg;
      break;
    case 'h': // help
      fprintf(stdout, "%s", USAGE);
      exit(0);
      break;
    }
  }

  if (NULL == hostname) {
    fprintf(stderr, "%s @ %d: invalid host name\n", __FILE__, __LINE__);
    exit(1);
  }

  if (NULL == filename) {
    fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
    exit(1);
  }

  if ((portno < 1025) || (portno > 65535)) {
    fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__,
            portno);
    exit(1);
  }

  /* Socket Code Here */

  int sockfd;
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
  snprintf(portbuf, sizeof(portbuf), "%hu", portno);

  // getaddrinfo
  status = getaddrinfo(hostname, portbuf, &server_addr, &serverinfo);
  if (status != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    exit(1);
  }

  // sockets
  sockfd = socket(serverinfo->ai_family, serverinfo->ai_socktype,
                  serverinfo->ai_protocol);
  if (sockfd < 0) {
    perror("socket");
    exit(1);
  }

  // connect
  if (connect(sockfd, serverinfo->ai_addr, serverinfo->ai_addrlen) < 0) {
    perror("connect");
    exit(1);
  }

  // buffer size
  char buffer[BUFSIZE];
  int msg;

  /* write file */
  FILE *fp;

  fp = fopen(filename, "wb");

  if (fp == NULL) {
    perror("File Read Error");
    exit(1);
  }

  // recieve if msg is greater than 1
  while ((msg = recv(sockfd, buffer, BUFSIZE, 0)) > 0) {
    if (msg < 0) {
      perror("recv");
      exit(1);
    }

    if (fwrite(buffer, sizeof(char), msg, fp) < sizeof(msg)) {
      perror("write");
      fclose(fp);
      exit(1);
    }

    // printf("%s", buffer);
  }

  fclose(fp);
  close(sockfd);
  freeaddrinfo(serverinfo);
  return 0;
}
