#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>

#define BUFSIZE 1024

int write_n_bytes(int fd, char * buf, int count);
int connect_socket( char *hostname, int port);
char *format_request( char *path );

int main(int argc, char * argv[]) {
    char * server_name = NULL;
    int server_port = 0;
    char * server_path = NULL;

    int sock = 0;
    int rc = -1;
    int datalen = 0;
    bool ok = true;
    int outfd = 1;
    struct sockaddr_in sa;
    FILE * wheretoprint = stdout;
    struct hostent * site = NULL;
    char * req = NULL;

    char buf[BUFSIZE + 1];
    char * bufline = NULL;
    char * oneline = NULL;
    char * endheaders = NULL;
   
    struct timeval timeout;
    fd_set set;

    /*parse args */
    if (argc != 5) {
	fprintf(stderr, "usage: http_client k|u server port path\n");
	exit(-1);
    }

    server_name = argv[2];
    server_port = atoi(argv[3]);
    server_path = argv[4];



    /* initialize minet */
    if (toupper(*(argv[1])) == 'K') { 
	minet_init(MINET_KERNEL);
    } else if (toupper(*(argv[1])) == 'U') { 
	minet_init(MINET_USER);
    } else {
	fprintf(stderr, "First argument must be k or u\n");
	exit(-1);
    }

    printf("initializing socket... \n");
    sock = connect_socket(server_name, server_port);
    if (sock < 0) {
      printf("error connecting socket \n");
    }
    printf("initialized socket \n");
    /* create socket */

    // Do DNS lookup
    /* Hint: use gethostbyname() */

    /* set address */

    /* connect socket */
    
    /* send request */
    req = (char *)malloc(sizeof("GET  HTTP/1.0\r\n") + sizeof(char) * (strlen(server_path) + 1));
    sprintf(req, "GET %s HTTP/1.0\r\n\r\n", server_path);
    printf("request: %s", req);
    if (send(sock, req, strlen(req), 0) < 0) {
      printf("error with page request \n");
    }
    printf("successfully sent request \n");

    /* wait till socket can be read */
    /* Hint: use select(), and ignore timeout for now. */
    printf("waiting for socket to be ready... \n");
    FD_ZERO(&set);
    FD_SET(sock, &set);
    printf("number of socket: %d \n", sock);
    select(sock + 1, &set, NULL, NULL, NULL);
    printf("reading socket \n");
    fflush(stdout);

    bool checkedfirst = false;
    /* first read loop -- read headers */
    while((rc = recv(sock, &buf, BUFSIZE, 0)) > 0) {
      //printf("received %d bytes \n", rc);
      buf[rc] = '\0';
      while((oneline = index(buf, '\n')) != NULL) {
        int index = oneline - buf + 1;
        /* first read server response */
        if (!checkedfirst) {
          checkedfirst = true;
          if ( strncmp(buf, "HTTP/1.0 200", 12) == 0 || strncmp(buf, "HTTP/1.0 3", 10) == 0 || strncmp(buf, "HTTP/1.1 200", 12) == 0 || strncmp(buf, "HTTP/1.1 3", 10) == 0) { printf("okay code obtained \n\n");}
          else {
            ok = false;
            printf(" error code received \n\n");
            outfd = 2;
          }
        }
        write_n_bytes(outfd, buf, index+1);
        for(int pos = 0; pos < BUFSIZE - index; pos++) {
          buf[pos] = buf[index + pos + 1];
        }
        if ( strncmp(buf, "\r\n", 2) == 0 || strncmp(buf, "\n", 1) == 0) {
          printf("reached end of header \n");
	  fflush(stdout);
          //break;
        }
      }
    }
    
    /* examine return code */   
    //Skip "HTTP/1.0"
    //remove the '\0'
    // Normal reply has return code 200

    /* print first part of response */

    /* second read loop -- print out the rest of the response */
    
    /*close socket and deinitialize */
    close(sock);
    free(req);

    if (ok) {
	return 0;
    } else {
	return -1;
    }
}

int write_n_bytes(int fd, char * buf, int count) {
    int rc = 0;
    int totalwritten = 0;

    while ((rc = minet_write(fd, buf + totalwritten, count - totalwritten)) > 0) {
	totalwritten += rc;
    }
    
    if (rc < 0) {
	return -1;
    } else {
	return totalwritten;
    }
}

int connect_socket( char *hostname, int port) {
  int sock;
  struct sockaddr_in sin;
  struct hostent *host;
  sock = socket( AF_INET, SOCK_STREAM, 0);
  if (sock == -1)
    return sock;
  host = gethostbyname( hostname);
  if (host == NULL) {
    close( sock);
    return -1;
  }
  memset (& sin, 0, sizeof( sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons( port);
  sin.sin_addr.s_addr = *( unsigned long *) host-> h_addr_list[0];
  if (connect(sock, (struct sockaddr *) &sin, sizeof(sin)) != 0) {
    close (sock);
    return -1;
  }
  return sock;
} 

