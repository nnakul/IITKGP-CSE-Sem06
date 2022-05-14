
//	[ CS39006 Computer Networks Laboratory ]
//		[ Assignment 02.B 	|	Using TCP Sockets  ]
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
#define __SUCCESS__ 0
#define __FAILURE__ -1

int Handle_TCP_Request ( int server_fd , struct sockaddr_in * addressp ) ;
int Handle_UDP_Request ( int server_fd ) ;

int main ( )  {

    struct sockaddr_in address ;
	address.sin_family = AF_INET ;
	address.sin_addr.s_addr = INADDR_ANY ;
	address.sin_port = htons(PORT) ;
    
    // Create & bind a UDP Socket to handle requests from a UDP client
    int udp_server_fd = socket(AF_INET, SOCK_DGRAM, 0) ;
	if ( udp_server_fd < 0 ) {
		printf("\n [ ERROR : UDP server socket could not be created. ]\n\n") ;
        exit(__FAILURE__) ;
	}

	int udp_opt = 1 ;
	if ( setsockopt(udp_server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &udp_opt, sizeof(udp_opt)) ) {
		printf("\n [ ERROR : UDP server socket -- error returned by 'setsockopt()' ]\n\n") ;
        close(udp_server_fd) ; exit(__FAILURE__) ;
	}
	
	if ( bind(udp_server_fd, (struct sockaddr*)(&address), sizeof(address)) < 0 )	{
		printf("\n [ ERROR : UDP server socket could not be bound. ]\n\n") ;
        close(udp_server_fd) ; exit(__FAILURE__) ;
	}

    // Create & bind a TCP Socket to handle requests from a TCP client
    int tcp_server_fd = socket(AF_INET, SOCK_STREAM, 0) ;
	if ( tcp_server_fd < 0 ) {
		printf("\n [ ERROR : TCP server socket could not be created. ]\n\n") ;
        close(udp_server_fd) ; exit(__FAILURE__) ;
	}

    int tcp_opt = 1 ;
	if ( setsockopt(tcp_server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &tcp_opt, sizeof(tcp_opt)) ) {
		printf("\n [ ERROR : UDP server socket -- error returned by 'setsockopt()' ]\n\n") ;
        close(udp_server_fd) ; close(tcp_server_fd) ; exit(__FAILURE__) ;
	}

	if ( bind(tcp_server_fd, (struct sockaddr*)(&address), sizeof(address)) < 0 )	{
		printf("\n [ ERROR : TCP server socket could not be bound. ]\n\n") ;
        close(udp_server_fd) ; close(tcp_server_fd) ; exit(__FAILURE__) ;
	}

	if ( listen(tcp_server_fd, 10) < 0 )	{
		printf("\n [ ERROR : TCP server could not be opened for listening to client connections. ]\n\n") ;
        close(udp_server_fd) ; close(tcp_server_fd) ; exit(__FAILURE__) ;
	}
    
    fd_set readfds ;
    int max_fd = udp_server_fd ;
    if ( tcp_server_fd > max_fd )   max_fd = tcp_server_fd ;

    while ( true ) {
        FD_ZERO(&readfds) ;
		FD_SET(tcp_server_fd, &readfds) ;
        FD_SET(udp_server_fd, &readfds) ;
        int rec = select(max_fd + 1, &readfds, NULL, NULL, NULL) ;

        if ( rec < 0 ) {
			printf("\n [ ERROR : Error given by 'select()'. ]\n\n") ;
			close(udp_server_fd) ; close(tcp_server_fd) ; exit(__FAILURE__) ;
		}

        if ( FD_ISSET(tcp_server_fd, &readfds) )    Handle_TCP_Request(tcp_server_fd, &address) ;
        if ( FD_ISSET(udp_server_fd, &readfds) )    Handle_UDP_Request(udp_server_fd) ;
    }
}

