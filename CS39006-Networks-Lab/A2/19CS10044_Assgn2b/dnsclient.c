
//	[ CS39006 Computer Networks Laboratory ]
//		[ Assignment 02.B   |	Using TCP Sockets  ]
//		[ 24 January 2022 	| 	Monday			   ]
//		[ Nakul Aggarwal	|	19CS10044		   ]

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
#define NAME_SIZE  260
#define RECV_SIZE  200
#define true 1
#define false 0
#define __debug__	0
#define WAIT_TIME	2

int main ( )	{
	
    int client_sock = socket(AF_INET, SOCK_DGRAM, 0) ;
    if ( client_sock < 0 ) {
        printf("\n [ ERROR : Client socket could not be created. ]\n\n") ;
		return -1 ;
    }

	struct sockaddr_in serv_addr ;
	serv_addr.sin_family = AF_INET ;
	serv_addr.sin_port = htons(PORT) ;
	
	if( inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0 )	{
		printf("\n [ ERROR : Invalid/unsupported server address. ]\n\n") ;
		close(client_sock) ;
		return -2 ;
	}

	char dnsname [NAME_SIZE + 1] ;
	printf("\n Enter DNS name : ") ;
	scanf("%s", dnsname) ;

	struct timeval tv ;
	fd_set readfds ;
	int addrlen = sizeof(serv_addr) ;

	int bytes_sent = sendto(client_sock, dnsname, strlen(dnsname), MSG_CONFIRM, 
							(struct sockaddr*)(&serv_addr), addrlen) ;
	if ( bytes_sent < 0 ) {
		printf("\n [ ERROR : Data not successfully sent by the client. ]\n\n") ;
		close(client_sock) ;
		return -3 ;
	}
	if ( __debug__ )	printf("\n [ %d bytes sent from client to server ]", bytes_sent) ;

	char ip_add[RECV_SIZE + 1] ;

	while ( true ) {
		tv.tv_sec = WAIT_TIME ;
		tv.tv_usec = 0 ;
		FD_ZERO(&readfds) ;
		FD_SET(client_sock, &readfds) ;
		int rec = select(client_sock + 1, &readfds, NULL, NULL, &tv) ;

		if ( rec < 0 ) {
			printf("\n [ ERROR : Error given by 'select()'. ]\n\n") ;
			close(client_sock) ;
			return -4 ;
		}

		if ( ! FD_ISSET(client_sock, &readfds) ) {
			printf("\n [ ERROR : Oops! Timeout! Client had to wait for too long to recieve data. ]\n\n") ;
			close(client_sock) ;
			return -5 ;
		}

		int bytes_recv = recvfrom(client_sock, ip_add, RECV_SIZE, MSG_WAITALL, 
							  	  (struct sockaddr*)(&serv_addr), &addrlen) ;
		if ( bytes_recv < 0 ) {
			printf("\n [ ERROR : Data not successfully recieved by the client. ]\n\n") ;
			close(client_sock) ;
			return -6 ;
		}
		if ( __debug__ )	printf("\n [ %d bytes recieved from server by client ]", bytes_recv) ;

		ip_add[bytes_recv] = 0 ;

		int dobreak = (bytes_recv == 0) ;
		if ( bytes_recv > 0 && ip_add[bytes_recv - 1] == EOF ) {
			ip_add[bytes_recv - 1] = 0 ;
			bytes_recv -- ;
			dobreak = true ;
		}

		if ( bytes_recv )	printf("%s", ip_add) ;
		if ( dobreak )	break ;
	}
	
	printf("\n\n") ;
	close(client_sock) ;
	return 0 ;
}