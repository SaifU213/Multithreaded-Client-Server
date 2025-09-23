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

/* Be prepared accept a response of this length */
#define BUFSIZE 1024

#define USAGE                                                                  \
  "usage:\n"                                                                   \
  "  echoclient [options]\n"                                                   \
  "options:\n"                                                                 \
  "  -s                  server_addr (Default: localhost)\n"                   \
  "  -m                  Message to send to server_addr (Default: \"Hello "    \
  "Spring!!\")\n"                                                              \
  "  -p                  Port (Default: 39483)\n"                              \
  "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"port", required_argument, NULL, 'p'},
    {"server_addr", required_argument, NULL, 's'},
    {"help", no_argument, NULL, 'h'},
    {"message", required_argument, NULL, 'm'},
    {NULL, 0, NULL, 0}};

/* Main ========================================================= */
int main(int argc, char **argv) {
  int option_char = 0;
  unsigned short portno = 39483;
  char *hostname = "localhost";
  char *message = "Hello Fall!!";

  // Parse and set command line arguments
  while ((option_char =
              getopt_long(argc, argv, "s:p:m:hx", gLongOptions, NULL)) != -1) {
    switch (option_char) {
    case 'p': // listen-port
      portno = atoi(optarg);
      break;
    case 'm': // message
      message = optarg;
      break;
    case 's': // server_addr
      hostname = optarg;
      break;
    case 'h': // help
      fprintf(stdout, "%s", USAGE);
      exit(0);
      break;
    default:
      fprintf(stderr, "%s", USAGE);
      exit(1);
    }
  }

  setbuf(stdout, NULL); // disable buffering

  if ((portno < 1025) || (portno > 65535)) {
    fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__,
            portno);
    exit(1);
  }

  if (NULL == message) {
    fprintf(stderr, "%s @ %d: invalid message\n", __FILE__, __LINE__);
    exit(1);
  }

  if (NULL == hostname) {
    fprintf(stderr, "%s @ %d: invalid host name\n", __FILE__, __LINE__);
    exit(1);
  }

  /* File I/O */

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

  char buffer[BUFSIZE];
  int msg_len;

  // send
  if ((msg_len = send(sockfd, message, strlen(message), 0)) < 0) {
    perror("send");
    exit(1);
  }

  // recieve
  if ((msg_len = recv(sockfd, buffer, BUFSIZE - 1, 0)) < 0) {
    perror("recv");
    exit(1);
  }

  buffer[msg_len] = '\0';
  printf("%s", buffer);

  close(sockfd);
  freeaddrinfo(serverinfo);
  return 0;
}
