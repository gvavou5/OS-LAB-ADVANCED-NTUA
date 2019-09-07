/*
 * socket-server.c
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

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <crypto/cryptodev.h>

#include "socket-common.h"

#define all  100
#define minus  -100


/* Insist until read all the data */
ssize_t insist_read(int fd, void *buf, size_t cnt)
{
        ssize_t ret;
        size_t orig_cnt = cnt;

        while (cnt > 0) {
                ret = read(fd, buf, cnt);
                if (ret == 0) {
                	printf("Server went away. Exiting...\n");
                	return 0;
                }
                if (ret < 0) {
                	perror("read from server failed");
                        return ret;
                }
                buf += ret;
                cnt -= ret;
        }

        return orig_cnt;
}

/* Insist until all of the data has been written */
ssize_t insist_write(int fd, const void *buf, size_t cnt)
{
	ssize_t ret;
	size_t orig_cnt = cnt;
	
	while (cnt > 0) {
	        ret = write(fd, buf, cnt);
	        if (ret < 0)
	                return ret;
	        buf += ret;
	        cnt -= ret;
	}

	return orig_cnt;
}


int main(int argc,char *argv[]){

   char message[256],msg1[256],key[KEY_SIZE+1],iv[BLOCK_SIZE+1],temp[256],keyboard[256],buff[256];
   int sd, fd, sd2, index, ClientArr[MAX_CLIENTS][2], clnum, i, myselect,fdnew,flag;
   struct sockaddr_in sa;
   struct session_op sess;
   struct crypt_op cryp;
   fd_set set1, temp_set;
   
   memset(&sess, 0, sizeof(sess));
   memset(&cryp, 0, sizeof(cryp));
   
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
    
    /* Listen for incoming connections */
    if (listen(sd, TCP_BACKLOG) < 0){
	perror("listen");
	exit(EXIT_FAILURE);
    }
    
    fdnew = open("/dev/crypto", O_RDWR);
    if (fdnew < 0) {
	perror("open(/dev/cryptodev0)");
	return 1;
    }
    // if you want info about this open the cryptodev.h
    strcpy(iv,IV);
    strcpy(key,KEY);
    sess.cipher = CRYPTO_AES_CBC;
    sess.keylen = KEY_SIZE;
    sess.key = key;
    if (ioctl(fdnew, CIOCGSESSION, &sess)) {
	perror("ioctl(CIOCGSESSION)");
	return 1;
    }
    cryp.ses = sess.ses;
    cryp.iv = iv;

    /* Initializes the file descriptor set fdset to have zero bits for all file descriptors */
    FD_ZERO(&set1);
    /* Sets the bit for the file descriptor sd and 0 in the file descriptor set set1 */
    FD_SET(sd, &set1);
    FD_SET(0, &set1);	
    clnum = 0;

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
	/* dialegw apo thn domh fd_set kapoio to opoio einai etoimo */
	select(FD_SETSIZE, &temp_set, NULL, NULL, NULL);
	for(fd=0; fd<FD_SETSIZE; fd++){
	    if (FD_ISSET(fd, &temp_set)){
		if (fd == 0){
		    /* server writes in stdin */
		    scanf("%d",&myselect); // shows what the server have to do
		    if (myselect != minus){
			/* if i dont take -100 i should take the server's message */	
			fgets(keyboard, MSG_SIZE, stdin);
	   		/* use AES for encryption - i have my encrypted message in temp */
            		cryp.src = keyboard;
	    		cryp.dst = temp;
	   		cryp.len = sizeof(keyboard);
	    		cryp.op  = COP_ENCRYPT;
            		if (ioctl(fdnew, CIOCCRYPT, &cryp)) {
				perror("ioctl(CIOCCRYPT)");
				return 1;
            		}
		    }
		    if (myselect == minus){
			    /* if server writes -100 the server is going down and i send halt to clients manually */
			    for (i=0; i<clnum; i++){
				// when server is going down i quit all the clnum because i want it.
				cryp.src = "halt\n";
	    			cryp.dst = temp;
	   			cryp.len = sizeof(keyboard);
	    			cryp.op  = COP_ENCRYPT;
            			if (ioctl(fdnew, CIOCCRYPT, &cryp)) {
					perror("ioctl(CIOCCRYPT)");
					return 1;
            			}
				insist_write(ClientArr[i][0], temp, sizeof(temp));
				close(ClientArr[i]);
			    }
			    printf("Server is going down and all clients are dead also\n");
			    close(sd);
			    exit(0);
		    }	
		    else if (myselect == all){
			    /* if server writes 100, have to write the message in all the clients */
			    for (i = 0; i<clnum; i++)
				insist_write(ClientArr[i][0], temp, sizeof(temp));
			    break;
		    }
		    else{
			    if (myselect == 55555) {
					printf("I don't have client with this fd.\n");
					flag = 1 ;
			    }
			    else{
			    	printf("Server wants to send the message to client with fd = %d.\n", myselect);	
			    	for (i=0; i<clnum; i++){
					// search  the client that server want to send the msg
					if (ClientArr[i][0] == myselect){
				    		 // i find the right client
				   		 insist_write(ClientArr[i][0], temp, sizeof(temp));
				    		 break;
					}
			    	}
			    }
			    if (i == clnum && flag == 0){
				// i did not find the client that server wants.
				printf("I don't have client with this fd.\n");
			    	break;
			    }
		    }
		    
		}
		else if (fd == sd){
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
			ClientArr[clnum][1] = clnum+1;
			clnum++;
			fprintf(stderr, "Client accepted. This is client %d and has fd = %d.\n", clnum, ClientArr[clnum-1][0]);
			strcpy(buff,"Request Accepted. You are client with fd = ");			
			snprintf(msg1,256,"%d.\n",ClientArr[clnum-1][0]);				
			strcat(buff, msg1);
			cryp.src = buff;
			cryp.dst = temp;
			cryp.len = sizeof(buff);
			cryp.op  = COP_ENCRYPT;
			if (ioctl(fdnew, CIOCCRYPT, &cryp)) {
				perror("ioctl(CIOCCRYPT)");
				return 1;
			}			
			insist_write(sd2, temp, sizeof(temp));				
		    }
		    else {
			/* if server is full of clients inform them */
			cryp.src = "Full of clients. Try later. I am sorry! \n";
	    		cryp.dst = temp;
	   		cryp.len = sizeof(keyboard);
	    		cryp.op  = COP_ENCRYPT;
            		if (ioctl(fdnew, CIOCCRYPT, &cryp)) {
				perror("ioctl(CIOCCRYPT)");
				return 1;
            		}
			insist_write(sd2, temp, sizeof(temp));		
			close(sd2);
		    }
		}
		else if (fd > 0){
		    /* a client sent something and first i have to read it */
		    if (insist_read(fd, message, MSG_SIZE) != MSG_SIZE){
			perror("read");
			exit(EXIT_FAILURE);
		    }
		    cryp.src = message;
		    cryp.dst = temp;
		    cryp.len = sizeof(message);
		    cryp.op = COP_DECRYPT;
		    if (ioctl(fdnew, CIOCCRYPT, &cryp)) {
			perror("ioctl(CIOCCRYPT)");
			return 1;
         	    }
		    /* print the message that a client sent */
		    printf("Client with fd = %d sent: %s", fd, temp);
		    /* if a client wants to see all the list of clients 
		       that server manages sends the following msg */
                    if (strcmp(temp, "show list of clients\n") == 0){
				for (i = 0; i<clnum; i++){		
					strcpy(buff,"\nClient with fd = ");			
					snprintf(msg1,256,"%d.\n",ClientArr[i][0]);				
					strcat(buff, msg1);					
					cryp.src = buff;
			    		cryp.dst = temp;
			   		cryp.len = sizeof(buff);
			    		cryp.op  = COP_ENCRYPT;
			    		if (ioctl(fdnew, CIOCCRYPT, &cryp)) {
						perror("ioctl(CIOCCRYPT)");
						return 1;
			    		}			
					insist_write(fd, temp, sizeof(temp));
				}
		    }
		    /* if a client sent koukis means that is dead and i have to close the socket */
		    if (strcmp(temp, "koukis\n") == 0){
				printf("Client with fd = %d is out and i will close the socket!\n",fd);								
				close(fd);
				// clear the client that send "koukis"
	    			FD_CLR(fd, &set1); 
				/* drop this client from the array */
	    			for (index = 0; index < clnum - 1; index++)
					if (ClientArr[index] == fd) break;          
	    			for (; index < clnum - 1; index++) (ClientArr[index][0]) = (ClientArr[index + 1][0]);
	    			clnum--;
		    }
		}
		else printf("fd<0????\n");
	    }  
	}
    }
    if (ioctl(fdnew, CIOCFSESSION, &sess.ses)) {
		perror("ioctl(CIOCFSESSION)");
		return 1;
    }
    if (close(fdnew) < 0) {
		perror("close(fd)");
		return 1;
    }
    if (close(sd) < 0) {
	        perror("close(fd)");
		return 1;
    }
    return 0;
}
