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
  "  transferserver [options]\n"                                               \
  "options:\n"                                                                 \
  "  -f                  Filename (Default: 6200.txt)\n"                       \
  "  -p                  Port (Default: 29345)\n"                              \
  "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"filename", required_argument, NULL, 'f'},
    {"help", no_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}};

// Function for partial sends
int sendall(int s, char *buf, int *len) {
  int total = 0;
  int bytesleft = *len;
  int n;

  while (total < *len) {
    n = send(s, buf + total, bytesleft, 0);
    if (n == -1) {
      break;
    }
    *len = total;
  }
  return n == -1 ? -1 : 0;
}

int main(int argc, char **argv) {
  int portno = 29345; /* port to listen on */
  int option_char;
  char *filename = "6200.txt"; /* file to transfer */

  setbuf(stdout, NULL); // disable buffering

  // Parse and set command line arguments
  while ((option_char =
              getopt_long(argc, argv, "p:hf:x", gLongOptions, NULL)) != -1) {
    switch (option_char) {
    case 'p': // listen-port
      portno = atoi(optarg);
      break;
    case 'f': // file to transfer
      filename = optarg;
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

  if ((portno < 1025) || (portno > 65535)) {
    fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__,
            portno);
    exit(1);
  }

  if (NULL == filename) {
    fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
    exit(1);
  }

  /* Socket Code Here */

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
  }

  if ((server_sockfd = socket(serverinfo->ai_family, serverinfo->ai_socktype,
                              serverinfo->ai_protocol)) < 0) {
    // printf("Error while creating socket\n");
    perror("socket");
    exit(1);
  }

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
  if (listen(server_sockfd, 5) < 0) {
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

    /* File I/O */
    // Open file
    // Parse file into buffer
    // Compare length recieved to lenght of file
    // send to client
    // indicate that length of message is incomplete
    // send rest then indicate complete

    /* Send file */
    FILE *fp;
    int bytes_read; // Variable to store character
    fp = fopen(filename, "rb");

    // buffer size
    // char s[BUFSIZE] = "\0";
    char buffer[BUFSIZE] = "\0";
    // https://stackoverflow.com/questions/2029103/correct-way-to-read-a-text-file-into-a-buffer-in-c

    if (fp == NULL) {
      perror("File Read Error");
      exit(1);
    }
    /* Implementaion of partial send by Beej */
    // fread: number of bytes of length char read sucessfully in buffer length
    while ((bytes_read = fread(buffer, sizeof(char), BUFSIZE, fp)) > 0) {
      // fwrite(s, sizeof(char), n, stdout);
      int total_sent = 0;
      int bytesleft = bytes_read;
      int actual_sent;

      while (total_sent < bytes_read) {
        actual_sent = send(client_sockfd, buffer + total_sent, bytesleft, 0);
        if (actual_sent < 0) {
          // Check for missing bytes sent
          perror("send");
          exit(1);
        }
        total_sent += actual_sent;
        bytesleft -= actual_sent;
      }

      printf("%d\n", bytes_read);
      if (ferror(fp)) {
        perror("Error reading test.bin");
      }
    }
    if (feof(fp)) {
      printf("End of file\n");
    }

    // close
    fclose(fp);
    close(client_sockfd);
  }

  freeaddrinfo(serverinfo);
  return 0;
}
