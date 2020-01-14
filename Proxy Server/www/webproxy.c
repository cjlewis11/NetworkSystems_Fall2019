//Chad Lewis, PA3

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <unistd.h>      /* for read, write */
#include <sys/socket.h>  /* for socket use */
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <openssl/md5.h>

#define MAXLINE  8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */
#define MAX_CACHE 100

//Define out mutex and shared variables
pthread_mutex_t pageCacheLock;
pthread_mutex_t ipAddressCacheLock;
pthread_mutex_t fileReadWriteLock;
static int cacheFreshTime;
typedef struct CacheEntry{
  unsigned char* hostHash;
  char* ipaddrress;
  char* hostHashString;
  time_t lastTimeUsed;
  int hostToIpCacheIndex;
  char *fileType;
  char *fileLength;
  char *filename;
} CacheEntry;

typedef struct ipAddressCacheEntry{
  char* hostName;
  char* ip;
  int h_addrtype;
  long* h_addr;
} ipAddressCacheEntry;
struct CacheEntry pageCache[MAX_CACHE];




//A simple structure for our HTTPResponse headers
struct HTTPResponse{
  char main[MAXBUF/2];
  int lengthOfContent;
  char message[MAXBUF];
};


//A structure for the request that came in
struct HTTPRequest{
  char *requestType;
  char *hostName;
  unsigned char *md5Hash;
  char *md5string;
  char *destIP;
  char *port;
  int portVal;
  char *directory;
  int connfd;
  int hostToIpIndex;
  char *fileType;
  char *fileLength;
  char *filename;
  int isLinkPreFetch;
};

//Socket Stuff
int open_listenfd(int port);
int connect_fd(struct HTTPRequest *request);
int isBlackListed(char *ip, struct HTTPRequest *request);

//Drivers!
void driver(int connfd);
void *thread(void *vargp);

//Quick Senders!
void handle_error(struct HTTPRequest *request, int type, char *location);
void sendAccept(struct HTTPRequest *request);

//HTTP Request Stuff
void sendHTTPRequest(struct HTTPRequest *request);
void linkPreFetchThreadCreator(char *hostName);
void linkPreFetcher(char *link);

//Cache Stuff
void freeCacheEntry(int hashIndex);
void putInCache(struct HTTPRequest *request, int hashIndex);
void cacheAndSend(struct HTTPRequest *request, int hashIndex);
void sendFromCache(struct HTTPRequest *request, int hashIndex);
void sendFromPoll(struct HTTPRequest *request);
void checkCacheChoice(struct HTTPRequest *request);
int getHostToIPCacheLocation(struct HTTPRequest *request);

int main(int argc, char **argv){
    int listenfd, *connfdp, port, clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid;

    if (argc != 3) {
    	fprintf(stderr, "usage: %s <port> <cache timeout value>\n", argv[0]);
    	exit(0);
    }

    //Mutex Init
    if(pthread_mutex_init(&pageCacheLock, NULL) != 0){
      fprintf(stderr, "Page Cache Mutex init failed.\n" );
      exit(0);
    }
    if(pthread_mutex_init(&ipAddressCacheLock, NULL) != 0){
      fprintf(stderr, "IP Cache Mutex init failed.\n" );
      exit(0);
    }
    if(pthread_mutex_init(&fileReadWriteLock, NULL) != 0){
      fprintf(stderr, "IP Cache Mutex init failed.\n" );
      exit(0);
    }

    //Create our Caches
    port = atoi(argv[1]);
    cacheFreshTime = atoi(argv[2]);
    for(int i=0; i<MAX_CACHE;i++){
      struct CacheEntry a = {"","","",time(NULL),-1,"","",""};
      pageCache[i]=a;
    }
    // for(int i=0; i<MAX_CACHE;i++){
    //   long *t;
    //   struct ipAddressCacheEntry a = {"","",-1};
    //   hostToIpCache[i]=a;
    // }


    listenfd = open_listenfd(port);
    while (1) {
  	connfdp = malloc(sizeof(int));
  	*connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
  	pthread_create(&tid, NULL, thread, connfdp);
    }

}

