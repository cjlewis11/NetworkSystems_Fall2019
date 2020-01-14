/*
 * dfc.c - A client for interacting with the DFS servers!
 * Chad Lewis: PA4
 */
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

#define MAXBUF 1024
#define DEBUGGING true
#define XORKEY 'P'
int filedef [4][4][2];
int serverPart[4][4][2];


typedef struct Configuration{
  //DFS1 Information
  char* addressOne;
  int portOne;
  //DFS2 Information
  char* addressTwo;
  int portTwo;
  //DFS3 Information
  char* addressThree;
  int portThree;
  //DFS4 Information
  char* addressFour;
  int portFour;
  //User Information
  char* username;
  char* password;
} Configuration;

typedef struct Connection{
  //DFS1 Information
  char *addresses[4];
  int ports[4];

  int *one_ptr;
  int one_connfd;
  //DFS2 Information
  int *two_ptr;
  int two_connfd;
  //DFS3 Information
  int *three_ptr;
  int three_connfd;
  //DFS4 Information
  int *four_ptr;
  int four_connfd;

  //Int array of alive or dead
  int alive[4];
  int connfds[4];
  //User Information!
  char* username;
  char* password;
} Connection;


//Error Handling and DEBUGGING
void errorHandler(char *msg);
void debugger(char *msg);

//Quick sends
void sendToAll(Connection *connectStatus, char *msg);
void readFromAll(Connection *connectStatus);
//Command Rounting and Handling
void choiceRouter(Connection *connectStatus);
void command_put(Connection *connectStatus, char *filename);
void command_list(Connection *connectStatus);

//Connection and Authentication
void connectToDFSServers(Configuration *conf, Connection *connectStatus);
void connectForCommand(Connection *connectStatus);
void disconnect(Connection *connectStatus);
int connect_fd(char* address, int port);

//Builders
void buildConfiguration(Configuration *conf, char *filename);
void populateDefs();

//Memory Freers
void freeConf(Configuration *conf);
void freeConnection(Connection *connectStatus);

int main(int argc, char **argv) {
    char *confFile;
    /* check command line arguments */
    if (argc != 2) {
       fprintf(stderr,"usage: %s <Configuration File>\n", argv[0]);
       exit(0);
    }
    confFile = argv[1];
    //Pop Defs
    populateDefs();

    //Build our Configuration settings
    Configuration conf;
    buildConfiguration(&conf,confFile);

    //Connect to the Servers and Authenticate the User
    Connection connectStatus;
    for(int i=0;i<4;i++)
      connectStatus.connfds[i] = 0;
    connectToDFSServers(&conf,&connectStatus);

    //Free Memory in conf!
    freeConf(&conf);

    /* get a message from the user */
      int choice = 0;
      _Bool end;
      while(true){
      choiceRouter(&connectStatus);
    }
    return 0;
}

//-----------------------------------------------------------------------------------HANDLE ERRORS ----------------------------------------------------------------------------------
/*
 * error - wrapper for perror
 */
void errorHandler(char *msg) {
      printf("%s\n", msg);
}

void debugger(char *msg){
  if(DEBUGGING){
    printf("%s\n", msg);
  }
}

//-----------------------------------------------------------------------------------Quick Senders ----------------------------------------------------------------------------------
void sendToAll(Connection *connectStatus, char *msg){
  int n;

  for(int i=0;i<4;i++){
    if(connectStatus->connfds[i]>0){
      printf("%d\n", connectStatus->connfds[i]>0);
      if(write(connectStatus->connfds[i],msg,strlen(msg)) < 0){
        printf("ERROR ON SEND TO: %d\n", i+1);
      }
    }
  }
  // if(connectStatus->one_connfd > 0){
  //   n = write(connectStatus->one_connfd,msg,strlen(msg));
  //   if(n<0){
  //     printf("%s\n", "ERROR ON SENDTOALL");
  //   }
  // }
  //
  // if(connectStatus->two_connfd > 0){
  //   n = write(connectStatus->two_connfd,msg,strlen(msg));
  //   if(n<0){
  //     printf("%s\n", "ERROR ON SENDTOALL");
  //   }
  // }
  //
  // if(connectStatus->three_connfd > 0){
  //   n = write(connectStatus->three_connfd,msg,strlen(msg));
  //   if(n<0){
  //     printf("%s\n", "ERROR ON SENDTOALL");
  //   }
  // }
  //
  // if(connectStatus->four_connfd > 0){
  //   n = write(connectStatus->four_connfd,msg,strlen(msg));
  //   if(n<0){
  //     printf("%s\n", "ERROR ON SENDTOALL");
  //   }
  // }
}

