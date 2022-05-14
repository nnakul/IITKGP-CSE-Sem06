

// Computer Networks Laboratory (CS39006)
// Lab Test 02  |  04 April 2022  |  Monday
// Nakul Aggarwal | 19CS10044

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


void error ( const char * msg ) {
    printf("\n\n [ ERROR : %s() returned error ]\n\n", msg) ;
    exit(-1) ;
}

int sendall ( int fd , char * buf , int size , int flag ) {
	// send whole buffer (for TCP) -> safe send()
	int left = size ;
	int off = 0 ;
	while ( left ) {
		int bs = send(fd, buf+off, left, flag) ;
		if ( bs < 0 )	error("send") ;
		off += bs ;
		left -= bs ;
	}
	return 0 ;
}

int recvall ( int fd , char * buf , int flag ) {
	// recv upto delimiter '\0' (for TCP connection)
	int i = 0 ;
	char c ;
	while ( true ) {
		int t = recv(fd, &c, 1, flag) ;
		if ( t < 0 )	error("recv") ;
		buf[i ++] = c ;
		if ( ! c )	break ;
	}
	return 0 ;
}


void Delete ( char * ) ;
void GetBytes ( char * , char * , char * ) ;


int main ( int argc , char ** argv ) {
	printf("\n\n") ;
	if ( argc != 3 && argc != 5 )	exit(-1) ;

	if ( argc == 3 ) {
		if ( strcmp(argv[1], "del") )	exit(-1) ;
		Delete(argv[2]) ;
	}

	else {
		if ( strcmp(argv[1], "getbytes") )	exit(-1) ;
		GetBytes(argv[2], argv[3], argv[4]) ;
	}
}

void Delete ( char * filename ) {
	if ( ! filename || ! strlen(filename) )	exit(-1) ;

	int client_sock = socket(AF_INET, SOCK_STREAM, 0) ;
    if ( client_sock < 0 )	error("socket") ;
    
    struct sockaddr_in serv_addr ;
	serv_addr.sin_family = AF_INET ;
	serv_addr.sin_port = htons(PORT) ;
	serv_addr.sin_addr.s_addr = INADDR_ANY ;

    if ( connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 )	{
		close(client_sock) ;
		error("connect") ;
	}

	char del [4] = "del" ;
	int flen = strlen(filename) ;
	if ( send(client_sock, del, 4, 0) < 4 )	error("send") ;
	if ( send(client_sock, filename, flen + 1, 0) < flen + 1 )	error("send") ;

	char reply[20] ;
	if ( recvall(client_sock, reply, 0) < 0 ) {
		printf(" [ DELETION ERROR ]\n") ;
	}

	else if ( strcmp(reply, "delete success") == 0 ) {
		printf(" [ DELETION SUCCESS ]\n") ;
	}

	else {
		printf(" [ DELETION ERROR ]\n") ;
	}

	printf("\n\n") ;
	close(client_sock) ;
	return ;
}


void GetBytes ( char * filename , char * x , char * y ) {
	
	int client_sock = socket(AF_INET, SOCK_STREAM, 0) ;
    if ( client_sock < 0 )	error("socket") ;
    
    struct sockaddr_in serv_addr ;
	serv_addr.sin_family = AF_INET ;
	serv_addr.sin_port = htons(PORT) ;
	serv_addr.sin_addr.s_addr = INADDR_ANY ;

    if ( connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 )	{
		close(client_sock) ;
		error("connect") ;
	}

	char cmd [9] = "getbytes" ;
	int flen = strlen(filename) ;

	if ( send(client_sock, cmd, 9, 0) < 9 )	error("send") ;
	if ( send(client_sock, filename, flen + 1, 0) < flen + 1 )	error("send") ;
	if ( send(client_sock, x, strlen(x) + 1, 0) < strlen(x) + 1 )	error("send") ;
	if ( send(client_sock, y, strlen(y) + 1, 0) < strlen(y) + 1 )	error("send") ;

	int len = atoi(y) - atoi(x) + 1 ;
	char c ;
	while ( len -- ) {
		int t = recv(client_sock, &c, 1, 0) ;
		if ( t < 1 ) {
			close(client_sock) ;
			printf(" [ GETBYTES ERROR ]\n\n\n") ;
			exit(-1) ;
		}
		printf("%c", c) ;
	}

	printf("\n\n") ;
	close(client_sock) ;
	return ;
}