/* thread routine */
void * thread(void * vargp){
    int connfd = *(int *)vargp;
    free(vargp);
    pthread_detach(pthread_self());
    driver(connfd);
    close(connfd);
    return NULL;
}

void *thread_prefetch(void *vargp){
  char *filename;
  filename = (char *)vargp;
  pthread_detach(pthread_self());
  linkPreFetchThreadCreator(filename);
  free(vargp);
  return NULL;
}

void *thread_preFetchHTTP(void *vargp){
  char *link;
  link = (char *)vargp;
  pthread_detach(pthread_self());
  linkPreFetcher(link);
  free(vargp);
  return NULL;
}

//-----------------------------------------------------------------------Quick Sends -------------------------------------------------------------------------
//Handle any errors and send them to the client.
void handle_error(struct HTTPRequest *request, int type, char *location){
  char *msg;
  if(type == 400)
    msg = "HTTP/1.1 400 Bad Request\n";
  if(type == 403)
    msg = "HTTP/1.1 403 Forbidden\n";
  if(type == 404)
    msg = "HTTP/1.1 404 Not Found\n";
  if(type == 500)
    msg = "HTTP/1.1 500 Internal Server Error\n";
  int len = strlen(msg);
  //printf("ERROR: %s\n", location); //Debugging
  send(request->connfd, msg, len,MSG_NOSIGNAL);
  return;
}

//Send an 200 Ok to client.
void sendAccept(struct HTTPRequest *request){
  char *msg;
  msg = "HTTP/1.1 200 Ok\n";
  int len = strlen(msg);
  write(request->connfd, msg, len);
  //printf("Sent Ok!\n");
  return;
}

//-----------------------------------------------------------------------Send HTTP Request -------------------------------------------------------------------------


/*
 *Build and Collect the Response from the HTTP Server
*/
void sendHTTPRequest(struct HTTPRequest *request){
  int totalRead = 0;
  char buf[MAXBUF];

  //printf("%s\n", "-------------------------IN HERE-------------------------");
  //Fun our Cacheing Operations
  checkCacheChoice(request);
  return;

}

void linkPreFetchThreadCreator(char *hostName){
  char * filename;
  strtok_r(hostName,":",&filename);
  FILE *f;
  char fileBuf[MAXBUF];
  int n = 0;
  //printf("Inside Prefetch: %s\n",filename );

  pthread_mutex_lock(&fileReadWriteLock);
  f = fopen(filename,"r");
  if(!f){
    printf("Link linkPreFetchThreadCreator Failed!\n");
    pthread_mutex_unlock(&fileReadWriteLock);
    return;
  }

  char *saveptr;
  char *token;

  char websiteName[200];
  pthread_t tid;
  char *fn;

  while(fgets(fileBuf,MAXBUF,f) != NULL){
    if(strstr(fileBuf,"href") != NULL){
      if(strstr(fileBuf,"<a") || strstr(fileBuf,"</a>")){
        if(strstr(fileBuf,"http") == NULL){
          token = strstr(fileBuf,"href=" )+strlen("href=")+1;
          token = strtok(token,"\"");
          if(strcmp(token,"#") != 0){
            sprintf(websiteName,"%s/%s",hostName,token);
            fn = (char *)malloc(strlen(websiteName)+1*sizeof(char));
            strcpy(fn,websiteName);
            pthread_create(&tid, NULL, thread_preFetchHTTP, fn);
          }
        } else {
          if(strstr(fileBuf,"https") == NULL){
            token = strstr(fileBuf,"//")+strlen("//");
            token = strtok(token,"\"");
            fn = (char *)malloc(strlen(token)+1*sizeof(char));
            strcpy(fn,token);
            pthread_create(&tid, NULL, thread_preFetchHTTP, fn);
          }
        }
      }
    }
  }
  fclose(f);
  pthread_mutex_unlock(&fileReadWriteLock);
}

