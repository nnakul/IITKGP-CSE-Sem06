
// CS39006 - Networks Laboratory
// Nakul Aggarwal | 19CS10044
// Lab Test 01    | 28 February 2022

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#define PORT 6000
#define true 1
#define false 0

void error ( const char * err ) {
    printf("\n [ ERROR : %s ]\n\n", err) ;
    exit(-1) ;
}

int sendall ( int fd , char * buf , int len ) {
    int sent = 0 ;
    int left = len ;
    while ( left ) {
        int b = send(fd, buf+sent, left, 0) ;
        if ( b < 0 )   return -1 ;
        if ( b == 0 )   continue ;
        sent += b ;
        left -= b ;
    }
    return 0 ;
}

int recvall ( int fd , char * buf ) {
    char c ;
    int i = 0 ;
    while ( true ) {
        int b = recv(fd, &c, 1, 0) ;
        if ( b < 0 )   return -1 ;
        if ( b == 0 )   continue ;
        buf[i ++] = c ;
        if ( i > 6 && ! c )  return 0 ;
    }
    return 0 ;
}

int main ( ) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0) ;
    if ( sock_fd < 0 )  error("socket could not be created") ;

    int val = fcntl(sock_fd, F_GETFL, 0);
	int err = fcntl(sock_fd, F_SETFL, val | O_NONBLOCK);
    if (err != 0) {
        error("socket cannot be made nonblocking");
    }

    struct sockaddr_in serv_addr ;
	serv_addr.sin_family = AF_INET ;
	serv_addr.sin_port = htons(PORT) ;
	
	if( inet_pton(AF_INET, "127.0.0.1", &(serv_addr.sin_addr)) <= 0 )	{
        close(sock_fd) ;
        error("inet_pton() returned error") ;
	}

	while ( connect(sock_fd, (struct sockaddr *)(&serv_addr), sizeof(serv_addr)) < 0 )	{
        if ( errno == EINPROGRESS || errno == EALREADY  )   continue ;
		close(sock_fd) ;
        error("connect() returned error") ;
	}

    int N = 1 ;
    while ( true ) {
        int T = rand() % 3 + 1 ;
        sleep(T) ;

        if ( N < 6 ) {
            char msg [] = "Message  " ;
            msg[strlen(msg) - 1] = N + '0' ;

            if ( sendall(sock_fd, msg, strlen(msg) + 1) < 0 ) {
                error("send() returned an error") ;
            }

            printf(" Message %d sent\n", N) ;
            N ++ ;
        }

        char inmsg [30] ;
        memset(inmsg, 0, 30) ;

        while ( true ) {
            int stat = recvall(sock_fd, inmsg) ;
            
            if ( stat < 0 ) {
                if (errno != EWOULDBLOCK && errno != EAGAIN) {
                    error("recv() returned fatal error") ;
                }
                break ;
            }

            struct sockaddr_in sender ;
            memcpy(&sender.sin_addr, inmsg, 4) ;
            memcpy(&sender.sin_port, inmsg+4, 2) ;

            printf(" Client: Recieved \"%s\" from <%s:%d>\n", inmsg+6, inet_ntoa(sender.sin_addr), sender.sin_port) ;
        }
    }
}

/*

Also, write as a comment at the end of the client file how would you change the client 
code if the client did not use non-blocking receive AND read the messages to be sent 
from the keyboard (to be entered by the user) instead of generating fixed messages after 
random sleep.

If you do not want non-blocking recieve, remove the fcntl calls. The flags in send, recv
will still remain 0.
Now if the messages sent by the client are also taken by keyboard, we can use 
scanf function to take a string as input and then send that string to the server
(like before). Now the messages entered by keyboard may not be of fixed length.
So we need to dynamic char-arrays in place of static arrays. Look below.

++++++++++++
    int size = 10 ;
    char c ;
    char * msg = (char*) malloc (sizeof(char) * size) ;
    int idx = 0 ;
    while ( (c = getchar()) != '\n' ) {
        if ( idx == size - 1 ) {
            size *= 2 ;
            msg = (char*) realloc (msg, size) ;
        }
        msg[idx ++] = c ;
    }
    msg[idx] = 0 ;
++++++++++++

Send this message from the client to the server (taking care of the fact that not
all the bytes may be sent in 1 go -> use sendall function implemented in this file).

*/