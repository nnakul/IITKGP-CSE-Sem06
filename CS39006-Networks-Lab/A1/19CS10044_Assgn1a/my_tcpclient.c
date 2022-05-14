
//	[ CS39006 Computer Networks Laboratory ]
//		[ Assignment 01 	|	Socket Programming ]
//		[ 17 January 2022 	| 	Monday			   ]
//		[ Nakul Aggarwal	|	19CS10044		   ]
//	{ SUMMARY : Client program for TCP sockets }

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#define PORT 8080
#define CHUNK_SIZE  100
#define true 1
#define RECV_SIZE	100
#define __debug__	0

int main ( int argc , char const *argv[] )	{
    
	if ( argc < 2 ) {
        printf("\n [ ERROR : File name not provided. ]\n\n") ;
        return -1 ;
    }

    int file = open(argv[1], O_RDONLY) ;
    if ( file < 0 ) {
        printf("\n [ ERROR : File not found. ]\n\n") ;
        return -2 ;
    }

    int client_sock = socket(AF_INET, SOCK_STREAM, 0) ;
    if ( client_sock < 0 ) {
        printf("\n [ ERROR : Client socket could not be created. ]\n\n") ;
		close(file) ;
        return -3 ;
    }

	struct sockaddr_in serv_addr ;
	serv_addr.sin_family = AF_INET ;
	serv_addr.sin_port = htons(PORT) ;
	
	if( inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0 )	{
		printf("\n [ ERROR : Invalid/unsupported server address. ]\n\n") ;
		close(file) ;	close(client_sock) ;
		return -5 ;
	}
	if ( connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 )	{
		printf("\n [ ERROR : Server-client connection could not be established. ]\n\n") ;
		close(file) ;	close(client_sock) ;
		return -6 ;
	}

	char file_buf [CHUNK_SIZE + 1] ;
	char finish[2] = {EOF, 0} ;

    while ( true ) {
        int size_read = read(file, file_buf, CHUNK_SIZE) ;

		if ( size_read < 0 ) {
			printf("\n [ ERROR : File reading disrupted. ]\n\n") ;
			close(file) ;	close(client_sock) ;
        	return -4 ;
		}
        if ( size_read == 0 )	break ;

        file_buf[size_read] = '\0' ;
		int bytes_sent = send(client_sock, file_buf, strlen(file_buf), 0) ;
		if ( __debug__ )	printf("\n [ %d bytes sent from client to server ]", bytes_sent) ;
    }

	int bytes_sent = send(client_sock, finish, strlen(finish), 0) ;
	if ( __debug__ )	printf("\n [ %d bytes sent from client to server ]", bytes_sent) ;

	char text_info[RECV_SIZE + 1] ;
	int bytes_recv = recv(client_sock, text_info, RECV_SIZE, 0) ;
	if ( __debug__ )	printf("\n [ %d bytes recieved from server by client ]", bytes_recv) ;

	printf("\n\n%s", text_info) ;

	printf("\n\n") ;
	close(file) ;
	close(client_sock) ;
	return 0 ;
}