void linkPreFetcher(char *link){
  char *directory;
  strtok_r(link,"/",&directory);
  //printf("hostName: %s\n", link);
  //printf("directory: %s\n", directory);
  struct HTTPRequest temp;
  temp.hostName = link;
  char dir[strlen(directory)+5];
  sprintf(dir,"/%s",directory);
  temp.directory = dir;
  temp.portVal = 80;
  temp.port = "";

  //Calculate the md5Hash
  unsigned char result[MD5_DIGEST_LENGTH];
  char hostandDir[strlen(temp.hostName)+strlen(temp.directory)+5];
  sprintf(hostandDir,"%s%s",temp.hostName, temp.directory);
  //printf("%s\n",hostandDir );
  //printf("Address to hash: %s\n",hostandDir );
  MD5((unsigned char*) hostandDir, strlen(hostandDir), result);
  temp.md5Hash = result;

  //Calculate the md5string
  char md5string[33];
  for(int i=0;i<16;i++)
    sprintf(&md5string[i*2],"%02x", (unsigned int)result[i]);
  temp.md5string = md5string;
  //printf("Resulting Hash: %s\n",md5string );
  temp.destIP = "";

  //For Debugging
  // printf("Request Type: %s\n", request.requestType);
  // printf("Host: %s\n", request.hostName);
  // printf("Port: %s\n", request.port);
  // printf("Directory: %s\n", request.directory);

  //Send the HTTP Request and Gather the Result.
  //clock_t t = clock(); //Debugging
  temp.fileType = "";
  temp.fileLength = "";
  temp.isLinkPreFetch = 1;
  checkCacheChoice(&temp);
  return;
}
//-----------------------------------------------------------------------Cache Operations -------------------------------------------------------------------------

void freeCacheEntry(int hashIndex){
  free(pageCache[hashIndex].hostHash);
  free(pageCache[hashIndex].ipaddrress);
  free(pageCache[hashIndex].hostHashString);
  free(pageCache[hashIndex].fileType);
  free(pageCache[hashIndex].fileLength);
}

void putInCache(struct HTTPRequest *request, int hashIndex){

  //Allocate our memory!
  pageCache[hashIndex].hostHash = (unsigned char*)malloc(strlen(request->md5Hash)+1*sizeof(unsigned char));
  pageCache[hashIndex].ipaddrress = (char *)malloc(strlen(request->destIP)+1*sizeof(char));
  pageCache[hashIndex].hostHashString = (char *)malloc(strlen(request->md5string)+1*sizeof(char));

  strcpy(pageCache[hashIndex].hostHash,request->md5Hash);
  strcpy(pageCache[hashIndex].ipaddrress,request->destIP);
  strcpy(pageCache[hashIndex].hostHashString,request->md5string);
  pageCache[hashIndex].lastTimeUsed = time(NULL);

  //Allocate our memory...
  pageCache[hashIndex].fileType = (char *)malloc(strlen(request->fileType)+1*sizeof(char));
  pageCache[hashIndex].fileLength = (char *)malloc(strlen(request->fileLength)+1*sizeof(char));
  pageCache[hashIndex].filename = (char *)malloc(strlen(request->filename)+1*sizeof(char));
  //printf("-------------------------6----------------------\n");
  strcpy(pageCache[hashIndex].fileType,request->fileType);
  strcpy(pageCache[hashIndex].fileLength,request->fileLength);
  strcpy(pageCache[hashIndex].filename,request->filename);
  //printf("Before Copy%s\n",pageCache[hashIndex].fileType);
  //printf("Before Copy%s\n",pageCache[hashIndex].fileLength);
  free(request->fileType);
  free(request->fileLength);
  //printf("After Copy%s\n",pageCache[hashIndex].fileType);
  //printf("After Copy%s\n",pageCache[hashIndex].fileLength);
}

