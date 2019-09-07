/*
 * socket-client.c
 * Simple TCP/IP communication using sockets
 *
 * Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
 * Vavouliotis Giorgos <nuovocominzio@hotmail.com>
 * AM : 03112083 
 */

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "socket-common.h"

int main(int argc, char *argv[]){
    
   char server_msg[MSG_SIZE + 1],keyboard[MSG_SIZE + 10],smsg[MSG_SIZE + 1];
   char * hostname = argv[1];
   int  port,sd,readvar,flag=0;
   struct hostent *hp;
   struct sockaddr_in sa;
   fd_set set1, temp_set;

   /* check if i take the right args */
   if (argc != 3) {
	fprintf(stderr, "Usage: %s hostname port\n", argv[0]);
	exit(1);
   }
	
   /*
      create a socket. socket() returns as a file descriptor named sd 
      the correct thing to do is to use AF_INET in your struct sockaddr_in and PF_INET in your call to socket(). 
      But practically speaking, you can use AF_INET everywhere
   */
   if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0){
	perror("socket");
	exit(-1);
   }
    
   /* get host info ---> hostname is localhost */
   if ( !(hp = gethostbyname(hostname))) {
	fprintf(stderr, "DNS failed lookup for host: %s\n", hostname);
	exit(-1);
   }

   /* all the clients that i will create the will have the name localhost */
   printf("I am the client with name %s and now i am starting.\n", hp->h_name);
    
   /* atoi converts a string into a integer. If no valid conversion could be performed, it returns zero*/
   port = atoi(argv[2]);
   if ( port <= 0 ){
	printf("Problem with atoi.\n");
	exit(1);
   }

   sa.sin_family = AF_INET;
   /* The htons() function converts the unsigned short integer port from host byte order to network byte order.*/
   sa.sin_port = htons(port);
   /* copy to the sa.sin_addr.s_addr the hp->h_addr */
   memcpy(&sa.sin_addr.s_addr, hp->h_addr, sizeof(struct in_addr));
  
   /* Initializes the file descriptor set with name set1 to have zero bits for all file descriptors */
   FD_ZERO(&set1);

   /* connect() sundeei ena socket me fd = sd me thn dieu8unsh pou uparxei san 2o orisma 
   --> an exw tcp/ip tote egka8ista mia nea ekserxomenh sundesh apo ton client */
   printf("I want to connect with the server and i will make a request\n");
   if (connect(sd, (struct sockaddr *) &sa, sizeof(sa)) < 0){
	perror("connect");
	exit(-1);
   }
  
   /* Sets the bit for the file descriptors sd and 0 in the file descriptor set with name set1 */
   FD_SET(0,&set1); 
   FD_SET(sd,&set1);

   /* Now i am ready to read and write to fd 0 and sd */  
   for (;;){
        /* this loop ends when the client gives koukis\n" */

	/*
        select() and pselect() allow a program to monitor multiple file descriptors, waiting until one 
	or more of the file descriptors become "ready" for some class of I/O operation 
        */

	temp_set = set1; // fd_set variable
	/* dialegw apo thn domh fd_set kapoio to opoio einai etoimo */
	select(FD_SETSIZE, &temp_set, NULL, NULL, NULL);
	
	/* FD_ISSET() returns a non-zero value if the bit for the file descriptor fd is set in the file descriptor 
	set pointed to by fdset, and 0 otherwise */
	if (FD_ISSET(0, &temp_set)){
	    /* get the message from the stdin and put it to the variable keyboard */
	    fgets(keyboard, MSG_SIZE, stdin);
	    /* write the message to the socket */
	    write(sd, keyboard, strlen(keyboard));
	    if (strcmp(keyboard, "koukis\n") == 0){
		    printf("Koukis killed the client(OMG)\n");
		    close(sd); // close the socket
		    exit(1);
	    }
	}
	if (FD_ISSET(sd, &temp_set)){
	    /* the server send a message and i have to read it */
	    if ((readvar = read(sd, server_msg, MSG_SIZE)) < 0){
		perror("read");
		exit(-1);
	    }
	    /* if the read success then i have to put \0 to the message */
	    server_msg[readvar] = '\0';
	    /* print the message that server sent to the client */
	    printf("Server sent: %s", server_msg);
	    if (strcmp(server_msg, "halt\n") == 0){
		    printf("Server killed the client because sent 'halt'\n");
		    close(sd); // close the socket
		    exit(1);
	    }
	}
    }
}

