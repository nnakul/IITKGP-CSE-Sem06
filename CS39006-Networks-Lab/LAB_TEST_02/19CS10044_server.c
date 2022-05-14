

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
#include <sys/stat.h>
#include <netinet/in.h>
#include <pthread.h>
#define PORT 6000
#define true 1
#define false 0


void error ( const char * msg ) {
    printf("\n\n [ ERROR : %s() returned error ]\n\n", msg) ;
    exit(-1) ;
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


void Delete ( int sock ) {
	char filename[260] ;
	recvall(sock, filename, 0) ;

	if ( remove(filename) )	return ;

	char reply[20] = "delete success" ;
	sendall(sock, reply, strlen(reply) + 1, 0) ;
}


void GetBytes ( int sock ) {
	char filename[260] ;
	char xs[30] ;
	char ys[30] ;
	recvall(sock, filename, 0) ;
	recvall(sock, xs, 0) ;
	recvall(sock, ys, 0) ;

	int x = atoi(xs) ;
	int y = atoi(ys) ;
	if ( x < 0 || y < x )	return ;

	int file = open(filename, O_RDONLY) ;
	if ( file < 0 )	return ;

	struct stat filepr;
	fstat(file, &filepr);
	off_t size = filepr.st_size;
	if ( x >= size || y >= size )	return ;

	char c ;
	int pos = 0 ;
	while ( pos < x ) {
		if ( read(file, &c, 1) < 1 )	return ;
		pos ++ ;
	}

	while ( pos <= y ) {
		if ( read(file, &c, 1) < 1 )	return ;
		if ( send(sock, &c, 1, 0) < 1 )	return ;
		pos ++ ;
	}

	printf("byte %d to byte %d of file %s sent\n", x, y, filename) ;
	return ;
}


void * handle ( void * arg ) {
	int new_sock = *(int*)(arg) ;
	free(arg) ;

	char cmd[20] ;
	recvall(new_sock, cmd, 0) ;

	if ( strcmp(cmd, "del") == 0 )	Delete(new_sock) ;
	else if ( strcmp(cmd, "getbytes") == 0 )	GetBytes(new_sock) ;

	close(new_sock) ;
	pthread_exit(NULL) ;
}



int main ( ) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0) ;
	if ( server_fd < 0 )    error("socket") ;

	struct sockaddr_in serv_addr ;
	serv_addr.sin_family = AF_INET ;
	serv_addr.sin_addr.s_addr = INADDR_ANY ;
	serv_addr.sin_port = htons(PORT) ;
	

	if ( bind(server_fd, (struct sockaddr*)(&serv_addr), sizeof(serv_addr)) < 0 )
        error("bind") ;

    if ( listen(server_fd, 5) < 0 )    error("listen") ;


	struct sockaddr_in cl_add ;
	cl_add.sin_family = AF_INET ;
	cl_add.sin_addr.s_addr = INADDR_ANY ;
	cl_add.sin_port = htons(PORT) ;
	int addrlen = sizeof(cl_add) ;

	while ( true ) {
		int new_sock = accept(server_fd, (struct sockaddr*)(&cl_add), (socklen_t*)(&addrlen)) ;

		int *arg = malloc(sizeof(int));
		if ( ! arg )	error("malloc") ;
		*arg = new_sock ;

		pthread_t tid ;
		if ( pthread_create(&tid, NULL, handle, (void *)(arg)) ) {
			free(arg) ;
			error("pthread_create") ;
		}
	}
}