//We will cache the results from a poll
//We will send the results from the cache after the poll
//Call sendFromCache to send it!
void cacheAndSend(struct HTTPRequest *request, int hashIndex){
  //printf("Caching then Sending: %s%s\n", request->hostName,request->directory);
  //printf("HASH: %s\n", request->md5string);
  //Open our file for writing!

  //printf("%s\n",filename); //Debugging

  //Establish a connection with the HTTP Server
  int httpServerConn = connect_fd(request);
  if(httpServerConn == -1){
    close(httpServerConn);
    handle_error(request, 404, "In sendHTTPRequest httpServerConn == -1");
    return;
  }
  if(httpServerConn == -2){
    handle_error(request,403,"Black Listed Site!");
    return;
  }

  //Let the Server know we've accepted the request.
  //sendAccept(request);

  //Craft our request
  char getRequest[250];
  sprintf(getRequest,"GET %s HTTP/1.1\r\nHost: %s%s\r\n\r\n",request->directory,request->hostName,request->port); //{directory,,host,port}


  //printf("Sending Request!\n%s",getRequest);
  FILE *f;
  char filename[strlen(request->md5string)+40];

  //Send our request, then read it into the file!
  char *fileType;
  char *fileLength;
  char *bufTemp;
  int totalRead;
  size_t toReadAfter;
  char buf[MAXBUF]="";
  char head[MAXBUF]="";
  char another[MAXBUF];
  char *temp;
  char *headTemp;

  write(httpServerConn,getRequest,strlen(getRequest));

  _Bool chunk = false;
  _Bool notpastHeaders = true;
  _Bool image = false;
  _Bool havBoth = false;
  _Bool isHTML = false;

  //Get our file information.
  totalRead = read(httpServerConn, buf, MAXLINE);
  if(request->isLinkPreFetch == 1){
    if(strstr(buf,"HTTP/1.1 301 TLS Redirect") != NULL)
      return;
  }
  //printf("%s\n", buf);
  memcpy(head,buf,sizeof(head));
  //Get Content-Length
  if(strstr(head,"Content-Length:")!=NULL){
    temp=(char *)malloc(strlen(buf)+1*sizeof(char));
    strcpy(temp,head);
    fileLength = strstr(temp,"Content-Length:");
    fileLength = strtok(fileLength,"\r\n");
    request->fileLength = (char *)malloc(strlen(fileLength)+1*sizeof(char));
    strcpy(request->fileLength,fileLength);
    free(temp);
  }
  //Check if it's chunked!
  if(strstr(head,"Transfer-Encoding: chunked") != NULL){
    chunk = true;
    char c[]="Transfer-Encoding: chunked";
    request->fileLength = (char *)malloc(strlen(c)+1*sizeof(char));
    strncpy(request->fileLength,c,strlen(c));
  }
  //Get the Content Type
  if(strstr(head,"Content-Type:") != NULL){
    temp=(char *)malloc(strlen(buf)+1*sizeof(char));
    strcpy(temp,head);
    fileType = strstr(temp,"Content-Type:");
    fileType = strtok(fileType,"\r\n");
    request->fileType = (char *)malloc(strlen(fileType)+1*sizeof(char));
    strcpy(request->fileType,fileType);
    free(temp);
    if(strstr(request->fileType, "image") != NULL){
      image=true;
    } else if(strstr(request->fileType, ";") != NULL){
      //printf("%s\n",request->fileType );
      strtok(request->fileType,";");
    }
    //Link PreFetching!
    if(strstr(request->fileType,"html") != NULL){
      isHTML = true;
    }
  }
  if(request->fileLength == NULL || request->fileType == NULL){
    handle_error(request,400,"NULL fileLength and fileType");
    return;
  }
  //if(image)
  sprintf(filename,"./cache/%s.%s",request->md5string,strstr(request->fileType,"/")+1);
  //else
  //  sprintf(filename,"./cache/%s",request->md5string);
  request->filename = filename;

  pthread_mutex_lock(&fileReadWriteLock);
  f = fopen(filename,"wb");
  if(!f){
    printf("Can't Cache, switching to poll!\n");
    pthread_mutex_unlock(&fileReadWriteLock);
    if(request->isLinkPreFetch == 0){
        sendFromPoll(request);
      }
    return;
  }
  memcpy(another,head,sizeof(another));
  if((bufTemp = strstr(another,"\r\n\r\n")+4),bufTemp != NULL){
    toReadAfter=(size_t)(bufTemp-another);
    headTemp = &buf[toReadAfter];
  }
  fwrite(headTemp,totalRead-toReadAfter,1,f);


  while((totalRead = read(httpServerConn, buf, MAXLINE)), totalRead > 0){
    fwrite(buf,1,totalRead,f);
    if(chunk && strstr(buf,"\r\n0\r\n") != NULL){
      break;
    }
    bzero(buf,MAXBUF);
  }
  fclose(f);
  close(httpServerConn);
  if(totalRead == -1){
    handle_error(request,500,"In sendHTTPRequest on Read from HTTP Server");
    return;
  }

  if(isHTML && request->isLinkPreFetch == 0){
    //Do link PreFetching
    char websiteFileName[200];
    sprintf(websiteFileName,"%s:%s",request->hostName,request->filename);
    pthread_t tid;
    char *fn;
    fn = (char *)malloc(strlen(websiteFileName)+1*sizeof(char));
    strcpy(fn,websiteFileName);
    pthread_create(&tid, NULL, thread_prefetch, fn);
  }
  //After we've placed it in the cache, send the file from the cache!
  pthread_mutex_lock(&pageCacheLock);
  putInCache(request,hashIndex);
  if(request->isLinkPreFetch == 0){
    sendFromCache(request,hashIndex);
  }
  pthread_mutex_unlock(&pageCacheLock);
  pthread_mutex_unlock(&fileReadWriteLock);
  return;
}

