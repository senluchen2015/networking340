#include "minet_socket.h"
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>


#define FILENAMESIZE 100
#define BUFSIZE 1024

typedef enum \
{NEW,READING_HEADERS,WRITING_RESPONSE,READING_FILE,WRITING_FILE,CLOSED} states;

typedef struct connection_s connection;
typedef struct connection_list_s connection_list;

struct connection_s
{
  int sock;
  int fd;
  char filename[FILENAMESIZE+1];
  char buf[BUFSIZE+1];
  char *endheaders;
  bool ok;
  long filelen;
  states state;
  int headers_read,response_written,file_read,file_written;

  connection *next;
};

struct connection_list_s
{
  connection *first,*last;
};

connection *get_connection(int, connection_list*);
int make_listen_socket(int);
int get_client_socket( int);

void add_connection(int,connection_list *);
void insert_connection(int,connection_list *);
void init_connection(connection *con);


int writenbytes(int,char *,int);
int readnbytes(int,char *,int);
void read_headers(connection *);
void write_response(connection *);
void read_file(connection *);
void write_file(connection *);

int main(int argc,char *argv[])
{
  int server_port;
  int sock,sock2;
  struct sockaddr_in sa,sa2;
  int rc;
  fd_set readlist,writelist;
  connection_list connections;
  connection *i;
  int maxfd;

  /* parse command line args */
  if (argc != 3)
  {
    fprintf(stderr, "usage: http_server3 k|u port\n");
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
			
  connections.first = NULL;	
  connections.last = NULL;
  
	insert_connection(sock, &connections);	

  maxfd = sock;

  /* connection handling loop */
  while(1)
  {
    /* create read and write lists */
    
    FD_ZERO(&readlist);
    FD_ZERO(&writelist);
    
    FD_SET(sock, &readlist);
    i = connections.first;
		printf("looping through i states\n");
		fflush(stdout);
    while(i){
			if(i->state == NEW){
				init_connection(i);
				FD_SET(i->sock, &readlist);
				i->state = READING_HEADERS;
			}
			else if(i->state == READING_HEADERS){
				FD_SET(i->sock, &readlist);
			}
			else if(i->state == READING_FILE){
				FD_SET(i->fd, &readlist);
  		}
			else if(i->state == WRITING_RESPONSE || i->state == WRITING_FILE){
				FD_SET(i->sock, &writelist);
			}
			i = i->next;
    }
		printf("before select\n");
		fflush(stdout);
    /* do a select */
		select(maxfd + 1, &readlist, &writelist, NULL, NULL);
		
    /* process sockets that are ready */
  	i = connections.first;
		while(i){  
	  	if (FD_ISSET(i->sock, &readlist)) {
        /* for the accept socket, add accepted connection to connections */
     	 	if (i->sock == sock)
       	{
          if ((sock2 = get_client_socket(sock)) > 0) {
            printf("received connection request \n");
            if (sock2 > maxfd) {
              maxfd = sock2;
            }
            insert_connection(sock2, &connections);
          }
        }
        else /* for a connection socket, handle the connection */
        {
          printf("received write request \n");
          read_headers(i); 
        }
     	}
			else if(FD_ISSET(i->fd, &readlist)){
				read_file(i);
			}
			else if(FD_ISSET(i->sock, &writelist)){
				/* handle writelist */
				if(i->state == WRITING_RESPONSE){
					write_response(i);
				}
				else{	
					write_file(i);
				}
			}
			i = i->next;
		}  
	}
}

connection *get_connection(int sock, connection_list *con_list){
	connection *i = con_list -> first;
	while(i){
		if(i->sock == sock){
			return i;
		}
		i = i->next;
	}
	return NULL;
}

void read_headers(connection *con)
{
 	char temp_buf[BUFSIZE + 1];
	struct stat filestat;
	int flag;
	int rc;
	 /* first read loop -- get request and headers*/
  while( (rc = recv(con->sock, &temp_buf, BUFSIZE, 0)) > 0) {
    temp_buf[rc] = '\0';
    
		if (con->headers_read == 0) {
      strcpy(con->buf, temp_buf);
			con->headers_read = rc;
    } 
		else {
      strcat(con->buf, temp_buf);
			con->headers_read += rc;
    }
    printf(con->buf);
    fflush(stdout);
    if (strstr(con->buf, "\r\n\r\n")) {
      printf("detected end of request \n");
      break;
    }
  }
	if(rc<0){
		if(errno == EAGAIN){
			printf("error EAGAIN \n");	
			return;
		}
	}
	/* parse request to get file name */
	char *req = strtok(con->buf, " ");
  
	/* Assumption: this is a GET request and filename contains no spaces*/
	req = strtok(NULL, " ");
	if(!req){
		con->ok = false;
	}
	else{
		con->ok = true;
		/* Assumption: this is a GET request and filename contains no spaces*/
		printf("request: %s \n", req);
		fflush(stdout);
		/* direct requests relative to current directory of server */
		getcwd(con->filename, FILENAMESIZE);
		//printf("current directory: %s \n", filename);
		fflush(stdout);
		strcat(con->filename, "/");
		strcat(con->filename, req);
		printf("relative path requested: %s \n", con->filename);
		 /* try opening the file */
		if (access(con->filename, F_OK) == -1) {
			printf("error: no file exists \n");
			con->ok = false;
		} else {
			printf("file found \n");
			stat(con->filename, &filestat);
		}
	}
   
 
  /* get file name and size, set to non-blocking */
  if ((con->fd = open(con->filename, O_RDONLY)) < 0 || !(S_ISREG(filestat.st_mode))) {
		printf("error reading requested file %s \n", con->filename);
		fflush(stdout);
		con->ok = false;
	}   
	else{
	/* set to non-blocking, get size */
		con->state = WRITING_RESPONSE;
		flag = fcntl(con->fd, F_GETFL, 0);
		fcntl(con->fd, F_SETFL, flag | O_NONBLOCK);
		con->filelen = filestat.st_size;
	}
      
  
  write_response(con);
}

void write_response(connection *con)
{
  int sock2 = con->sock;
  int rc;
  int written = con->response_written;
  char *ok_response_f = "HTTP/1.0 200 OK\r\n"\
                      "Content-type: text/plain\r\n"\
                      "Content-length: %d \r\n\r\n";
  char ok_response[100];
  char *notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"\
                         "Content-type: text/html\r\n\r\n"\
                         "<html><body bgColor=black text=white>\n"\
                         "<h2>404 FILE NOT FOUND</h2>\n"\
                         "</body></html>\n";
  /* send response */
  if (con->ok)
  {
    /* send headers */
    sprintf(ok_response, ok_response_f, con->filelen);
    printf(ok_response);
    fflush(stdout);
		
		if((rc = send(sock2, &ok_response[written], strlen(&ok_response[written]), 0)) > 0) {	
			printf("sending with %d rc \n", rc);
			fflush(stdout);
			con->response_written += rc;
		}
		else if(rc<0){
			if(errno == EAGAIN){
				printf("error EAGAIN\n");
				fflush(stdout);
				return;
			}
			else{
				con->ok = false;	
			}
		}
		else{
			printf("successfully send responses\n");
			printf("con -> fd: %d\n",con->fd);
			fflush(stdout);

			con->state = READING_FILE;
			read_file(con);
		}
	}
  else
  {
		if(rc = send(sock2, &notok_response[written], strlen(&notok_response[written]), 0));
		{
			con->response_written += rc;
		}
		if(rc<0){
			if(errno == EAGAIN){
				return;
			}
			else{
				con->ok = false;	
			}
		}
		else{
			con->state = READING_FILE;
			read_file(con);
		}
		close(con->sock);
		con->state = CLOSED; 
	} 
	 
}

void read_file(connection *con)
{
  int rc;

    /* send file */
  rc = read(con->fd,con->buf,BUFSIZE);
  if (rc < 0)
  { 
    if (errno == EAGAIN)
      return;
    fprintf(stderr,"error reading requested file %s\n",con->filename);
    return;
  }
  else if (rc == 0)
  {
    con->state = CLOSED;
    minet_close(con->sock);
  }
  else
  {
    con->file_read = rc;
    con->state = WRITING_FILE;
    write_file(con);
  }
}

void write_file(connection *con)
{
  int towrite = con->file_read;
  int written = con->file_written;
  int rc = writenbytes(con->sock, con->buf+written, towrite-written);
  if (rc < 0)
  {
    if (errno == EAGAIN)
      return;
    minet_perror("error writing response ");
    con->state = CLOSED;
    minet_close(con->sock);
    return;
  }
  else
  {
    con->file_written += rc;
    if (con->file_written == towrite)
    {
      con->state = READING_FILE;
      con->file_written = 0;
      read_file(con);
    }
    else
      printf("shouldn't happen\n");
  }
}

int readnbytes(int fd,char *buf,int size)
{
  int rc = 0;
  int totalread = 0;
  while ((rc = read(fd,buf+totalread,size-totalread)) > 0)
    totalread += rc;

  if (rc < 0)
  {
    return -1;
  }
  else
    return totalread;
}  

int writenbytes(int fd,char *str,int size)
{
  int rc = 0;
  int totalwritten =0;
  while ((rc = write(fd,str+totalwritten,size-totalwritten)) > 0)
    totalwritten += rc;
  
  if (rc < 0)
    return -1;
  else
    return totalwritten;
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
  int flag;
  socklen_t sin_len;
  memset(& sin, 0, sizeof( sin));
  sin_len = sizeof( sin);
  if ((sock = accept( listen_socket, (struct sockaddr *) &sin, &sin_len)) < 0) {
    return -1;
  }

	flag = fcntl(sock, F_GETFL, 0);
  if(flag < 0){
		flag = 0;
	}

 	fcntl(sock, F_SETFL, flag | O_NONBLOCK);  
  return sock;
}


// inserts a connection in place of a closed connection
// if there are no closed connections, appends the connection 
// to the end of the list

void insert_connection(int sock,connection_list *con_list)
{
  connection *i;
  for (i = con_list->first; i != NULL; i = i->next)
  {
    if (i->state == CLOSED)
    {
      i->sock = sock;
      i->state = NEW;
      return;
    }
  }
  add_connection(sock,con_list);
}
 
void add_connection(int sock,connection_list *con_list)
{
  connection *con = (connection *) malloc(sizeof(connection));
  con->next = NULL;
  con->state = NEW;
  con->sock = sock;
  if (con_list->first == NULL)
    con_list->first = con;
  if (con_list->last != NULL)
  {
    con_list->last->next = con;
    con_list->last = con;
  }
  else
    con_list->last = con;
}

void init_connection(connection *con)
{
  con->headers_read = 0;
  con->response_written = 0;
  con->file_read = 0;
  con->file_written = 0;
}
