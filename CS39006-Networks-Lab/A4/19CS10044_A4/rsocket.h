
// CS39006 - Networks Laboratory
// Assignment 04 | 14 March 2022
// [ Nakul Aggarwal - 19CS10044 ]
// [ Hritaban Ghosh - 19CS30053 ]

#ifndef RSOCKET_H
#define RSOCKET_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define T 2
#define p 0.05
#define SOCK_MRP 0
#define TABLE_SIZE 50
#define RECV_SIZE 65535
#define RECV_BLOCK_T 1

typedef struct _unack_msg {
    int valid ;
    int sent_time ;
    int msg_seq_no ;
    int size ;
    void * msg ;
    const struct sockaddr * dest_addr ;
} unack_msg ;

typedef struct _recvd_msg {
    void * msg ;
    int size ;
    struct sockaddr sender_addr ;
    socklen_t sender_addr_len ;
} recvd_msg ;

typedef struct _recvd_msg_queue {
    recvd_msg table [TABLE_SIZE] ;
    int front ;
    int back ;
    int size ;
} recvd_msg_queue ;

int r_socket (int __domain, int __type, int __protocol) ;
int r_bind (int __fd, const struct sockaddr *__addr, socklen_t __len) ;
ssize_t r_sendto (int __fd, const void *__buf, size_t __n, int __flags, 
                const struct sockaddr *__addr, socklen_t __addr_len) ;
ssize_t r_recvfrom (int __fd, void *__restrict__ __buf, size_t __n, int __flags, 
                struct sockaddr *__restrict__ __addr, socklen_t *__restrict__ __addr_len) ;
int r_close (int __fd) ;
int dropMessage ( float __p ) ;

#endif