//We wil not Poll
//We will send the file from the cache
void sendFromCache(struct HTTPRequest *request, int hashIndex){
  //printf("Sending from Cache with no Poll: %s%s\n", request->hostName,request->directory);
  //printf("HASH: %s\n", request->md5string);
  char fileBuf[MAXBUF];
  int n =0;
  FILE *f;
  //printf("%s\n",filename); //Debugging
  f = fopen(pageCache[hashIndex].filename,"rb");
  if(!f){
    printf("Can't find file, switching to poll only!\n");
    sendFromPoll(request);
    return;
  }

  //Craft a better response
  //printf("Request: %s%s\n", request->hostName,request->directory);
  struct HTTPResponse response;
  char temp[MAXBUF];
  char *fileLen = pageCache[hashIndex].fileLength;
  char *fileType = pageCache[hashIndex].fileType;
  //printf("Before Send fileLen:%s\n", fileLen);
  //printf("Before Send fileType:%s\n", fileType);
  if(strcmp(fileLen,"Transfer-Encoding: chunked") == 0){
    sprintf(temp,"HTTP/1.1 200 Document Follows\r\n%s\r\n%s\r\n",fileType,fileLen);
  }
  else{
    sprintf(temp,"HTTP/1.1 200 Document Follows\r\n%s\r\n%s\r\n\r\n",fileLen,fileType);
  }
  strncpy(response.message,temp,strlen(temp));
  // printf("SENDING:\n%s", response.message); //Debugging
  send(request->connfd,temp,strlen(temp),MSG_NOSIGNAL);
  //printf("FIRST MESSAGE\n");

  while(n=fread(fileBuf,1,MAXBUF,f), n>0){
    //printf("%s\n",fileBuf );
    if((n = send(request->connfd, fileBuf,n,MSG_NOSIGNAL)), n<0){
      handle_error(request,500,"sendFromCache - write to client");
      printf("Failed in:%s%s\n",request->hostName,request->directory );
      break;
    }
  }
  //printf("Time of After Send %ld\n",time(NULL));
  fclose(f);
  return;
}

