#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>

#define BUFSIZE 1024

int write_n_bytes(int fd, char * buf, int count);
int connect_ socket( char *hostname, int port);
char *format_request( char *path );

int main(int argc, char * argv[]) {
    char * server_name = NULL;
    int server_port = 0;
    char * server_path = NULL;

    int sock = 0;
    int rc = -1;
    int datalen = 0;
    bool ok = true;
    struct sockaddr_in sa;
    FILE * wheretoprint = stdout;
    struct hostent * site = NULL;
    char * req = NULL;

    char buf[BUFSIZE + 1];
    char * bptr = NULL;
    char * bptr2 = NULL;
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

    sock = connect_socket(server_name, server_port);
    /* create socket */

    // Do DNS lookup
    /* Hint: use gethostbyname() */

    /* set address */

    /* connect socket */
    
    /* send request */
    req = format_request(server_path);
    int count = send(sock, req, strlen(req), 0);

    /* wait till socket can be read */
    /* Hint: use select(), and ignore timeout for now. */
    FD_SET(sock, &set);
    select(sock + 1, &set, NULL, NULL, NULL);
    
    /* first read loop -- read headers */
    //while( 
    
    /* examine return code */   
    //Skip "HTTP/1.0"
    //remove the '\0'
    // Normal reply has return code 200

    /* print first part of response */

    /* second read loop -- print out the rest of the response */
    
    /*close socket and deinitialize */


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

int connect_ socket( char *hostname, int port) {
  int sock;
  struct sockaddr_in sin;
  struct hostent *host;
  sock = socket( AF_ INET, SOCK_ STREAM, 0);
  if (sock == -1)
    return sock;
  host = gethostbyname( hostname);
  if (host == NULL) {
    close( sock);
    return -1;
  }
  memset (& sin, 0, sizeof( sin));
  sin. sin_ family = AF_ INET;
  sin. sin_ port = htons( port);
  sin. sin_ addr. s_ addr = *( unsigned long *) host-> h_ addr_
  list[ 0];
  if (connect( sock, (struct sockaddr *) &sin, sizeof( sin)) != 0) {
    close (sock);
    return -1;
  }
  return sock;
} 

char *format_request( char *path ) {
  return sprintf("GET %s HTTP/1.0\r\n", path);
}