int Handle_TCP_Request ( int server_fd , struct sockaddr_in * addressp ) {
    
    int addrlen = sizeof(*addressp) ;
    char buffer[BUF_SIZE + 1] ;
	char message[SEND_SIZE + 1] ;
	char finish[] = { EOF , 0 } ;

    int new_socket = accept(server_fd, (struct sockaddr*)(addressp), (socklen_t*)(&addrlen)) ;
    if ( new_socket < 0 ) {
        printf("\n\n [ ERROR : TCP server did not accept connection request successfully. ]\n\n") ;
        return __FAILURE__ ;
    }

    if ( __debug__)	printf("\n\n [ SUCCESS : TCP server accepted connection request. ]") ;

    int bytes_recv = recv(new_socket, buffer, BUF_SIZE, 0) ;

    int pid = fork() ;  // Create a (new) child process to handle the request of a TCP socket
    if ( pid < 0 ) {
        printf("\n [ ERROR : TCP server could not start a child process. ]\n\n") ;
        close(new_socket) ; return __FAILURE__ ;
    }
    if ( pid > 0 ) { close(new_socket) ; return __SUCCESS__ ; }

    //  CHILD PROCESS -- Handles (concurrently) the request sent by a TCP client
    if ( __debug__)	printf("\n [ SUCCESS : TCP server created a child process. ]") ;
    close(server_fd) ;

    if ( bytes_recv < 0 ) {
        printf("\n [ ERROR : Data not successfully recieved by the TCP server. ]") ;
        close(new_socket) ; exit(__FAILURE__) ;
    }
    
    buffer[bytes_recv] = '\0' ;
    if ( __debug__ )	printf("\n [ TCP SERVER : %d bytes recieved from client by server ]", bytes_recv) ;

    struct hostent * hosts ;
    hosts = gethostbyname(buffer) ;

    if ( ! hosts ) {
        sprintf(message, "\n >>> %s { No IP Address Found }", EMPTY_IP) ;
        
        int bytes_sent = send(new_socket, message, strlen(message), 0) ;
        if ( bytes_sent < 0 ) {
            printf("\n [ ERROR : Data not successfully sent by the TCP server. ]\n\n") ;
            close(new_socket) ; exit(__FAILURE__) ;
        }
        if ( __debug__ )	printf("\n [ TCP SERVER : %d bytes sent from server to client ]", bytes_sent) ;
        
        bytes_sent = send(new_socket, finish, strlen(finish), 0) ;
        if ( bytes_sent < 0 ) {
            printf("\n [ ERROR : Data not successfully sent by the TCP server. ]\n\n") ;
            close(new_socket) ; exit(__FAILURE__) ;
        }
        if ( __debug__ )	printf("\n [ TCP SERVER : %d bytes sent from server to client ]", bytes_sent) ;

        close(new_socket) ;
        exit(__SUCCESS__) ;
    }
    
    char * ipadd ;
    int i = 0 ;
    while ( IP_ADDRESSES[i] ) {
        ipadd = inet_ntoa(*((struct in_addr*)(IP_ADDRESSES[i]))) ;
        sprintf(message, "\n >>> %s", ipadd) ;
        
        int bytes_sent = send(new_socket, message, strlen(message), 0) ;
        if ( bytes_sent < 0 ) {
            printf("\n [ ERROR : Data not successfully sent by the TCP server. ]\n\n") ;
            close(new_socket) ; exit(__FAILURE__) ;
        }
        if ( __debug__ )	printf("\n [ TCP SERVER : %d bytes sent from server to client ]", bytes_sent) ;

        i ++ ;
    }
    
    int bytes_sent = send(new_socket, finish, strlen(finish), 0) ;
    if ( bytes_sent < 0 ) {
        printf("\n [ ERROR : Data not successfully sent by the TCP server. ]\n\n") ;
        close(new_socket) ; exit(__FAILURE__) ;
    }
    if ( __debug__ )	printf("\n [ TCP SERVER : %d bytes sent from server to client ]", bytes_sent) ;

    close(new_socket) ;
    exit(__SUCCESS__) ;
}

int Handle_UDP_Request ( int server_fd ) {
    
    struct sockaddr_in client_addr ;
    int clientaddrlen = sizeof(client_addr) ;
    char buffer[BUF_SIZE + 1] ;
	char message[SEND_SIZE + 1] ;
	char finish[] = { EOF , 0 } ;

    int bytes_recv = recvfrom(server_fd, buffer, BUF_SIZE, MSG_WAITALL, 
                             (struct sockaddr*)(&client_addr), &clientaddrlen) ;
    if ( bytes_recv < 0 ) {
        printf("\n\n [ ERROR : Data not successfully recieved by the UDP server. ]\n\n") ;
        return __FAILURE__ ;
    }
    
    buffer[bytes_recv] = '\0' ;
    if ( __debug__ )	printf("\n\n [ UDP SERVER : %d bytes recieved from client by server ]", bytes_recv) ;

    struct hostent * hosts ;
    hosts = gethostbyname(buffer) ;

    if ( ! hosts ) {
        sprintf(message, "\n >>> %s { No IP Address Found }", EMPTY_IP) ;
        int bytes_sent = sendto(server_fd, message, strlen(message), MSG_CONFIRM, 
                                (struct sockaddr*)(&client_addr), clientaddrlen) ;
        if ( bytes_sent < 0 ) {
            printf("\n [ ERROR : Data not successfully sent by the UDP server. ]\n\n") ;
            return __FAILURE__ ;
        }
        if ( __debug__ )	printf("\n [ UDP SERVER : %d bytes sent from server to client ]", bytes_sent) ;
        
        bytes_sent = sendto(server_fd, finish, strlen(finish), MSG_CONFIRM, 
                            (struct sockaddr*)(&client_addr), clientaddrlen) ;
        if ( bytes_sent < 0 ) {
            printf("\n [ ERROR : Data not successfully sent by the UDP server. ]\n\n") ;
            return __FAILURE__ ;
        }
        if ( __debug__ )	printf("\n [ UDP SERVER : %d bytes sent from server to client ]", bytes_sent) ;
        
        return __SUCCESS__ ;
    }
    
    char * ipadd ;
    int i = 0 ;
    while ( IP_ADDRESSES[i] ) {
        ipadd = inet_ntoa(*((struct in_addr*)(IP_ADDRESSES[i]))) ;
        sprintf(message, "\n >>> %s", ipadd) ;
        
        int bytes_sent = sendto(server_fd, message, strlen(message), MSG_CONFIRM, 
                                (struct sockaddr*)(&client_addr), clientaddrlen) ;
        if ( bytes_sent < 0 ) {
            printf("\n [ ERROR : Data not successfully sent by the UDP server. ]\n\n") ;
            return __FAILURE__ ;
        }
        if ( __debug__ )	printf("\n [ UDP SERVER : %d bytes sent from server to client ]", bytes_sent) ;

        i ++ ;
    }
    
    int bytes_sent = sendto(server_fd, finish, strlen(finish), MSG_CONFIRM, 
                            (struct sockaddr*)(&client_addr), clientaddrlen) ;
    if ( bytes_sent < 0 ) {
        printf("\n [ ERROR : Data not successfully sent by the UDP server. ]\n\n") ;
        return __FAILURE__ ;
    }
    if ( __debug__ )	printf("\n [ UDP SERVER : %d bytes sent from server to client ]", bytes_sent) ;

    return __SUCCESS__ ;
}