void readFromAll(Connection *connectStatus){
  int n;
  char msgBuf[MAXBUF];
  if(connectStatus->one_connfd > 0){
    n = read(connectStatus->one_connfd,msgBuf,MAXBUF);
    if(n<0){
      printf("%s\n", "ERROR ON READFROM");
    }
  }

  if(connectStatus->two_connfd > 0){
    n = write(connectStatus->two_connfd,msgBuf,MAXBUF);
    if(n<0){
      printf("%s\n", "ERROR ON READFROM");
    }
  }

  if(connectStatus->three_connfd > 0){
    n = write(connectStatus->three_connfd,msgBuf,MAXBUF);
    if(n<0){
      printf("%s\n", "ERROR ON READFROM");
    }
  }

  if(connectStatus->four_connfd > 0){
    n = write(connectStatus->four_connfd,msgBuf,MAXBUF);
    if(n<0){
      printf("%s\n", "ERROR ON READFROM");
    }
  }
}

//-----------------------------------------------------------------------------------COMMAND HANDLING ----------------------------------------------------------------------------------

/*
* This function handles the get command, and all the actions needed for it.
  Return false on failure.
  True on success.
*/
void command_get(Connection *connectStatus, char *file){
  if(strstr(file,"\n") != NULL){
    file = strtok(file,"\n");
  } else {
    file = file;
  }
  file = strstr(file," ")+1;
  char sendMsg[MAXBUF];
  char respBuf[MAXBUF];
  sprintf(sendMsg,"LOCATE\nFILE:%s\nUsername:%s\nPassword:%s\n",file,connectStatus->username, connectStatus->password);
  connectForCommand(connectStatus);

  //AUTH CONNECTION!
  for(int i=0;i<4;i++){
    bzero(respBuf,MAXBUF);
    if(connectStatus->connfds[i] > 0){
      if(write(connectStatus->connfds[i],sendMsg,strlen(sendMsg)) > 0){
        read(connectStatus->connfds[i],respBuf,MAXBUF);
        if(strcmp(respBuf,"AUTHORIZED_OK")!=0){
          close(connectStatus->connfds[i]);
          connectStatus->connfds[i] = -1;
        }
      }
    }
  }

  _Bool authed = false;
  for(int i=0;i<4;i++){
    if(connectStatus->connfds[i] > 0){
      authed = true;
    }
  }
  if(!authed){
    printf("%s\n", "YOU'RE EITHER NOT AUTHORIZED TO CONNECT\n OR SERVERS ARE DOWN!");
    return;
  }

  //Once authorized, get the components!
  bzero(respBuf,MAXBUF);
  char *partChar;
  char *parts2;
  char *therest;
  int p1,p2;
  int serverPart[4][4] = {0,0,0,0};
  for(int i=0;i<4;i++){
    if(connectStatus->connfds[i] > 0){
      read(connectStatus-> connfds[i], respBuf, MAXBUF);
      if(strcmp(respBuf,"NO_SUCH_FILE") == 0)
        continue;
      partChar = strtok_r(respBuf,"\n", &therest);
      partChar = strstr(partChar,":")+1;
      p1 = atoi(strtok_r(partChar,":",&parts2))-1;
      p2 = atoi(parts2)-1;
      serverPart[i][p1] = 1;
      serverPart[i][p2] = 2;
    }
  }
  disconnect(connectStatus);

  //Get the Servers we will get the file from!
  int serverHolder[4] = {-1,-1,-1,-1};
  int serverHolderP1orP2[4] = {-1,-1,-1,-1};
  for(int i=0;i<4;i++){
    for(int j=0;j<4;j++){
      if(serverPart[i][j] != 0){
        serverHolder[j]=i;
        serverHolderP1orP2[j] = serverPart[i][j];
      }
    }
  }

  //Check if we have the entire file!
  _Bool haveAll = true;
  for(int i=0;i<4;i++){
    if(serverHolder[i] == -1){
      haveAll = false;
    }
  }
  if(!haveAll){
    printf("Unable to get file %s, it is incomplete!\n", file);
    return;
  }
  //printf("Server Locations: [%d,%d,%d,%d]\n",serverHolder[0],serverHolder[1],serverHolder[2],serverHolder[3]);
  //printf("Server Parts: [%d,%d,%d,%d]\n",serverHolderP1orP2[0],serverHolderP1orP2[1],serverHolderP1orP2[2],serverHolderP1orP2[3]);

  connectForCommand(connectStatus);




  int n;
  bzero(respBuf,MAXBUF);
  FILE *f;
  char filename[MAXBUF];
  char fileBuf[MAXBUF];
  sprintf(filename,"./files/%s",file);
  int totalwrote = 0;
  f = fopen(filename,"wb");
  for(int i=0;i<4;i++){
    sprintf(sendMsg,"GET\nFILE:%s\nUsername:%s\nPassword:%s\nPART:%d\n",file,connectStatus->username, connectStatus->password,serverHolderP1orP2[i]);
    if(serverHolder[i] > -1){
      if(connectStatus->connfds[serverHolder[i]] <= 0){
        connectStatus->connfds[serverHolder[i]] = connect_fd(connectStatus->addresses[serverHolder[i]],connectStatus->ports[serverHolder[i]]);
      }
      write(connectStatus->connfds[serverHolder[i]], sendMsg,strlen(sendMsg));

      while(n=read(connectStatus->connfds[serverHolder[i]],respBuf,MAXBUF), n>0){
        for(int i=0;i<n;i++){
          respBuf[i] = respBuf[i] ^ XORKEY;
        }
        fwrite(respBuf,1,n,f);
        totalwrote+=n;
      }
      connectStatus->connfds[serverHolder[i]] = -1;
    }
  }
  fclose(f);
  //printf("%d\n", totalwrote);
}

