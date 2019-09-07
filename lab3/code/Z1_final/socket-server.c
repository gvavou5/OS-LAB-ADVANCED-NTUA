/*
 * socket-client.c
 * Simple TCP/IP communication using sockets
 *
 * Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
 * Vavouliotis Giorgos <nuovocominzio@hotmail.com>
 * AM : 03112083 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "socket-common.h"

#define all  100
#define minus  -100

int main(int argc,char *argv[]){

   int sd, j, sd2, index,flag=0;
   int ClientArr[MAX_CLIENTS][2], clnum, i, readvar, myselect;
   struct sockaddr_in sa;
   fd_set set1, temp_set;
   char message[MSG_SIZE + 1],msg1[MSG_SIZE + 1],keyboard[MSG_SIZE + 10];

   /*
      create a socket. socket() returns as a file descriptor named sd 
      the correct thing to do is to use AF_INET in your struct sockaddr_in and PF_INET in your call to socket(). 
      But practically speaking, you can use AF_INET everywhere.
   */
   if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0){
	perror("socket");
	exit(EXIT_FAILURE);
   }
    
   memset(&sa, 0, sizeof(sa));
   sa.sin_family = AF_INET;

   /* The htons() function converts the unsigned short integer port from host byte order to network byte order.*/
   sa.sin_port = htons(TCP_PORT);
   sa.sin_addr.s_addr = htonl(INADDR_ANY);
   /* Bind to a well-known port */ 
   if (bind(sd, (struct sockaddr *)&sa, sizeof(sa)) < 0){
	perror("bind");
	exit(EXIT_FAILURE);
    }
    
    /* Initialize the file descriptor set with name set1 to have zero bits for all file descriptors */
    FD_ZERO(&set1);
    
    /* Listen for incoming connections */
    if (listen(sd, TCP_BACKLOG) < 0){
	perror("listen");
	exit(EXIT_FAILURE);
    }
    
    /* Set the bit for the file descriptors sd and 0 in the file descriptor set set1 */
    FD_SET(sd, &set1);
    FD_SET(0, &set1);	
    clnum = 0; // number of clients 
    
    for(;;){
	 /* this loop ends when the server gives -100\n" */

	/* arxikopoiw to myselect se mia megalh sxetika timh h opoia den antistoixei se
	   se kapoio j etsi wste na parw swsto apotelesma otan grapsw se ena client kai
	   meta paw na grapsw kati sto server.
        */
	myselect = 55555; 
	flag = 0;
        /*
        select() and pselect() allow a program to monitor multiple file descriptors, waiting until one 
	or more of the file descriptors become "ready" for some class of I/O operation.
        */
	temp_set = set1;
	select(FD_SETSIZE, &temp_set, NULL, NULL, NULL);
	
	for(j=0; j<FD_SETSIZE; j++){
	    if (FD_ISSET(j, &temp_set)){
		if (j == 0){
		    /* server writes in stdin */
		    scanf("%d",&myselect); // shows what the server have to do
		    if (myselect != minus){
			/* if i dont take -100 i should take the server's message */	
			fgets(keyboard, MSG_SIZE, stdin);
			sprintf(message, "%s", keyboard+1);
		    }
		    if (myselect == minus){
			    /* if server writes -100 the server is going down and i send halt to clients manually */
			    for (i=0; i<clnum; i++){
				// when server is going down i quit all the clients.
				write(ClientArr[i][0], "halt\n", strlen("halt\n"));
				close(ClientArr[i][0]);
			    }
			    printf("Server is going down and all clients are dead also\n");
			    close(sd); // close the socket
			    exit(0);
		    }	
		    else if (myselect == all){
			    /* if server writes 100, have to write the message in all the clients */
			    printf("Server wants to send the message to all clients. Check all the clients\n");		
			    for (i = 0; i<clnum; i++)  write(ClientArr[i][0], message, strlen(message));
			    break;
		    }
		    else{
			    if (myselect == 55555) {
					printf("I don't have client with this fd\n");
					flag = 1 ;
			    }
			    else{
			    	printf("Server wants to send the message to client with fd = %d\n", myselect);	
			    	for (i=0; i<clnum; i++){
					// search  the client that server want to send the msg
					if (ClientArr[i][0] == myselect){
				    	// i find the right client
				    	write(myselect, message, strlen(message));
				    	break;
					}
			    	}
			    }
			    if (i == clnum && flag == 0){
				// i did not find the client that server wants.
				printf("I don't have client with this fd\n");
			    	break;
			    }
		    }
		    
		}
		else if (j == sd){
		    /* a client wants something */
 		    /* accept returns a new file descriptor in case of success
		       and the old sd can be used again. */
		    if ((sd2 = accept(sd, NULL, NULL)) < 0){
			perror("accept");
			exit(EXIT_FAILURE);
		    }
			
		    /* if my client list is not full enter here. */
		    if (clnum < MAX_CLIENTS){
			/* Set the bit for the file descriptor sd2 */
			FD_SET(sd2, &set1);
			/* put the sd2 of the new client into the ClientArr */
			ClientArr[clnum][0] = sd2;
			ClientArr[clnum][1] = clnum;
			clnum++; //increase the clients by 1
			fprintf(stderr, "Client accepted. This is client %d and has fd = %d\n", clnum, ClientArr[clnum-1][0]);
			sprintf(message, "Request Accepted. You are client with number %d and fd = %d\n", clnum, ClientArr[clnum-1][0]);
			write(sd2, message, strlen(message));		
		    }
		    else {
			/* if server is full of clients inform them */
			sprintf(message, "Full of clients. Try later. I am sorry! \n");
			write(sd2, message, strlen(message));		
			close(sd2);
		    }
		}
		else if (j > 0){
		    /* a client sent something and first i have to read it */
		    if ((readvar = read(j, message, MSG_SIZE)) < 0){
			perror("read");
			exit(EXIT_FAILURE);
		    }
		    if (readvar > 0){
			message[readvar] = '\0';
			/* print the message that a client sent */
			printf("Client with fd = %d sent: %s", j, message);
			/* if a client wants to see all the list of clients 
		           that server manages sends the following msg */
			if (strcmp(message, "show list of clients\n") == 0){
				printf("Server will send the list of all active clients to the client with fd=%d\n", j);
				sprintf(msg1, "List of clients:\n");				
				write(j,msg1, strlen(msg1));			
				for (i = 0; i<clnum; i++){
					 sprintf(msg1, "\n%d)Client with fd = %d\n", i+1 , ClientArr[i][0]);			
					 write(j, msg1, strlen(msg1));
				}
		       }
		       /* if a client sent koukis means that is dead and i have to close the socket */
		       else if (strcmp(message, "koukis\n") == 0){
				printf("Client with fd = %d is dead and i close the socket!\n",j);								
				close(j);
				// clear the client that send "koukis"
	    			FD_CLR(j, &set1); 
				/* drop this client from the array */
	    			for (index = 0; index < clnum - 1; index++)
					if (ClientArr[index][0] == j) break;          
	    			for (; index < clnum - 1; index++) {
					ClientArr[index][0] = ClientArr[index + 1][0];
					ClientArr[index][1] = ClientArr[index + 1][1];
				}	
	    			clnum--;
		       }
		       else ;
		   }		
		}
		else ;
	    }
	    
	}
    }
}
