
//	[ CS39006 Computer Networks Laboratory ]
//		[ Assignment 02.A 	|	Using TCP Sockets  ]
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
#define true 1
#define false 0
#define BUF_SIZE	260
#define SEND_SIZE	200
#define __debug__	0
#define IP_ADDRESSES    hosts->h_addr_list
#define EMPTY_IP	"0.0.0.0"

int main ( ) {
	
	int server_fd = socket(AF_INET, SOCK_DGRAM, 0) ;
	if ( server_fd < 0 ) {
		printf("\n [ ERROR : Server socket could not be created. ]\n\n") ;
        return -1 ;
	}

	int opt = 1 ;
	if ( setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) ) {
		printf("\n [ ERROR : Error returned by 'setsockopt()' ]\n\n") ;
        close(server_fd) ; return -2 ;
	}
	
	struct sockaddr_in address ;
	address.sin_family = AF_INET ;
	address.sin_addr.s_addr = INADDR_ANY ;
	address.sin_port = htons(PORT) ;

	if ( bind(server_fd, (struct sockaddr*)(&address), sizeof(address)) < 0 )	{
		printf("\n [ ERROR : Server socket could not be bound. ]\n\n") ;
        close(server_fd) ; return -3 ;
	}

	int addrlen = sizeof(address) ;
	char buffer[BUF_SIZE + 1] ;
	char message[SEND_SIZE + 1] ;
	char finish[] = { EOF , 0 } ;

	while ( true ) {
		struct sockaddr_in client_addr ;
		int clientaddrlen = sizeof(client_addr) ;

        int bytes_recv = recvfrom(server_fd, buffer, BUF_SIZE, MSG_WAITALL, 
								  (struct sockaddr*)(&client_addr), &clientaddrlen) ;
        if ( bytes_recv < 0 ) {
            printf("\n [ ERROR : Data not successfully recieved by the server. ]\n\n") ;
            close(server_fd) ; return -4 ;
        }
		
        buffer[bytes_recv] = '\0' ;
        if ( __debug__ )	printf("\n [ %d bytes recieved from client by server ]", bytes_recv) ;

		struct hostent * hosts ;
		hosts = gethostbyname(buffer) ;

		if ( ! hosts ) {
			sprintf(message, "\n >>> %s { No IP Address Found }", EMPTY_IP) ;
			int bytes_sent = sendto(server_fd, message, strlen(message), MSG_CONFIRM, 
									(struct sockaddr*)(&client_addr), clientaddrlen) ;
			if ( bytes_sent < 0 ) {
				printf("\n [ ERROR : Data not successfully sent by the server. ]\n\n") ;
				close(server_fd) ; return -5 ;
			}
			if ( __debug__ )	printf("\n [ %d bytes sent from server to client ]", bytes_sent) ;
			
			bytes_sent = sendto(server_fd, finish, strlen(finish), MSG_CONFIRM, 
								(struct sockaddr*)(&client_addr), clientaddrlen) ;
			if ( bytes_sent < 0 ) {
				printf("\n [ ERROR : Data not successfully sent by the server. ]\n\n") ;
				close(server_fd) ; return -5 ;
			}
			if ( __debug__ )	printf("\n [ %d bytes sent from server to client ]", bytes_sent) ;
			
			continue ;
		}
		
		char * ipadd ;
		int i = 0 ;
		while ( IP_ADDRESSES[i] ) {
		    ipadd = inet_ntoa(*((struct in_addr*)(IP_ADDRESSES[i]))) ;
			sprintf(message, "\n >>> %s", ipadd) ;
		    
			int bytes_sent = sendto(server_fd, message, strlen(message), MSG_CONFIRM, 
									(struct sockaddr*)(&client_addr), clientaddrlen) ;
			if ( bytes_sent < 0 ) {
				printf("\n [ ERROR : Data not successfully sent by the server. ]\n\n") ;
				close(server_fd) ; return -5 ;
			}
			if ( __debug__ )	printf("\n [ %d bytes sent from server to client ]", bytes_sent) ;

		    i ++ ;
		}
		
        int bytes_sent = sendto(server_fd, finish, strlen(finish), MSG_CONFIRM, 
								(struct sockaddr*)(&client_addr), clientaddrlen) ;
		if ( bytes_sent < 0 ) {
			printf("\n [ ERROR : Data not successfully sent by the server. ]\n\n") ;
			close(server_fd) ; return -5 ;
		}
		if ( __debug__ )	printf("\n [ %d bytes sent from server to client ]", bytes_sent) ;
	}

	close(server_fd) ; 
	return 0 ;
}