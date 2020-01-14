/*
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

void get_filename(char buf[], char filename[]){
  strtok(buf, " ");
  filename = strtok(NULL, "\n");
}

/*
* This function handles the get command, and all the actions needed for it.
  Return false on failure.
  True on success.
*/
_Bool command_get(int sockfd, char buf[], size_t len, int flags, struct sockaddr_in serveraddr, int serverlen){
  int n;
  _Bool fileOpen;

  //Send Command to Server
  n = sendto(sockfd, buf, len, 0, &serveraddr, serverlen);
  if (n < 0)
    error("ERROR in sendto");


  //Open File
  char *com, *filename;
  com = strtok(buf, " ");
  filename = strtok(NULL, "\n");
  FILE *file;
  char *name = filename;
  file = fopen(name, "w");

  if (n < 0)
    error("ERROR in recvfrom");


  //Keep Listening until our packet size is less than 0.
  int size = 0;
  int totalpackets = 0;
  while(true){
    //Recieve the packet size.
    n = recvfrom(sockfd, &size, sizeof(int), 0, &serveraddr, &serverlen);
    if (n < 0)
      error("ERROR in recvfrom");
    else if(size == -1){
        break;
      }

    //Reset our buffer to 0 and read in the next wave of data.
    bzero(buf,BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, 0, &serveraddr, &serverlen);
    if (n < 0)
      error("ERROR in recvfrom");

    //For debugging purposes, track the number of packets.
    if(n >=  0)
      totalpackets +=  size;

    //Write  to a file.
    fwrite(buf, 1, size, file);
    if(size < BUFSIZE-1){
      n = recvfrom(sockfd, &size, sizeof(int), 0, &serveraddr, &serverlen);
      break;
    }
  }
  // printf("Recieved: %d\n", totalpackets );
  fclose(file);
  return true;


}

/*
* This function handles the get command, and all the actions needed for it.
*/
void command_put(int sockfd, char buf[], size_t len, int flags, struct sockaddr_in serveraddr, int serverlen){
  //Some variables
  int n;

  //Send Command to Server
  n = sendto(sockfd, buf, len, 0, &serveraddr, serverlen);
  if (n < 0)
    error("ERROR in sendto");


  //Get the Filename from our function.
  char *filename;
  strtok(buf, " ");
  filename = strtok(NULL, "\n");

  //Open the file and make sure it's valid.
  FILE* file;
  file = fopen(filename, "rb");
  bzero(buf, BUFSIZE);
  if(file == NULL){
    printf("FILE NOT FOUND!\n");
    return;
  }



  int size = 0;
  int totalpackets = 0;
  bzero(buf, BUFSIZE);
  while(!feof(file)){
    size = fread(buf,1,BUFSIZE-1,file);
    //We need packet length to be sent, we use this to tell if we are still streaming content or not.
    n = sendto(sockfd, &size, sizeof(int), 0, (struct sockaddr *) &serveraddr, serverlen);
    //Send the actual data, BUFSIZE is the size  to ensure all data is sent.
    n = sendto(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, serverlen);
    //We want to check the amount of packets sent versus recieved, this is for debugging.
    totalpackets += size;
  }
  fclose(file);
  //Idnicate we're done
  size = -1;
  n = sendto(sockfd, &size, sizeof(int), 0, (struct sockaddr *)&serveraddr, serverlen);

}




/*
* This function handles all routing and asking the user for input.
  We return an integer for 3 different cases.
  1. The user is sending a file.
  2. The user is getting a file.
  3. All other cases.
*/
int getUserInput(char buf[]){
    bzero(buf, BUFSIZE);
    printf("\n\nPlease enter a Command: \n\tget [file_name] \n\tput [file_name]\n\tdelete [file_name] \n\tls \n\texit \n");
    printf("Command:");
    fgets(buf, BUFSIZE, stdin);
    size_t len = strlen(buf);
    // if(buf[--len] =="\n")
    //   buf[--len] = "\0";
    if(strstr(buf,"get")){
      printf("Getting the Requested file!\n");
      return 1;
    }
    else if(strstr(buf,"put")){
      printf("Sending the file\n");
      return 2;
    }
    else if(strstr(buf,"delete")){
      printf("Deleting the file on the Server!\n");
      return 3;
    }
    else if(strstr(buf,"ls")){
      printf("Getting the list of files on the Server\n");
      return 3;
    }
    else if(strstr(buf,"exit")){
      printf("\nTelling the Server to Exit\n");
      return 3;
    }
    else{
      printf("Sending the Command to the Server\n");
      return 3;
    }
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    //printf("Command: %s\n",buf );

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);

    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);
    serverlen = sizeof(serveraddr);

    /* get a message from the user */
      int choice = 0;
      while(true){
      int choice = getUserInput(buf);

      /* send the message to the server */
      //Get a file from the server!
      if(choice == 1){
        if(command_get(sockfd, buf, strlen(buf), 0, serveraddr, serverlen) == false){
          printf("Echo from server: \n%s", buf);
        }
      }
      //Send a file to the server!
      else if(choice == 2){
        command_put(sockfd, buf, strlen(buf), 0, serveraddr, serverlen);
      }
      else{
        n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
        if (n < 0)
          error("ERROR in sendto");

        /* print the server's reply */
        n = recvfrom(sockfd, buf, BUFSIZE, 0, &serveraddr, &serverlen);
        if (n < 0)
          error("ERROR in recvfrom");
        printf("Echo from server: \n%s", buf);
      }
    }
    return 0;
}
