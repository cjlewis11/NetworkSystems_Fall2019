//Chad Lewis, PA2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <unistd.h>      /* for read, write */
#include <sys/socket.h>  /* for socket use */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define MAXLINE  8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */


//A simple structure for our HTTPResponse headers
struct HTTPResponse{
  char main[MAXBUF/2];
  int lengthOfContent;
  char message[MAXBUF];
};


//A structure for the request that came in
struct HTTPRequest{
  char *requestType;
  char *directory;
  char *filename;
  int connfd;
};

int open_listenfd(int port);
void driver(int connfd);
void *thread(void *vargp);
void handle_error(struct HTTPRequest *request);
void set_response_main(char *type, struct HTTPResponse *response);
void get_request_sender(struct HTTPRequest *request);

int main(int argc, char **argv)
{
    int listenfd, *connfdp, port, clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid;

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }
    port = atoi(argv[1]);

    listenfd = open_listenfd(port);
    while (1) {
	connfdp = malloc(sizeof(int));
	*connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
	pthread_create(&tid, NULL, thread, connfdp);
    }
}

/* thread routine */
void * thread(void * vargp)
{
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self());
    free(vargp);
    driver(connfd);
    close(connfd);
    return NULL;
}


//Build our HTTP Response code
void set_response_main(char *type, struct HTTPResponse *response){
  //HTML
  //printf("Type: %s\n", type); //Debugging
  if(strstr(type, ".html") != NULL){
    strcpy(response->main, "HTTP/1.1 200 Document Follows\r\nContent-Type:text/html\r\nContent-Length:");
    return;
  }
  //TXT
  if(strstr(type, ".txt") != NULL){
    strcpy(response->main, "HTTP/1.1 200 Document Follows\r\nContent-Type:text/plain\r\nContent-Length:");
    return;
  }
  //PNG
  if(strstr(type, ".png") != NULL){
    strcpy(response->main, "HTTP/1.1 200 Document Follows\r\nContent-Type:image/png\r\nContent-Length:");
    return;
  }
  //GIF
  if(strstr(type, ".gif") != NULL){
    strcpy(response->main, "HTTP/1.1 200 Document Follows\r\nContent-Type:image/gif\r\nContent-Length:");
    return;
  }
  //JPEG
  if(strstr(type, ".jpg") != NULL){
    strcpy(response->main, "HTTP/1.1 200 Document Follows\r\nContent-Type:image/jpg\r\nContent-Length:");
    return;
  }
  //CSS
  if(strstr(type, ".css") != NULL){
    strcpy(response->main, "HTTP/1.1 200 Document Follows\r\nContent-Type:text/css\r\nContent-Length:");
    return;
  }
  //JS
  if(strstr(type, ".js") != NULL){
    strcpy(response->main, "HTTP/1.1 200 Document Follows\r\nContent-Type:application/javascript\r\nContent-Length:");
    return;
  }

}

void get_request_sender(struct HTTPRequest *request){
  char temp[MAXLINE];
  char buf[MAXLINE];
  sprintf(temp,"./%s/%s",request->directory, request->filename);
  struct HTTPResponse response;

  //Build First Response
  set_response_main(request->filename, &response);

  //Get File Input and Length
  FILE *f;
  f = fopen(temp, "rb");
  if(!f){
    //printf("FILE %s/%s FAILED TO OPEN\n", request->directory,request->filename); //Debugging
    handle_error(request);
    return;
  }
  fseek(f,0,SEEK_END);
  response.lengthOfContent= ftell(f);
  fseek(f,0,SEEK_SET);

  //Send the header and length of the file.
  sprintf(response.message, "%s%d\r\n\r\n", response.main, response.lengthOfContent);
  strcpy(buf,response.message);
  //printf("Response Message:%s\n", response.message);
  write(request->connfd, buf, strlen(response.message));


  //Send the file content with a loop and buffer
  size_t n;
  size_t total = 0;
  while(n = fread(temp, 1, MAXBUF/2, f), n > 0){
    //printf("N before: %ld\n", n);
    if(n=write(request->connfd, temp, n), n < 0){
      handle_error(request);
      return;
    }
    total += n;
  }
  //printf("Total: %ld Size of File: %d\n", total, response.lengthOfContent); //Debugging
  fclose(f);
}

void handle_error(struct HTTPRequest *request){
  char msg[] = "HTTP/1.1 500 Internal Server Error";
  int len = strlen(msg);
  write(request->connfd, msg, len);
  return;
}


/*
 * Driver - drive our server
 */
void driver(int connfd)
{
  size_t n;
  char buf[MAXLINE];
  struct HTTPRequest request;
  request.connfd = connfd;


  /*
  In this section of code we read in the request for processing.
  */
  n = read(connfd, buf, MAXLINE);
  //printf("server received the following request:\n%s\n",buf); ///Debugging
  //Set some variables for reading in file
  char delim[] = "/";
  char *saveptr;
  //GET or PUT
  request.requestType = strtok_r(buf, delim, &saveptr);
  if(request.requestType == NULL){
    handle_error(&request);
    return;
  }
  //File Location or PATH
  request.directory = strtok_r(NULL, delim, &saveptr);
  //We want to see if there is no directory passed, which means we display the default webpage otherwise we get the filename.
  if(strcmp(request.directory," HTTP") != 0){
    //Actual filename
    request.filename = strtok_r(NULL, delim, &saveptr);
    request.filename = strtok_r(request.filename, " ", &saveptr);
  } else { //This is our default
    request.directory = ".";
    request.filename = "index.html";
  }
  //printf("request type:%s directory:%s, filename:%s\n", request.requestType,request.directory,request.filename);
  if(strstr(request.requestType, "GET")!=NULL){
    get_request_sender(&request);
  }
  return;
}


/*
 * open_listenfd - open and return a listening socket on port
 * Returns -1 in case of failure
 */
int open_listenfd(int port)
{
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;

    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    /* listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);
    if (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;
    return listenfd;
} /* end open_listenfd */
