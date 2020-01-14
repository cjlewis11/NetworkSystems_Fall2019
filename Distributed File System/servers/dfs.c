//Chad Lewis, PA3

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <unistd.h>      /* for read, write */
#include <sys/socket.h>  /* for socket use */
#include <sys/stat.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <dirent.h>
#include <openssl/md5.h>

#define MAXLINE  8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */
#define MAX_CACHE 100

//Define out mutex and shared variables
char *directory;
pthread_mutex_t lock;
int filedef [4][4][2];





//A simple structure for our HTTPResponse headers
struct HTTPResponse{
  char main[MAXBUF/2];
  int lengthOfContent;
  char message[MAXBUF];
};


//A structure for the request that came in
struct HTTPRequest{
  char *requestType;
  unsigned char *md5Hash;
  char *md5string;
  char *directory;
  int connfd;
  char *fileType;
  char *fileLength;
  char *filename;
};

//Socket Stuff
int open_listenfd(int port);

//Drivers!
void driver(int connfd);
void *thread(void *vargp);
void populateDefs();

//Commands
_Bool authenticateUser(char *username, char *password);
void getFilesLocations(struct HTTPRequest *request, char *buf);
void getFiles(struct HTTPRequest *request, char *buf);
void putFiles(struct HTTPRequest *request, char *buf);
void listFiles(struct HTTPRequest *request,char *buf);
_Bool checkFile(char *username, char *file);

//Quick Senders!
void handle_error(struct HTTPRequest *request, int type, char *location);
void sendAccept(struct HTTPRequest *request);

//Freers
void freeRequest(struct HTTPRequest *request);

//Quick tools
void getFileParts(int fileset, int *parts);

int main(int argc, char **argv){
    int listenfd, *connfdp, port, clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid;

    if (argc != 3) {
    	fprintf(stderr, " usage: %s <directory path> <port>\n", argv[0]);
    	exit(0);
    }

    //Mutex Init
    if(pthread_mutex_init(&lock, NULL) != 0){
      fprintf(stderr, "Page Cache Mutex init failed.\n" );
      exit(0);
    }

    //Create our directory
    directory = (char *)malloc(strlen(argv[1]) * sizeof(char)+1);
    strcpy(directory,argv[1]);
    port = atoi(argv[2]);
    populateDefs();

    listenfd = open_listenfd(port);
    while (1) {
  	connfdp = malloc(sizeof(int));
  	*connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
  	pthread_create(&tid, NULL, thread, connfdp);
    }

}

void populateDefs(){
  //Populate our fileset def!
  //X Value == 0
  filedef[0][0][0] = 1;
  filedef[0][0][1] = 2;
  filedef[0][1][0] = 2;
  filedef[0][1][1] = 3;
  filedef[0][2][0] = 3;
  filedef[0][2][1] = 4;
  filedef[0][3][0] = 4;
  filedef[0][3][1] = 1;
  //X Value == 1
  filedef[1][0][0] = 4;
  filedef[1][0][1] = 1;
  filedef[1][1][0] = 1;
  filedef[1][1][1] = 2;
  filedef[1][2][0] = 2;
  filedef[1][2][1] = 3;
  filedef[1][3][0] = 3;
  filedef[1][3][1] = 4;
  //X Value == 2
  filedef[2][0][0] = 3;
  filedef[2][0][1] = 4;
  filedef[2][1][0] = 4;
  filedef[2][1][1] = 1;
  filedef[2][2][0] = 1;
  filedef[2][2][1] = 2;
  filedef[2][3][0] = 2;
  filedef[2][3][1] = 3;
  //X Value == 3
  filedef[3][0][0] = 2;
  filedef[3][0][1] = 3;
  filedef[3][1][0] = 3;
  filedef[3][1][1] = 4;
  filedef[3][2][0] = 4;
  filedef[3][2][1] = 1;
  filedef[3][3][0] = 1;
  filedef[3][3][1] = 2;
}

/* thread routine */
void * thread(void * vargp){
    int connfd = *(int *)vargp;;
    free(vargp);
    pthread_detach(pthread_self());
    driver(connfd);
    close(connfd);
    return NULL;
}


