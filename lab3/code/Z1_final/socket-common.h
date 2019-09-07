/*
 * socket-common.h
 *
 * Simple TCP/IP communication using sockets
 *
 * Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
 *
 */

#ifndef _SOCKET_COMMON_H
#define _SOCKET_COMMON_H

/* Compile-time options */
#define TCP_PORT    35001
#define TCP_BACKLOG 5
#define MSG_SIZE 150
#define MAX_CLIENTS 100


#define KEY_SIZE	16
#define BLOCK_SIZE	16

#define IV		"0123456789ABCDEF"
#define KEY		"0123456789ABCDEF"

#endif /* _SOCKET_COMMON_H */

