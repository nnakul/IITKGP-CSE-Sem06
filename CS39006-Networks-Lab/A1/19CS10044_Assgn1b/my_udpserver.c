
//	[ CS39006 Computer Networks Laboratory ]
//		[ Assignment 01 	|	Socket Programming ]
//		[ 17 January 2022 	| 	Monday			   ]
//		[ Nakul Aggarwal	|	19CS10044		   ]
//	{ SUMMARY : Server program for UDP sockets }

#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#define PORT 8080
#define true 1
#define false 0
#define BUF_SIZE	100
#define SEND_SIZE	100
#define __debug__	0

int chars = 0 ;
int words = 0 ;
int sents = 0 ;
char prev = 0 ;
int sig = 0 ;

int WhiteSpace ( char c ) {
	if ( c == ' ' || c == '\t' )	return true ;
	return false ;
}

void Compute ( const char * para ) {
	int c = strlen(para) ;
    if ( c == 0 )   return ;

	for ( int i = 0 ; i < c ; i ++ ) {
		int prevstate = (WhiteSpace(prev) || ! prev) ;
		int presstate = WhiteSpace(para[i]) ;
		if ( prevstate && presstate )	continue ;
		
		if ( para[i] == '.' ) {
			sents ++ ;
			if ( ! prevstate ) {
				if ( sig )	words -- ;
				words ++ ;
				sig = 1 ;
				prev = ' ' ;
				continue ;
			}
		}

		else if ( presstate ) {
			if ( sig )	words -- ;
			words ++ ;
			sig = 1 ;
		}

        else    sig = 0 ;
		prev = para[i] ;
	}

	chars += c ;
	return ;
}

int main(int argc, char const *argv[])  {
	
	int server_fd = socket(AF_INET, SOCK_DGRAM, 0) ;
	if ( server_fd < 0 ) {
		printf("\n [ ERROR : Server socket could not be created. ]\n\n") ;
        return -1 ;
	}
	
	int opt = 1 ;
	if ( setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) ) {
		printf("\n [ ERROR : Server socket could not be attached to the 8080 port. ]\n\n") ;
        close(server_fd) ; return -1 ;
	}

	struct sockaddr_in address ;
	address.sin_family = AF_INET ;
	address.sin_addr.s_addr = INADDR_ANY ;
	address.sin_port = htons(PORT) ;

	if ( bind(server_fd, (struct sockaddr*)(&address), sizeof(address)) < 0 )	{
		printf("\n [ ERROR : Server socket could not be bound. ]\n\n") ;
        close(server_fd) ; return -2 ;
	}

	int addrlen = sizeof(address) ;
	char buffer[BUF_SIZE + 1] ;
	char result[SEND_SIZE + 1] ;

	while ( true ) {
		struct sockaddr_in client_addr ;
		int clientaddrlen = sizeof(client_addr) ;

        chars = words = sents = prev = sig = 0 ;

		while ( true ) {
			int bytes_recv = recvfrom(server_fd, buffer, BUF_SIZE, MSG_WAITALL, 
									  (struct sockaddr*)(&client_addr), &clientaddrlen) ;
			buffer[bytes_recv] = '\0' ;

			if ( __debug__ )	printf("\n [ %d bytes recieved from client by server ]", bytes_recv) ;
			
			int dobreak = false ;
			if ( bytes_recv > 0 && buffer[bytes_recv - 1] == EOF ) {
				dobreak = true ;
				buffer[bytes_recv - 1] = 0 ;
				bytes_recv -- ;
			}

			Compute(buffer) ;
			if ( dobreak )	break ;
		}

        if ( words )    words ++ ;
        if ( sig )      words -- ;
        sprintf(result, " CHARACTERS : %d\n WORDS : %d\n SENTENCES : %d", chars, words, sents) ;
		
        int bytes_sent = sendto(server_fd, result, strlen(result) + 1, MSG_CONFIRM, 
								(struct sockaddr*)(&client_addr), clientaddrlen) ;
		if ( __debug__ )	printf("\n [ %d bytes sent from server to client ]", bytes_sent) ;
	}

	return 0 ;
}