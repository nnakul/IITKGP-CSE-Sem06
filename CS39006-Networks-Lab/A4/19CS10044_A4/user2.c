
// CS39006 - Networks Laboratory
// Assignment 04 | 14 March 2022
// [ Nakul Aggarwal - 19CS10044 ]
// [ Hritaban Ghosh - 19CS30053 ]

#include "rsocket.h"
#include <stdio.h>

// [ The group member whose roll number is considered in the code is Hritaban Ghosh ]

int main()  {
    int M2;
    
    if ((M2 = r_socket(AF_INET, SOCK_MRP, 0)) < 0)  {
		perror("Socket Creation Failed.\n");
		exit(-1);
	}

    uint16_t user2_port = 50000 + 2 * 53 + 1;
    struct sockaddr_in	user2_addr;

    user2_addr.sin_family		= AF_INET;
	user2_addr.sin_addr.s_addr	= INADDR_ANY;
	user2_addr.sin_port		    = htons(user2_port);

    if (r_bind(M2, (struct sockaddr *) &user2_addr, sizeof(user2_addr)) < 0)    {
		perror("Unable to bind local address.\n");
        r_close(M2);
		exit(-1);
	}

    uint16_t user1_port = 50000 + 2 * 53;
    struct sockaddr_in	user1_addr;

    user1_addr.sin_family = AF_INET; 
    user1_addr.sin_port = htons(user1_port); 
    inet_aton("127.0.0.1", &user1_addr.sin_addr);
    socklen_t user1_len = sizeof(user1_addr);

    int number_of_bytes_received;
    
    while(1) {
        char buf[5] ;
        number_of_bytes_received = r_recvfrom(M2, buf, 5, 0, 
                                    (struct sockaddr *)&user1_addr, &user1_len);
        if ( number_of_bytes_received < 0 ) {
            perror("Unable to recieve data.\n");
            r_close(M2);
		    exit(-1);
        }
        
        printf("%c", buf[4]);
        fflush(stdout);
        
        if (number_of_bytes_received<=0)
            break;
    }

    printf("\n");
    r_close(M2);
}