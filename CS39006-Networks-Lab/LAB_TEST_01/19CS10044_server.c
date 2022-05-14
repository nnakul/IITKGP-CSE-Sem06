
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
#define MAX_LEN 100

void error ( const char * err ) {
    printf("\n [ ERROR : %s ]\n\n", err) ;
    exit(-1) ;
}

int max ( int x , int y ) {
    if ( x >= y )   return x ;
    return y ;
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
        if ( b <  0 )   return -1 ;
        if ( b == 0 )   continue ;
        buf[i ++] = c ;
        if ( ! c )  break ;
    }
    return 0 ;
}

int main ( ) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0) ;
    if ( sock_fd <  0 ) {
        error("socket could not be created") ;
    }

    int opt = 1 ;
	if ( setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) ) {
		close(sock_fd) ;
        error("setsocketopt() returned error") ;
	}

    struct sockaddr_in my_address ;
	my_address.sin_family = AF_INET ;
	my_address.sin_addr.s_addr = INADDR_ANY ;
	my_address.sin_port = htons(PORT) ;

    if ( bind(sock_fd, (struct sockaddr*)(&my_address), sizeof(my_address)) < 0 )	{
		close(sock_fd) ;
        error("bind() returned error") ;
	}

	if ( listen(sock_fd, 3) < 0 )	{
		close(sock_fd) ;
        error("listen() returned error") ;
	}

    int clientsockfd [3] = {-1, -1, -1} ;
    struct sockaddr_in clientId[3] ;
    int numclient = 0 ;

    fd_set readfds ;
    FD_ZERO(&readfds) ;
    int max_fd = sock_fd ;

    while ( true ) {
        FD_ZERO(&readfds) ;
        FD_SET(sock_fd, &readfds) ;
        for ( int i = 0 ; i < numclient ; i ++ )
            FD_SET(clientsockfd[i], &readfds) ;
        
        int rec = select(max_fd + 1, &readfds, NULL, NULL, NULL) ;


        if ( rec < 0 ) {
            error("select() returned error") ;
        }

        if ( FD_ISSET(sock_fd, &readfds) ) {
            if ( numclient < 3 ) {
                int addrlen = sizeof(clientId[numclient]) ;
                int new_fd = accept(sock_fd, (struct sockaddr*)(&clientId[numclient]), (socklen_t*)(&addrlen)) ;
                
                max_fd = max(max_fd, new_fd) ;
                
                clientsockfd[numclient ++] = new_fd ;
                printf(" Server: Recieved a new connection from client <%s:%d>\n", inet_ntoa(clientId[numclient-1].sin_addr), clientId[numclient-1].sin_port) ;
            }
        }

        else if ( numclient ) {
            for ( int i = 0 ; i < numclient ; i ++ ) {
                if ( ! FD_ISSET(clientsockfd[i], &readfds) )    continue ;
                
                char msg[20] ;
                if ( recvall(clientsockfd[i], msg) < 0 ) {
                    error("recv() returned error") ;
                }

                printf(" Server: Recieved message \"%s\" from client <%s:%d>\n", msg, inet_ntoa(clientId[i].sin_addr), clientId[i].sin_port) ;

                if ( numclient == 1 ) {
                    printf(" Server: Insufficient cleints, \"%s\" from client <%s:%d> dropped\n", 
                        msg, inet_ntoa(clientId[i].sin_addr), clientId[i].sin_port) ;
                }

                else {
                    char frame[26] ;
                    memset(frame, 0, 26) ;
                    memcpy(frame, &clientId[i].sin_addr, 4) ;
                    memcpy(frame+4, &clientId[i].sin_port, 2) ;
                    memcpy(frame+6, msg, strlen(msg) + 1) ;

                    for ( int k = 0 ; k < numclient ; k ++ ) {
                        if ( k == i )   continue ;
                        if ( sendall(clientsockfd[k], frame, strlen(msg) + 1 + 6) < 0 ) {
                            error("send() returned error") ;
                        }

                        printf(" Server: Sent message \"%s\" from client <%s:%d> to client <%s:%d>\n", msg, 
                            inet_ntoa(clientId[i].sin_addr), clientId[i].sin_port, inet_ntoa(clientId[k].sin_addr), clientId[k].sin_port) ;
                    }
                }
            }
        }
    }
}