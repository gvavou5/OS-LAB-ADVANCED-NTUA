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

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <crypto/cryptodev.h>

#include "socket-common.h"

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


int main(int argc, char *argv[]){
    char server_msg[256],keyboard[256],temp[256],key[KEY_SIZE+1],iv[BLOCK_SIZE+1];
    char * hostname = argv[1];
    int  port,sd,fdnew;
    struct hostent *hp;
    struct sockaddr_in sa;
    struct session_op sess;
    struct crypt_op cryp;
    fd_set set1, temp_set;

    /* Sets the first num bytes of the block of memory pointed by ptr to the specified value (interpreted as an unsigned char). */
    memset(&sess, 0, sizeof(sess));
    memset(&cryp, 0, sizeof(cryp));

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
	printf("I have a problem with atoi. I am gonna quit!\n");
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
    if (connect(sd, (struct sockaddr *) &sa, sizeof(sa)) < 0){
	perror("connect");
	exit(-1);
    }
    
    fdnew = open("/dev/crypto", O_RDWR);
    if (fdnew < 0) {
	perror("open(/dev/crypto)");
	return 1;	
    }

    // if you want info about this open the cryptodev.h
    strcpy(iv,IV);
    strcpy(key,KEY);
    sess.cipher = CRYPTO_AES_CBC;
    sess.keylen = KEY_SIZE;
    sess.key = key;

    /* h ioctl() elenxei ola ta I/O operations mias suskeuhs kai ta ektelei analoga me to ti 
       8a dwsoume san orisma . Analoga me to request code pou dinw(2o orisma) kanei to katallhlo
       I/O operation.
    */
    if (ioctl(fdnew, CIOCGSESSION, &sess)) {
	perror("ioctl(CIOCGSESSION)");
	return 1;
    }
    cryp.ses = sess.ses;
    cryp.iv = iv;

    /* Sets the bit for the file descriptor sd and 0 in the file descriptor set set1 */
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
	if (FD_ISSET(0, &temp_set)){
	    /* get the message from the stdin and put it to the variable keyboard */
	    fgets(keyboard, MSG_SIZE, stdin);
	    /* use AES for encryption */
        cryp.src = keyboard;
	    cryp.dst = temp;
	    cryp.len = sizeof(keyboard);
	    cryp.op = COP_ENCRYPT;
            if (ioctl(fdnew, CIOCCRYPT, &cryp)) {
			perror("ioctl(CIOCCRYPT)");
			return 1;
            }
            /* write the message to the socket */
            insist_write(sd, temp, sizeof(temp));
	    if (strcmp(keyboard, "koukis\n") == 0){
		    printf("Koukis killed the client(OMG)\n");
		    close(sd); // close the socket
		    exit(1);
	    }   
	}

	if (FD_ISSET(sd, &temp_set)){
	    /* the server send a message and i have to read it */
	    if (insist_read(sd, server_msg, MSG_SIZE) != MSG_SIZE){
			perror("read");
			exit(EXIT_FAILURE);
            }
	    /* time for decryption */
            cryp.src = server_msg;
            cryp.dst = temp;
	    cryp.len = sizeof(server_msg);
	    cryp.op = COP_DECRYPT;
            if (ioctl(fdnew, CIOCCRYPT, &cryp)) {
		perror("ioctl(CIOCCRYPT)");
		return 1;
            }
	    printf("Server sent:%s",temp);
	    if (strcmp(temp, "halt\n") == 0){
			printf("Server killed the client because sent 'halt'\n");
		    	close(sd); // close the socket
		    	exit(1);
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