//We will not Cache
//We will send the file straight from a poll.
void sendFromPoll(struct HTTPRequest *request){
  //printf("Sending Straight from Poll no Cache: %s%s\n", request->hostName,request->directory);
  //printf("Sending Straight from Poll no Cache: %s\n", request->md5string);
  int httpServerConn = connect_fd(request);
  if(httpServerConn == -1){
    close(httpServerConn);
    handle_error(request, 404, "In sendHTTPRequest httpServerConn == -1");
    return;
  }
  if(httpServerConn == -2){
    handle_error(request,403,"Black Listed Site!");
    return;
  }
  //Let the Server know we've accepted the request.
  sendAccept(request);

  //Craft our request
  char getRequest[250];
  sprintf(getRequest,"GET %s HTTP/1.1\r\nHost: %s%s\r\n\r\n",request->directory,request->hostName,request->port); //{directory,,host,port}

  //printf("Sending Request!\n%s",getRequest);
  int totalRead = 0;
  char buf[MAXBUF];
  send(httpServerConn,getRequest,strlen(getRequest),0);
  totalRead = read(httpServerConn,buf, MAXLINE);
  while(totalRead > 0){
    send(request->connfd,buf,totalRead,MSG_NOSIGNAL);
    totalRead = read(httpServerConn,buf, MAXLINE);
  }
  if(totalRead == -1){
    handle_error(request,500,"In sendHTTPRequest on Read from HTTP Server");
    return;
  }
  close(httpServerConn);
  return;
}

//Check our cache for the item.
//If it is, indicate it is and return the index
//If it isn't but we can put it in, return -1
//If it isn't, and we can't put it in, return -2;
void checkCacheChoice(struct HTTPRequest *request){

  //Calculate our HashIndex!
  //printf("%s\n", "-------------------------IN HERE 4-------------------------");
  int hashSum = 0;
  int choice = 0;
  for(int i=0; i<MD5_DIGEST_LENGTH;i++){
    hashSum += abs((int)request->md5Hash[i]);
  }
  int hashIndex = hashSum % MAX_CACHE;
  pthread_mutex_lock(&pageCacheLock);
  if(strcmp(pageCache[hashIndex].hostHash,"")==0){ //If its empty we can update. Send to cacheAndSend
    choice = 1;
  } else if(pageCache[hashIndex].lastTimeUsed + cacheFreshTime < time(NULL)){ //If it's not empty, but its overdue, we can. Send to cacheAndSend\
    choice = 1;
    freeCacheEntry(hashIndex);

  } else { //If it's not overdue we can't replace! Send to sendFromPoll
    choice = 3;
  }

  if(strcmp(pageCache[hashIndex].hostHash,request->md5Hash)==0){ //If it's the same hash do some actions!
    if(pageCache[hashIndex].lastTimeUsed + cacheFreshTime < time(NULL)){ //Check if we need to repoll
      //Send to cacheAndSend!
      choice = 1;
      freeCacheEntry(hashIndex);

    } else{ //The cache isn't overdue and we can pull from it!
      //Send only from the cache!
      choice = 2;

    }
  }
  //Unlock our mutex!
  pthread_mutex_unlock(&pageCacheLock);

  //Decide on our action!
  switch (choice){
    case 1: cacheAndSend(request,hashIndex);
      break;
    case 2:
      pthread_mutex_lock(&pageCacheLock);
      sendFromCache(request,hashIndex);
      pthread_mutex_unlock(&pageCacheLock);
      break;
    case 3: sendFromPoll(request);
      break;
    default: handle_error(request,500,"checkCacheChoice - no choice made");
  }
  return;
}