/*
* This function handles the get command, and all the actions needed for it.
*/
void command_put(Connection *connectStatus, char *file){
  int n = 0;
  FILE *f;
  char fileBuf[MAXBUF];
  int filesize;
  if(strstr(file,"\n") != NULL){
    file = strtok(file,"\n");
  } else {
    file = file;
  }

  file = strstr(file," ")+1;
  char filename[MAXBUF];
  sprintf(filename,"./files/%s",file);
  MD5_CTX mdContext;
  unsigned char c[MD5_DIGEST_LENGTH];

  f = fopen(filename,"r");
  if(!f){
    printf("No Such File in files folder!\n");
    return;
  }

  //Get File Length and part sizes
  fseek(f,0,SEEK_END);
  filesize = ftell(f);
  //printf("FS: %d\n", filesize);
  fseek(f,0,SEEK_SET);
  int partTwo, partThree, partFour, partSize;
  partSize = filesize /4;
  partTwo = partSize;
  partThree = partSize*2;
  partFour = partSize*3;

  MD5_Init (&mdContext);
  while(n=fread(fileBuf,1,MAXBUF,f), n>0){
    MD5_Update (&mdContext, fileBuf, n);
  }
  fclose(f);
  MD5_Final(c,&mdContext);
  char md5string[33];
  unsigned int hashVal = 0;
  for(int i = 0; i < 16; ++i){
     hashVal += (unsigned int)c[i];
     sprintf(&md5string[i*2], "%02x", (unsigned int)c[i]);
  }
  hashVal = hashVal % 4;

  //Read in and Send Part One!
  //printf("H:%d\n", hashVal);
  int parts[4][2];
  for(int i=0; i<4; i++){
    for(int j=0;j<2;j++){
      parts[i][j] = serverPart[hashVal][i][j];
    }
  }

  //Some VARS!
  char sendMsg[MAXBUF];
  char requestString[MAXBUF];
  char respBuf[MAXBUF];

  // printf("FS:%d\n", filesize);
  // printf("partTwo:%d\n",partTwo );
  // printf("partThree:%d\n",partThree );
  // printf("partFour:%d\n",partFour );
  // printf("HASH:%d\n",hashVal );
  for(int xx = 0; xx < 2; xx++){
    sprintf(sendMsg,"PUT\nUsername:%s\nPassword:%s\nHASH:%s\nFILESET:%d\nFILENAME:%s\nPART:%d",connectStatus->username, connectStatus->password,md5string,hashVal,file,xx+1);
    connectForCommand(connectStatus);

    //AUTH CONNECTION!
    for(int i=0;i<4;i++){
      bzero(respBuf,MAXBUF);
      if(connectStatus->connfds[i] > 0){
        if(write(connectStatus->connfds[i],sendMsg,strlen(sendMsg)) > 0){
          read(connectStatus->connfds[i],respBuf,MAXBUF);
          if(strcmp(respBuf,"AUTHORIZED_OK")!=0){
            close(connectStatus->connfds[i]);
            connectStatus->connfds[i] = -1;
          }
        }
      }
    }

    _Bool authed = false;
    for(int i=0;i<4;i++){
      if(connectStatus->connfds[i] > 0){
        authed = true;
      }
    }

    if(!authed){
      printf("%s\n", "YOU'RE EITHER NOT AUTHORIZED TO CONNECT\n OR SERVERS ARE DOWN!");
      return;
    }
    //Read File for Sending
    f = fopen(filename,"rb");
    if(!f){
      printf("%s\n", "FAILED TO OPEN FILE TO SEND!");
    }
    int totalRead = 0;
    int toWrite = 0;
    int toRead = MAXBUF;
    int written = 0;
    while(n=fread(fileBuf,1,toRead,f), n>0){
      totalRead += n;
      if(partSize - totalRead < MAXBUF){
        toRead = partSize - totalRead;
      }
      for(int i=0;i<n;i++){
        fileBuf[i] = fileBuf[i] ^ XORKEY;
      }
      write(connectStatus->connfds[parts[0][xx]],fileBuf,n);
    }
    close(connectStatus->connfds[parts[0][xx]]);
    connectStatus->connfds[parts[0][xx]] = -1;

    //Send Part Two!
    toRead = MAXBUF;
    while(n=fread(fileBuf,1,toRead,f), n>0){
      totalRead += n;
      if(partThree - totalRead < MAXBUF){
        toRead = partThree - totalRead;
      }
      for(int i=0;i<n;i++){
        fileBuf[i] = fileBuf[i] ^ XORKEY;
      }
      write(connectStatus->connfds[parts[1][xx]],fileBuf,n);
    }
    close(connectStatus->connfds[parts[1][xx]]);
    connectStatus->connfds[parts[1][xx]] = -1;

    //Send Part Three!
    toRead = MAXBUF;
    while(n=fread(fileBuf,1,toRead,f), n>0){
      totalRead += n;
      if(partFour - totalRead < MAXBUF){
        toRead = partFour - totalRead;
      }
      for(int i=0;i<n;i++){
        fileBuf[i] = fileBuf[i] ^ XORKEY;
      }
      write(connectStatus->connfds[parts[2][xx]],fileBuf,n);
    }
    close(connectStatus->connfds[parts[2][xx]]);
    connectStatus->connfds[parts[2][xx]] = -1;

    //Send Part Four!
    toRead = MAXBUF;
    while(n=fread(fileBuf,1,toRead,f), n>0){
      totalRead += n;
      if(filesize - totalRead < MAXBUF){
        toRead = filesize - totalRead;
      }
      for(int i=0;i<n;i++){
        fileBuf[i] = fileBuf[i] ^ XORKEY;
      }
      write(connectStatus->connfds[parts[3][xx]],fileBuf,n);
    }
    close(connectStatus->connfds[parts[3][xx]]);
    connectStatus->connfds[parts[3][xx]] = -1;
    //printf("TR:%d\n", totalRead);
    fclose(f);
    disconnect(connectStatus);
  }
}