//-----------------------------------------------------------------------Quick Sends -------------------------------------------------------------------------
//Handle any errors and send them to the client.
void handle_error(struct HTTPRequest *request, int type, char *location){
  char *msg;
  if(type == 400)
    msg = "400 Bad Request\n";
  if(type == 403)
    msg = "404 Invalid Username/Password. Please try again.\n";
  if(type == 404)
    msg = "404 Not Found\n";
  if(type == 500)
    msg = "500 Internal Server Error\n";
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

//-----------------------------------------------------------------------Driver -------------------------------------------------------------------------

/*
 * Driver - drive our server
 */
void driver(int connfd){
  size_t n = 1;
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

  //Check for KILL!
  if(strcmp(buf,"KILL")==0){
    printf("%s\n","EXIT SIGNAL SENT FROM CLIENT!" );
    exit(0);
  }

  //Set some variables for reading in file
  char delim[] = "/";
  char *saveptr;

  //Check if what request we have!
  //Check the request type!
  request.requestType = strtok_r(buf, "\n", &saveptr);
  //Check is its a user auth
  char *acceptMsg;

    if(strstr(request.requestType,"LOCATE")!=NULL){ //Check to make sure our request is a GET before anything else.
        getFilesLocations(&request, saveptr);
    }
    if(strstr(request.requestType,"PUT")!=NULL){ //Check to make sure our request is a GET before anything else.
          putFiles(&request, saveptr);
      }
    if(strstr(request.requestType,"LIST")!=NULL){ //Check to make sure our request is a GET before anything else.
          listFiles(&request, saveptr);
      }
    if(strstr(request.requestType,"GET")!=NULL){ //Check to make sure our request is a GET before anything else.
        getFiles(&request, saveptr);
    }
  return;
}


//------------------------------------------------------------------------Commands------------------------------------------------------------------
_Bool authenticateUser(char *username, char *password){
    char userPass[strlen(password)+strlen(username)+1];
    sprintf(userPass,"%s:%s",username,password);

    FILE *f;
    char filename[MAXBUF];
    char *dir;

    sprintf(filename,"./dfs.conf");
    f = fopen(filename,"r");
    if(!f){
      return false;
    }

    char fileBuf[MAXBUF];
    char *fb;
    while(fgets(fileBuf,MAXBUF,f) != NULL){
      if(strstr(fileBuf,"\r") != NULL){
        fb = strtok(fileBuf,"\r");
      } else {
        fb = fileBuf;
      }
      if(strcmp(fb,userPass)==0){
        return true;
      }
    }

    fclose(f);
    return false;

}

void getFilesLocations(struct HTTPRequest *request, char *buf){
  char respBuf[MAXBUF];
  char *username;
  char *password;
  char *file;
  char *therest;

  file = strtok_r(buf,"\n",&therest);
  file = strstr(file,":")+1;
  username = strtok_r(NULL,"\n",&therest);
  username = strstr(username,":")+1;
  password = strtok_r(NULL,"\n",&therest);
  password = strstr(password,":")+1;

  if(authenticateUser(username,password)){
    char msg[] = "AUTHORIZED_OK";
    write(request->connfd,msg,strlen(msg));
  } else {
    char msg[] = "AUTHORIZED_FAIL";
    write(request->connfd,msg,strlen(msg));
  }

  if(!checkFile(username,file)){
    sprintf(respBuf,"NO_SUCH_FILE");
    //printf("%s\n",respBuf );
    write(request->connfd,respBuf,strlen(respBuf));
    return;
  }

  char filedir[MAXBUF];
  sprintf(filedir,".%s/%s/%s/parts.txt",directory,username,file);
  FILE *f;
  f=fopen(filedir,"r");

  char *parts;
  char fileBuf[MAXBUF];
  int n;
  while(n=fread(fileBuf,1,MAXBUF,f), n>0){
    sprintf(respBuf,"PARTS:%s\n",fileBuf);
    write(request->connfd,respBuf,strlen(respBuf));
  }

}
void getFiles(struct HTTPRequest *request, char *buf){
  char respBuf[MAXBUF];
  char *username;
  char *password;
  char *file;
  char *therest;
  char *part;


  file = strtok_r(buf,"\n",&therest);
  file = strstr(file,":")+1;
  username = strtok_r(NULL,"\n",&therest);
  username = strstr(username,":")+1;
  password = strtok_r(NULL,"\n",&therest);
  password = strstr(password,":")+1;
  part = strtok_r(NULL,"\n",&therest);
  part = strstr(part,":")+1;


  char filedir[MAXBUF];
  sprintf(filedir,".%s/%s/%s/%s.%s",directory,username,file,part,file);
  //printf("%s\n", filedir);
  FILE *f;
  f=fopen(filedir,"rb");
  if(!f){
    printf("%s\n", "FAILED IN GET");
  }
  fseek(f,0,SEEK_END);
  //printf("FILESIZE:%ld\n", ftell(f));
  fseek(f,0,SEEK_SET);

  char fileBuf[MAXBUF];
  int n = 0;
  int out = 0;
  //printf("%s\n","HERE" );
  while(n=fread(fileBuf,1,MAXBUF,f), n>0){
    //printf("%s\n", fileBuf);
    out += write(request->connfd,fileBuf,n);
  }
  close(request->connfd);
  //printf("Wrote from %s:%d\n", directory, out);
  fclose(f);

}

void putFiles(struct HTTPRequest *request, char *buf){
  char readBuf[MAXBUF];
  int totalRead;
  char *therest;
  char *nums;
  char *hash;
  char *fileset;
  char *filename;
  char *username;
  char *password;
  char *part;
  int parts[] = {-1,-1};
  int partToWriteOne;
  int partToWriteTwo;
  int twoStart,threeStart,fourStart;

  //Get information on the file!
  username = strtok_r(buf,"\n",&therest);
  username = strstr(username,":")+1;
  password = strtok_r(NULL,"\n",&therest);
  password = strstr(password,":")+1;
  hash = strtok_r(NULL,"\n",&therest);
  hash = strstr(hash,":")+1;
  fileset = strtok_r(NULL,"\n",&therest);
  fileset = strstr(fileset,":")+1;
  filename = strtok_r(NULL,"\n",&therest);
  filename = strstr(filename,":")+1;
  part = strtok_r(NULL,"\n",&therest);
  part = strstr(part,":")+1;

  if(authenticateUser(username,password)){
    char msg[] = "AUTHORIZED_OK";
    write(request->connfd,msg,strlen(msg));
  } else {
    char msg[] = "AUTHORIZED_FAIL";
    write(request->connfd,msg,strlen(msg));
    return;
  }

  //Get file set and parts to use
  getFileParts(atoi(fileset), parts);
  partToWriteOne = parts[0];
  partToWriteTwo = parts[1];
  //Prepare a Directory for the files!
  FILE *f;
  char path[MAXBUF];
  char dir[MAXBUF];
  char userDir[MAXBUF];
  sprintf(userDir,".%s/%s", directory,username);
  sprintf(dir,".%s/%s/%s/",directory,username,filename);
  sprintf(path,".%s/%s/%s/parts.txt",directory,username,filename);
  //Check if Directory exists, if not make it.
  struct stat st = {0};
  if(stat(userDir, &st) == -1){
    mkdir(userDir,0700);
  }
  if(stat(dir, &st) == -1){
    mkdir(dir,0700);
  }

  f = fopen(path, "w");
  if(!f){
    printf("%s%s\n","SOMETHING WENT WRONG IN PUTTING IN A FILE FOR ",directory);
    return;
  }

  char partsMsg[5];
  sprintf(partsMsg,"%d:%d",partToWriteOne,partToWriteTwo);
  fwrite(partsMsg,1,sizeof(partsMsg),f);
  fclose(f);

  char fOne[MAXBUF];
  bzero(readBuf,MAXBUF);
  sprintf(fOne,"%s%s.%s",dir,part,filename);
  f = fopen(fOne,"wb");
  if(!f){
    printf("%s%s\n","FAILED TO OPEN WRITE TO FILE ONE IN ", dir);
    return;
  }

  //Start Reading in Files!
  int n = 0;
  totalRead=0;
  int toRead = 1024;
  int toWrite=0;
  //Read Part 1!

  while(n=read(request->connfd,readBuf,MAXBUF), n>0){
    fwrite(readBuf,1,n,f);
    totalRead+=n;
  }
  //printf("Read from %s:%d\n", directory, totalRead );
  fclose(f);
}

void listFiles(struct HTTPRequest *request,char *buf){
  char readBuf[MAXBUF];
  int totalRead;
  char *therest;
  char *username;
  char *password;

  //Get information on the file!
  username = strtok_r(buf,"\n",&therest);
  username = strstr(username,":")+1;
  password = strtok_r(NULL,"\n",&therest);
  password = strstr(password,":")+1;

  if(authenticateUser(username,password)){
    char msg[] = "AUTHORIZED_OK";
    write(request->connfd,msg,strlen(msg));
  } else {
    char msg[] = "AUTHORIZED_FAIL";
    write(request->connfd,msg,strlen(msg));
    return;
  }

  struct dirent *de;
  char serverDir[MAXBUF];
  sprintf(serverDir,".%s/%s/",directory,username);
  DIR *dr = opendir(serverDir);
  if(dr == NULL){
    printf("%s\n", "BEEP BOOP");
    return;
  }

  FILE *fparts;
  char filename[MAXBUF];
  char fileBuf[MAXBUF];
  int n=0;
  int dirCount = 0;
  //Count Directorys!
  while((de =readdir(dr)) != NULL){
    if(strcmp(de->d_name,".")==0 || strcmp(de->d_name,"..")==0)
      continue;
    dirCount+=1;
  }
  closedir(dr);
  dr = opendir(serverDir);

  if(dirCount == 0){
    ///RETURN!!!!!!!
  }
  //FILECOUNT:X\nFILE:exam.gif\nPARTS:1:2\nFILE:wine3.jpg
  char filesInServer[MAXBUF];
  int curDir = 0;

  sprintf(filesInServer,"FILECOUNT:%d\n",dirCount);
  write(request->connfd,filesInServer,strlen(filesInServer));
  bzero(filesInServer,MAXBUF);

  while((de =readdir(dr)) != NULL){
    if(strcmp(de->d_name,".")==0 || strcmp(de->d_name,"..")==0)
      continue;

    //Add directory to dirs
    sprintf(filesInServer + strlen(filesInServer),"FILE:%s\n",de->d_name);

    //Get File Parts!
    sprintf(filename,"%s%s/parts.txt",serverDir,de->d_name);
    fparts = fopen(filename,"r");
    while(n=fread(fileBuf,1,MAXBUF,fparts), n>0){
      sprintf(filesInServer + strlen(filesInServer), "PARTS:%s\n",fileBuf);
    }
    fclose(fparts);
    curDir+=1;
  }
  closedir(dr);
  //printf("%s\n", filesInServer);
  write(request->connfd,filesInServer,strlen(filesInServer));
  return;



}

_Bool checkFile(char *username, char *file){
  char filedir[MAXBUF];
  sprintf(filedir,".%s/%s/%s/parts.txt",directory,username,file);
  FILE *f;
  f =fopen(filedir,"r");
  if(!f){
    printf("%s: %s\n", "CAN'T FIND FILE ON", directory);
    return false;
  }
  fclose(f);
  return true;
}
//------------------------------------------------------------------------Freers------------------------------------------------------------------

//------------------------------------------------------------------------Tools------------------------------------------------------------------
void getFileParts(int fileset, int *parts){
  if(strcmp(directory,"/DFS1") == 0){
    parts[0] = filedef[fileset][0][0];
    parts[1] = filedef[fileset][0][1];
  }
  if(strcmp(directory,"/DFS2") == 0){
    parts[0] = filedef[fileset][1][0];
    parts[1] = filedef[fileset][1][1];
  }
  if(strcmp(directory,"/DFS3") == 0){
    parts[0] = filedef[fileset][2][0];
    parts[1] = filedef[fileset][2][1];
  }
  if(strcmp(directory,"/DFS4") == 0){
    parts[0] = filedef[fileset][3][0];
    parts[1] = filedef[fileset][3][1];
  }
  return;
}

//=========================================================================Listen FD------------------------------------------------------------------------------------------
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