//-----------------------------------------------------------------------Driver -------------------------------------------------------------------------

/*
 * Driver - drive our server
 */
void driver(int connfd){
  size_t n;
  char buf[MAXLINE];
  struct HTTPRequest request;
  request.connfd = connfd;
  /*
  In this section of code we read in the request for processing.
  */
  n = read(connfd, buf, MAXLINE);
  //printf("Bytes read from request!: %d\n", n);
  if(n == 0){
    handle_error(&request,400,"driver - no intial buffer.");
    return;
  }
  request.isLinkPreFetch = 0;
  //Set some variables for reading in file
  char delim[] = "/";
  char *saveptr;
  //printf("%s\n", buf);

  //Check if we have a web request!
  //Check if its a GET Request
  request.requestType = strtok_r(buf, " ", &saveptr);
  if(strstr(request.requestType,"GET")==NULL){ //Check to make sure our request is a GET before anything else.
      handle_error(&request, 400, "In driver, GET == NULL");
      return;
    }
    //printf("Time of Get %ld\n",time(NULL) );
    //printf("%s\n",saveptr );

  //Collect the Host and Port information
  char * host = strstr(saveptr,"//");
  host = host+2;
  if(host == NULL){
    handle_error(&request, 400, "In driver, Host == NULL");
    return;
  }

  char *portCopy = (char *)malloc(strlen(host)+1*sizeof(char));
  char *port;
  char *directoryCopy = (char *)malloc(strlen(host)+1*sizeof(char));
  char *dir;
  strncpy(portCopy,host,strlen(host));
  strncpy(directoryCopy,host,strlen(host));
  //Decide Host
  host = strtok(host, "/");
  if(strstr(host,":")!=NULL){
    host = strtok(host,":");
  }
  request.hostName = host;

  //REMOVE IN FUTURE _________
  // char temp[50];
  // sprintf(temp,"http://%s",host);
  // printf("%s\n", temp);
  // request.hostName = temp;

  //Decide Port
  port = strtok(portCopy,"/");
  port = strstr(portCopy,":");
  int portVal = 80;
  if(port == NULL){
    port = "";
  } else {
    portVal = atoi(port+1);
    if(portVal == 443){
      handle_error(&request,400,"Port is 443, no support of secure connections.\n");
      return;
    }
  }
  //Get the Port Value as an integer
  request.port = port;
  request.portVal = portVal;


  //Get Directory
  dir = strstr(directoryCopy, "/");
  dir = strtok(dir, " ");
  request.directory = dir;

  //Calculate the md5Hash
  unsigned char result[MD5_DIGEST_LENGTH];
  char hostandDir[strlen(request.hostName)+strlen(request.directory)+5];
  sprintf(hostandDir,"%s%s",request.hostName, request.directory);
  //printf("Address to hash: %s\n",hostandDir );
  MD5((unsigned char*) hostandDir, strlen(hostandDir), result);
  request.md5Hash = result;

  //Calculate the md5string
  char md5string[33];
  for(int i=0;i<16;i++)
    sprintf(&md5string[i*2],"%02x", (unsigned int)result[i]);
  request.md5string = md5string;
  //printf("Resulting Hash: %s\n",md5string );
  request.destIP = "";

  //For Debugging
  // printf("Request Type: %s\n", request.requestType);
  // printf("Host: %s\n", request.hostName);
  // printf("Port: %s\n", request.port);
  // printf("Directory: %s\n", request.directory);

  //Send the HTTP Request and Gather the Result.
  //clock_t t = clock(); //Debugging
  request.fileType = "";
  request.fileLength = "";
  sendHTTPRequest(&request);
  // t = clock()-t; //Debugging
  // double time_taken = ((double)t)/CLOCKS_PER_SEC; //Debugging
  // printf("sendHTTPRequest took: %f in thread: %lu\n", time_taken,pthread_self()); //Debugging

  //Free our dynamically allocated memory.
  free(portCopy);
  free(directoryCopy);

  //Create a space in any new debugging information
  return;
}