void command_list(Connection *connectStatus){
  char sendMsg[MAXBUF];
  char respBuf[MAXBUF];
  sprintf(sendMsg,"LIST\nUsername:%s\nPassword:%s\n",connectStatus->username, connectStatus->password);
  connectForCommand(connectStatus);

  //AUTH CONNECTION!
  for(int i=0;i<4;i++){
    bzero(respBuf,MAXBUF);
    if(connectStatus->connfds[i] > 0){
      if(write(connectStatus->connfds[i],sendMsg,strlen(sendMsg)) > 0){
        read(connectStatus->connfds[i],respBuf,MAXBUF);
        if(strcmp(respBuf,"AUTHORIZED_OK")!=0){
          close(connectStatus->connfds[i]);
          connectStatus->connfds[i] = -1;
        }
      }
    }
  }

  _Bool authed = false;
  for(int i=0;i<4;i++){
    if(connectStatus->connfds[i] > 0){
      authed = true;
    }
  }
  if(!authed){
    printf("%s\n", "YOU'RE EITHER NOT AUTHORIZED TO CONNECT\n OR SERVERS ARE DOWN!");
    return;
  }

  int serverCounts[4] = {0,0,0,0};
  int max = 0;
  for(int i=0;i<4;i++){
    bzero(respBuf,MAXBUF);
    if(connectStatus->connfds[i] > 0){
      read(connectStatus->connfds[i],respBuf,MAXBUF);
      serverCounts[i] = atoi(strstr(respBuf,":")+1);
      if(serverCounts[i] > max){
          max = serverCounts[i];
      }
    }
  }
  char files[max][MAXBUF];
  for(int i=0;i<max;i++){
    bzero(files[i],MAXBUF);
  }
  char *file;
  char *partChar;
  char *parts2;
  int p1,p2;
  char *therest;
  int parts[max][4];
  for(int i=0;i<max;i++){
    parts[i][0]=0;
    parts[i][1]=0;
    parts[i][2]=0;
    parts[i][3]=0;
  }
  for(int i=0;i<4;i++){
    bzero(respBuf,MAXBUF);
    if(serverCounts[i] > 0 && connectStatus->connfds[i] > 0 ){
      read(connectStatus->connfds[i],respBuf,MAXBUF);
      file = strtok_r(respBuf,"\n",&therest);
      file = strstr(file,":")+1;
      partChar = strtok_r(NULL,"\n", &therest);
      partChar = strstr(partChar,":")+1;
      p1 = atoi(strtok_r(partChar,":",&parts2))-1;
      p2 = atoi(parts2)-1;
      for(int j=0;j<max;j++){
        if(strlen(files[j]) == 0){
          sprintf(files[j],"%s",file);
          parts[j][p1] = 1;
          parts[j][p2] = 1;
          break;
        }
        if(strcmp(files[j],file) == 0){
          parts[j][p1]=1;
          parts[j][p2]=1;
          break;
        }
      }
      while(file = strtok_r(NULL,"\n",&therest), file!=NULL){
        file = strstr(file,":")+1;
        partChar = strtok_r(NULL,"\n", &therest);
        partChar = strstr(partChar,":")+1;
        p1 = atoi(strtok_r(partChar,":",&parts2))-1;
        p2 = atoi(parts2)-1;
        for(int j=0;j<max;j++){
          if(strlen(files[j]) == 0){
            sprintf(files[j],"%s",file);
            parts[j][p1] = 1;
            parts[j][p2] = 1;
            break;
          }
          if(strcmp(files[j],file) == 0){
            parts[j][p1]=1;
            parts[j][p2]=1;
            break;
          }

        }
      }
    }
  }
  disconnect(connectStatus);

  printf("Files on DFS Server: \n");
  int completes[max];
  for(int i=0;i<max;i++){
    int total = 0;
    for(int j=0;j<4;j++){
      total+=parts[i][j];
    }
    completes[i]=total;
    if(completes[i] == 4){
        printf("%d)%s\n", i+1,files[i]);
    } else {
      printf("%d)%s [incomplete]\n", i+1,files[i]);
    }
  }




}


