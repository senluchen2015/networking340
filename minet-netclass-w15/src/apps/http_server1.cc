#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>

#define BUFSIZE 1024
#define FILENAMESIZE 100

int handle_connection(int);
int writenbytes(int,char *,int);
int readnbytes(int,char *,int);
int make_listen_socket( int port);
int get_client_socket( int listen_socket);

int main(int argc,char *argv[])
{
  int server_port;
  int sock,sock2;
  struct sockaddr_in sa,sa2;
  int rc;

  /* parse command line args */
  if (argc != 3)
  {
    fprintf(stderr, "usage: http_server1 k|u port\n");
    exit(-1);
  }
  server_port = atoi(argv[2]);
  if (server_port < 1500)
  {
    fprintf(stderr,"INVALID PORT NUMBER: %d; can't be < 1500\n",server_port);
    exit(-1);
  }

  /* initialize and make socket */
  /* set server address*/
  /* bind listening socket */
  /* start listening */
  sock = make_listen_socket(server_port);

  /* connection handling loop */
  while(1)
  {
    /* handle connections */
    sock2 = get_client_socket(sock);
    rc = handle_connection(sock2);
  }
}

/* source: recitation slides */
int make_listen_socket( int port) {
  struct sockaddr_in sin;
  int sock;
  sock = socket( AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    return -1;
  memset(& sin, 0, sizeof( sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl( INADDR_ANY);
  sin.sin_port = htons( port);
  if (bind( sock, (struct sockaddr *) &sin, sizeof( sin)) < 0) {
    return -1;
  }
  // accept 5 connections on backlog
  listen(sock, 5);
  return sock;
} 

int get_client_socket( int listen_socket) {
  struct sockaddr_in sin;
  int sock;
  socklen_t sin_len;
  memset(& sin, 0, sizeof( sin));
  sin_len = sizeof( sin);
  if ((sock = accept( listen_socket, (struct sockaddr *) &sin, &sin_len)) < 0) {
    return -1;
  }
  return sock;
} 

int handle_connection(int sock2)
{
  char filename[FILENAMESIZE+1];
  int rc;
  int fd;
  struct stat filestat;
  char buf[BUFSIZE+1];
  char reqbuf[BUFSIZE+1];
  char *headers;
  char *endheaders;
  char *bptr;
  int datalen=0;
  char *ok_response_f = "HTTP/1.0 200 OK\r\n"\
                      "Content-type: text/plain\r\n"\
                      "Content-length: %d \r\n\r\n";
  char ok_response[100];
  char *notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"\
                         "Content-type: text/html\r\n\r\n"\
                         "<html><body bgColor=black text=white>\n"\
                         "<h2>404 FILE NOT FOUND</h2>\n"
                         "</body></html>\n";
  bool ok=true;
  char fullreq[BUFSIZE+1];
  bool firstrcv = true;

  /* first read loop -- get request and headers*/
  while( (rc = recv(sock2, &reqbuf, BUFSIZE, 0)) > 0) {
    reqbuf[rc] = '\0';
    if (firstrcv) {
      strcpy(fullreq, reqbuf);
      firstrcv = false;
    } else {
      strcat(fullreq, reqbuf);
    }
    printf(fullreq);
    fflush(stdout);
    if (strstr(fullreq, "\r\n\r\n")) {
      printf("detected end of request \n");
      break;
    }
  }

  /* parse request to get file name */
  char *req = strtok(fullreq, " ");
  // skip GET
  req = strtok(NULL, " ");
  if (!req) {
    ok = false;
  }
  if (ok) {
    /* Assumption: this is a GET request and filename contains no spaces*/
    printf("request: %s \n", req);
    fflush(stdout);
    /* direct requests relative to current directory of server */
    getcwd(filename, FILENAMESIZE);
    //printf("current directory: %s \n", filename);
    fflush(stdout);
    strcat(filename, "/");
    strcat(filename, req);
    printf("relative path requested: %s \n", filename);
    /* try opening the file */
    if (access(filename, F_OK) == -1) {
      printf("error: no file exists \n");
      ok = false;
    } else {
      printf("file found \n");
      stat(filename, &filestat);
    }
  }
  /* send response */
  while (ok)
  {
    /* send file */
    //printf("sending file \n");
    if ((fd = open(filename, O_RDONLY)) < 0 || !(S_ISREG(filestat.st_mode))) {
      printf("error reading requested file %s \n", filename);
      fflush(stdout);
      ok = false;
      break;
    }
    /* send headers */
    sprintf(ok_response, ok_response_f, filestat.st_size);
    //printf(ok_response);
    fflush(stdout);
    if (send(sock2,ok_response, strlen(ok_response), 0) < 0) {
      //printf("problem \n");
    }

    while ( (rc = read(fd, buf, BUFSIZE)) > 0) {
      //printf("reading file \n");
      //printf("reading n bytes: %d \n", rc);
      int result = send(sock2, buf, rc, 0);
      if (result < 0) {
        //printf("\n\n error sending file \n\n\n");
        fflush(stdout);
      } else {
        /*printf("result of send: %d \n", result);*/
      }
    }
    break;
  }
  if (!ok) // send error response
  {
    send(sock2, notok_response, strlen(notok_response), 0);
  }

  /* close socket and free space */
  close(sock2);

  if (ok)
    return 0;
  else
    return -1;
}

int readnbytes(int fd,char *buf,int size)
{
  int rc = 0;
  int totalread = 0;
  while ((rc = minet_read(fd,buf+totalread,size-totalread)) > 0)
    totalread += rc;

  if (rc < 0)
    return -1;
  else
    return totalread;
}

int writenbytes(int fd,char *str,int size)
{
  int rc = 0;
  int totalwritten =0;
  while ((rc = minet_write(fd,str+totalwritten,size-totalwritten)) > 0)
    totalwritten += rc;

  if (rc < 0)
    return -1;
  else
    return totalwritten;
}

