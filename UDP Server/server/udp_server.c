/*
 * udpserver.c - A simple UDP echo server
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}


/*
  This function handles the get command
  Return false if we fail for some reason and want to echo that.
*/
_Bool command_get(int sockfd, char buf[], size_t len, int flags, struct sockaddr_in clientaddr, int clientlen){
  //Some variables
  int n;
  //Get the Filename from our function.
  char *filename;
  strtok(buf, " ");
  filename = strtok(NULL, "\n");

  //Open the file and make sure it's valid.
  FILE* file;
  file = fopen(filename, "r");
  bzero(buf, BUFSIZE);
  if(file == NULL){ //Handle a case where files are null.
    char *s = "File not found";
    snprintf(buf,BUFSIZE, "%s", s);
    return false;
  }



  //This is our actual loop that sends  the code repeatedly.
  int size = 0;
  int totalpackets = 0;
  bzero(buf, BUFSIZE);
  while(!feof(file)){
    size = fread(buf,1,BUFSIZE-1,file);
    //We need packet length to be sent, we use this to tell if we are still streaming content or not.
    n = sendto(sockfd, &size, sizeof(int), 0, (struct sockaddr *) &clientaddr, clientlen); //Send the packet size
    //Send the actual data, BUFSIZE is the size  to ensure all data is sent.
    n = sendto(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &clientaddr, clientlen);

    //We want to check the amount of packets sent versus recieved, this is for debugging.
    totalpackets += size;
  }
  fclose(file);
  //Indicate we're done
  size = -1;
  n = sendto(sockfd, &size, sizeof(int), 0, (struct sockaddr *) &clientaddr, clientlen);
  printf("Sent: %d\n", totalpackets);
  return true;
}

/*
  This function handles the put command
  Return false if we fail for some reason and want to echo that.
*/
_Bool command_put(int sockfd, char buf[], size_t len, int flags, struct sockaddr_in clientaddr, int clientlen){
  //Open File
  int n;
  char *com, *filename;
  com = strtok(buf, " ");
  filename = strtok(NULL, "\n");
  FILE *file;
  char *name = filename;
  file = fopen(name, "w");

  if (n < 0)
    error("ERROR in recvfrom");


  //Keep Listening until our packet size is less than 0.
  //This code is same code as the code from udp_client.c for  the command_put funtion.
  int size, totalpackets;
  while(true){

    //Recieve the packet size.
    n = recvfrom(sockfd, &size, sizeof(int), 0, &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");
    else if(size == -1){
        break;
      }


    //Reset our buffer to 0 and read in the next wave of data.
    bzero(buf,BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, 0, &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");

    //For debugging purposes, track the number of packets.
    if(n >=  0)
      totalpackets +=  size;

    //Write  to a file.
    fwrite(buf, 1, size, file);
    if(size < BUFSIZE-1){
      n = recvfrom(sockfd, &size, sizeof(int), 0, &clientaddr, &clientlen);
      break;
    }
  }
  fclose(file);
  printf("File recieved!\n" );
  return true;

}

/*
  This function handles the delete command, we use the remove command.
*/
void command_delete(char buf[]){
  //find the first space and find everything after it
  char *com, *filename;
  com = strtok(buf, " ");
  filename = strtok(NULL, "\n");


  //Try to actually remove the file.
  char *name = filename;
  if(remove(name) == 0){
    char *s = "Deleted successfully.";
    snprintf(buf,BUFSIZE, "%s\n", s);
    return;
  }
  else {
    char *s = "Could not find file specified.";
    snprintf(buf,BUFSIZE, "%s\n", s);
    return;
  }
}


// A function handle the ls command
// Followed the following for directory opening
// https://www.geeksforgeeks.org/c-program-list-files-sub-directories-directory/
void command_ls(char buf[]){

  //Clear the buffer for new information:
  bzero(buf,BUFSIZE);


  struct dirent *de; //This is a directory pointer
  DIR *dr = opendir("."); //Returns a pointer of DIR type

  if(dr == NULL) //Null if we can't open directory
  {
    //If there's an error opening the directory, echo back to the client
    //that there is an issue.
    char *s = "Can't access the directory of files at this time!";
    snprintf(buf,BUFSIZE, "%s\n", s);
    return;
  }

  size_t len = strlen(buf);
  de = readdir(dr);
  while(de != NULL){
    snprintf(buf+len, BUFSIZE-len, "%s\n", de->d_name);
    len = strlen(buf);
    de = readdir(dr);
  }
  closedir(dr);
  return;
  //"Server Ending..."
}


/*
  This function handles the exit command and will ensure we echo we are ending, then end.
*/
void command_exit(char buf[]){
  //Clear the buffer
  bzero(buf,BUFSIZE);
  char *s = "Server Ending...";
  snprintf(buf,BUFSIZE, "%s\n", s);
  return;
}


/*
  This Function handles all routing for the different commands
  We return an boolean value, if its false we are not sending a file back
  If we're sending a file we return 1, if we are getting a file we return 2, else we return 3
*/
int checkCommand(char buf[]){
  if(strstr(buf,"get")){
    return 1;
  }
  else if(strstr(buf,"put")){
    printf("Getting a file....\n");
    return 2;
  }
  else if(strstr(buf,"delete")){
    command_delete(buf);
    return 3;
  }
  else if(strstr(buf,"ls")){
    command_ls(buf);
    return 3;
  }
  else if(strstr(buf,"exit")){
    command_exit(buf);
    return 3;
  }
  else{
    char* s = " - is not a valid command";
    printf("%s\n", buf);
    snprintf(buf+strlen(buf)-1,BUFSIZE-strlen(buf), "%s\n", s);
    return 3;
  }
}

int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  /*
   * check command line arguments
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /*
   * socket: create the parent socket
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets
   * us rerun the server immediately after we kill it;
   * otherwise we have to wait about 20 secs.
   * Eliminates "ERROR on binding: Address already in use" error.
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /*
   * bind: associate the parent socket with a port
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr,
	   sizeof(serveraddr)) < 0)
    error("ERROR on binding");

  /*
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(buf, BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, 0,
		 (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");



    /*
     * gethostbyaddr: determine who sent the datagram
     */
    hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
			  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL)
      error("ERROR on gethostbyaddr");
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");
    printf("server received datagram from %s (%s)\n",
	   hostp->h_name, hostaddrp);
    printf("server received %d/%d bytes: %s\n", strlen(buf), n, buf);

    /*
     * sendto: echo the input back to the client
     */
    int command = checkCommand(buf);
    if(command == 1){
      _Bool success = false;
      success = command_get(sockfd,buf,strlen(buf),0,clientaddr, clientlen);
      //If  the command_get fails, we  return information about the failure.
      if(success == false){
        int fail = -1;
        n = sendto(sockfd, &fail, sizeof(int), 0,
             (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0)
          error("ERROR in sendto");
      }
    }
    else if(command == 2){
      command_put(sockfd,buf,strlen(buf),0,clientaddr, clientlen);
    }
    else{
      n = sendto(sockfd, buf, strlen(buf), 0,
           (struct sockaddr *) &clientaddr, clientlen);
      if (n < 0)
        error("ERROR in sendto");
    }


    //CHECK IF WE END THE SERVER
    if(strstr(buf,"Server Ending...")){
      exit(0);
    }
  }
}