//-----------------------------------------------------------------------------------GATHER USERINPUT ----------------------------------------------------------------------------------

/*
* Gather in Comand Choice and route to appropriate method!
*/
void choiceRouter(Connection *connectStatus){
    char buf[MAXBUF];
    bzero(buf, MAXBUF);
    printf("\n\nPlease enter a Command: \n\tGET [file_name] \n\tPUT [file_name]\n\tlist \n\texit \n");
    printf("Command:");
    fgets(buf, MAXBUF, stdin);


    if(strstr(buf,"GET")){
      printf("\nGetting the Requested file!\n");
      command_get(connectStatus, buf);
    }
    else if(strstr(buf,"PUT")){
      printf("\nSending the file\n");
      command_put(connectStatus, buf);
      return;
    }
    else if(strstr(buf,"list")){
      printf("\nGetting the list of files on the Server\n");
      command_list(connectStatus);
    }
    else if(strstr(buf,"exit")){
      printf("\nHave a nice day!\n");
      exit(0);
    }
    else if(strstr(buf,"KILL")){
      char msg[] = "KILL";
      connectForCommand(connectStatus);
      sendToAll(connectStatus,msg);
      exit(0);

    }
    else{
      printf("Enter a Proper Command!\n");
    }
}

//-----------------------------------------------------------------------------------CONNECT AND AUTHENTICATE ----------------------------------------------------------------------------------
void connectToDFSServers(Configuration *conf, Connection *connectStatus){

  int connfd;
  connectStatus->password = (char *)malloc(strlen(conf->password)+1*sizeof(char));
  strcpy(connectStatus->password,conf->password);
  connectStatus->username = (char *)malloc(strlen(conf->username)+1*sizeof(char));
  strcpy(connectStatus->username,conf->username);

  //Connect for Connection 1
  //Build Connection 1
  connectStatus->addresses[0] = (char *)malloc(strlen(conf->addressOne)+1*sizeof(char));
  strcpy(connectStatus->addresses[0],conf->addressOne);
  connectStatus->ports[0] = conf->portOne;

  //Connect for Connection 2
  connectStatus->addresses[1] = (char *)malloc(strlen(conf->addressTwo)+1*sizeof(char));
  strcpy(connectStatus->addresses[1],conf->addressTwo);
  connectStatus->ports[1] = conf->portTwo;

  //Connect for Connection 3
  connectStatus->addresses[2] = (char *)malloc(strlen(conf->addressThree)+1*sizeof(char));
  strcpy(connectStatus->addresses[2],conf->addressThree);
  connectStatus->ports[2] = conf->portThree;

  //Connect for Connection 4
  connectStatus->addresses[3] = (char *)malloc(strlen(conf->addressFour)+1*sizeof(char));
  strcpy(connectStatus->addresses[3],conf->addressFour);
  connectStatus->ports[3] = conf->portFour;

}