/*
 * open_listenfd - open and return a listening socket on port
 * Returns -1 in case of failure
 */
int open_listenfd(int port){
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

//Check if the host is Blacklisted!
int isBlackListed(char *ip, struct HTTPRequest *request){
  FILE *f;
  f = fopen("blacklist.txt", "r");
  char hostname[strlen(request->hostName)+strlen(request->directory)+5];
  sprintf(hostname,"%s%s",request->hostName,request->directory);

  char buf[MAXLINE];
  while(fgets(buf,MAXBUF,f) != NULL){
    if(strcmp(buf,ip) == 0)
      return 1;
    if(strcmp(buf,hostname) == 0)
      return 1;
  }
  return 0;
}


//Connect to the HTTP Server we want to request from.
//We perform this if we need to add to a cash
//Or if we can't put it in the cache!
int connect_fd(struct HTTPRequest *request){

  int connectfd, optval=1;
  struct sockaddr_in serveraddr;
  char hostname[strlen(request->hostName)+strlen(request->directory)+5];
  sprintf(hostname,"%s%s",request->hostName,request->directory);

  FILE *f;
  f = fopen("hostToIpCache.txt","r");
  char buf[MAXBUF];
  char *temp;
  char *toChange;
  char *ip;
  int h_addrtype;
  long h_addr;
  _Bool inCache = false;
  while(fgets(buf,MAXBUF,f) != NULL){
    toChange = strtok_r(buf,":",&temp);
    //printf("%s\n",toChange );
    //printf("%s\n", hostname);
    if(strcmp(toChange,hostname)==0){
      inCache = true;
      ip = strtok_r(NULL,":",&temp);
      toChange = strtok_r(NULL,":",&temp);
      serveraddr.sin_family = atoi(toChange);
      toChange = strtok_r(NULL,":",&temp);
      serveraddr.sin_addr.s_addr = atol(toChange);
      serveraddr.sin_port = htons(request->portVal);
      break;
    }
  }
  fclose(f);
  if(!inCache){
    struct hostent *host_entity;
    ip=(char*)malloc(NI_MAXHOST*sizeof(char));
    if((host_entity =  gethostbyname(request->hostName)) == NULL){
      return -1;
    }
    strcpy(ip, inet_ntoa(*(struct in_addr *) host_entity->h_addr));
    request->destIP = ip;
    //Assign the structure
    serveraddr.sin_family = host_entity->h_addrtype;
    serveraddr.sin_port = htons(request->portVal);
    serveraddr.sin_addr.s_addr = *(long*)host_entity->h_addr;
    f = fopen("hostToIpcache.txt","a");
    if(!f){
      printf("Failed to Cache in IP!\n");
      pthread_mutex_unlock(&fileReadWriteLock);
      return -1;
    }
    char results[MAXLINE];
    sprintf(results,"%s:%s:%d:%ld\n",hostname,ip,host_entity->h_addrtype,*(long*)host_entity->h_addr);
    fputs(results,f);
    fclose(f);
  }

  int blist = isBlackListed(ip,request);
  if(blist){
    return -2;
  }
  // Create a socket descriptor
  if((connectfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return -1;

  /* Eliminates "Address already in use" error from bind. */
  if (setsockopt(connectfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0)
      return -1;


    if(inet_pton(AF_INET, ip, &serveraddr.sin_addr)<=0){
      printf("Invalid Address/ Not Supported Address:%s\n", request->hostName);
      return -1;
    }
    if(connect(connectfd,(struct sockaddr *)&serveraddr,sizeof(serveraddr)) <0 ){
      printf("Connection failed to host:%s with Port: %d\n", request->hostName, request->portVal);
      return -1;
    }
    if(!inCache)
      free(ip);
    return connectfd;
  }