void connectForCommand(Connection *connectStatus){
  for(int i=0;i<4;i++){
    connectStatus->connfds[i] = connect_fd(connectStatus->addresses[i],connectStatus->ports[i]);
  }
}
void disconnect(Connection *connectStatus){
  for(int i=0;i<4;i++){
    if(connectStatus->connfds[i] > 0){
      close(connectStatus->connfds[i]);
      connectStatus->connfds[i] = -1;
    }
  }
}

int connect_fd(char* address, int port){
  int connectfd, optval=1;
  struct sockaddr_in serveraddr;
  struct hostent *host_entity;

  //Get Host by name
  if((host_entity = gethostbyname(address)) == NULL){
    return -1;
  }

  //Assign Structure of serveraddr
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(port);
  serveraddr.sin_addr.s_addr = *(long*)host_entity->h_addr;

  // Create a socket descriptor
  if((connectfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
    return -1;

  /* Eliminates "Address already in use" error from bind. */
  if (setsockopt(connectfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0)
    return -1;

  //Check for Invalid Address or Not Support Address
  if(inet_pton(AF_INET, address, &serveraddr.sin_addr)<=0){
      char debugMsg[100];
      sprintf(debugMsg, "Invalid Address/ Not Supported Address: %s:%d\n", address,port);
      debugger(debugMsg);
      return -1;
    }


  if(connect(connectfd,(struct sockaddr *)&serveraddr,sizeof(serveraddr)) <0 ){
      char debugMsg[100];
      //sprintf(debugMsg,"Connection failed to host:%s with Port: %d\n", address, port);
      //debugger(debugMsg);
      return -1;
    }
  return connectfd;

}

//-----------------------------------------------------------------------------------BUILDERS ----------------------------------------------------------------------------------

void buildConfiguration(Configuration *conf, char* filename){
    //Load and Build Configuration
    FILE *f;
    char fileBuf[MAXBUF];
    char *usernameorpass;
    char *addressName;
    char *port;

    f = fopen(filename,"r");
    if(!f){
      errorHandler("Failed to Open Configuration File  buildConfiguration!");
      exit(0);
    }

    //DSF1 Setup
    fgets(fileBuf,MAXBUF,f);
    if(strstr(fileBuf,"Server DFS1") != NULL){
      fileBuf[strcspn(fileBuf,"\r")] = 0;
      //Get Address and Port Strings
      strtok_r(fileBuf,":",&addressName);
      strtok_r(addressName,":",&port);

      //Allocate and copy over addressOne
      conf->addressOne = (char *)malloc(strlen(addressName)+1*sizeof(char));
      strcpy(conf->addressOne,addressName);

      //Assign port for server 1
      conf->portOne = atoi(port);
    } else {
      errorHandler("In buildConfiguration - Configuration File missing DFS1!");
    }

    //DSF2 Setup
    fgets(fileBuf,MAXBUF,f);
    if(strstr(fileBuf,"Server DFS2") != NULL){
      fileBuf[strcspn(fileBuf,"\r")] = 0;
      //Get Address and Port Strings
      strtok_r(fileBuf,":",&addressName);
      strtok_r(addressName,":",&port);

      //Allocate and copy over addressTwo
      conf->addressTwo = (char *)malloc(strlen(addressName)+1*sizeof(char));
      strcpy(conf->addressTwo,addressName);

      //Assign port for server 2
      conf->portTwo = atoi(port);
    } else {
      errorHandler("In buildConfiguration - Configuration File missing DFS2!");
    }

    //DSF3 Setup
    fgets(fileBuf,MAXBUF,f);
    if(strstr(fileBuf,"Server DFS3") != NULL){
      fileBuf[strcspn(fileBuf,"\r")] = 0;
      //Get Address and Port Strings
      strtok_r(fileBuf,":",&addressName);
      strtok_r(addressName,":",&port);

      //Allocate and copy over addressThree
      conf->addressThree = (char *)malloc(strlen(addressName)+1*sizeof(char));
      strcpy(conf->addressThree,addressName);

      //Assign port for server 3
      conf->portThree = atoi(port);
    } else {
      errorHandler("In buildConfiguration - Configuration File missing DFS3!");
    }

    //DSF4 Setup
    fgets(fileBuf,MAXBUF,f);
    if(strstr(fileBuf,"Server DFS4") != NULL){
      fileBuf[strcspn(fileBuf,"\r")] = 0;
      //Get Address and Port Strings
      strtok_r(fileBuf,":",&addressName);
      strtok_r(addressName,":",&port);

      //Allocate and copy over addressFour
      conf->addressFour = (char *)malloc(strlen(addressName)+1*sizeof(char));
      strcpy(conf->addressFour,addressName);

      //Assign port for server 4
      conf->portFour = atoi(port);
    } else {
      errorHandler("In buildConfiguration - Configuration File missing DFS4!");
    }

    //Username Setup
    fgets(fileBuf,MAXBUF,f);
    if(strstr(fileBuf,"Username") != NULL){
      //Get username string
      fileBuf[strcspn(fileBuf,"\r")] = 0;
      strtok_r(fileBuf,":",&usernameorpass);

      //Allocate and copy over usernameorpass
      conf->username = (char *)malloc(strlen(usernameorpass)+1*sizeof(char));
      strcpy(conf->username,usernameorpass);

    } else {
      errorHandler("In buildConfiguration - Configuration File missing Username!");
    }

    //password Setup
    fgets(fileBuf,MAXBUF,f);
    if(strstr(fileBuf,"Password") != NULL){
      //Get password string
      fileBuf[strcspn(fileBuf,"\r")] = 0;
      strtok_r(fileBuf,":",&usernameorpass);
      usernameorpass = strtok(usernameorpass,"\n");

      //Allocate and copy over usernameorpass
      conf->password = (char *)malloc(strlen(usernameorpass)+1*sizeof(char));
      strcpy(conf->password,usernameorpass);

    } else {
      errorHandler("In buildConfiguration - Configuration File missing Password!");
    }

    //Close File!
    fclose(f);
    return;
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

  //Populate Server File Locations!
  //X Value == 0
  serverPart[0][0][0] = 0;
  serverPart[0][0][1] = 3;
  serverPart[0][1][0] = 0;
  serverPart[0][1][1] = 1;
  serverPart[0][2][0] = 1;
  serverPart[0][2][1] = 2;
  serverPart[0][3][0] = 2;
  serverPart[0][3][1] = 3;
  //X Value == 1
  serverPart[1][0][0] = 1;
  serverPart[1][0][1] = 0;
  serverPart[1][1][0] = 2;
  serverPart[1][1][1] = 1;
  serverPart[1][2][0] = 3;
  serverPart[1][2][1] = 2;
  serverPart[1][3][0] = 0;
  serverPart[1][3][1] = 3;
  //X Value == 2
  serverPart[2][0][0] = 2;
  serverPart[2][0][1] = 1;
  serverPart[2][1][0] = 3;
  serverPart[2][1][1] = 2;
  serverPart[2][2][0] = 0;
  serverPart[2][2][1] = 3;
  serverPart[2][3][0] = 1;
  serverPart[2][3][1] = 0;
  //X Value == 3
  serverPart[3][0][0] = 3;
  serverPart[3][0][1] = 2;
  serverPart[3][1][0] = 0;
  serverPart[3][1][1] = 3;
  serverPart[3][2][0] = 1;
  serverPart[3][2][1] = 0;
  serverPart[3][3][0] = 2;
  serverPart[3][3][1] = 1;
}
//-----------------------------------------------------------------------------------Memory Freers ----------------------------------------------------------------------------------

void freeConf(Configuration *conf){
  //Free Addresses
  free(conf->addressOne);
  free(conf->addressTwo);
  free(conf->addressThree);
  free(conf->addressFour);

  //Free username and password
  free(conf->username);
  free(conf->password);

  return;
}

void freeConnection(Connection *connectStatus){
  free(connectStatus->username);
  free(connectStatus->password);
  free(connectStatus->one_ptr);
  free(connectStatus->two_ptr);
  free(connectStatus->three_ptr);
  free(connectStatus->four_ptr);
}
