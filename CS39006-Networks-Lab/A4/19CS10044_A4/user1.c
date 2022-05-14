
// CS39006 - Networks Laboratory
// Assignment 04 | 14 March 2022
// [ Nakul Aggarwal - 19CS10044 ]
// [ Hritaban Ghosh - 19CS30053 ]

#include "rsocket.h"
#include <stdio.h>
#define MAX_LEN 49
#define MIN_LEN 26

// [ The group member whose roll number is considered in the code is Hritaban Ghosh ]

int main()  {
    int M1;
    
    if ((M1 = r_socket(AF_INET, SOCK_MRP, 0)) < 0)  {
		perror("Socket Creation Failed.\n");
		exit(-1);
	}

    uint16_t user1_port = 50000 + 2 * 53;
    struct sockaddr_in	user1_addr;

    user1_addr.sin_family		= AF_INET;
	user1_addr.sin_addr.s_addr	= INADDR_ANY;
	user1_addr.sin_port		    = htons(user1_port);

    if (r_bind(M1, (struct sockaddr *) &user1_addr, sizeof(user1_addr)) < 0)    {
		perror("Unable to bind local address.\n");
        r_close(M1);
		exit(-1);
	}

    char input[MAX_LEN + 2];
    memset(input, 0, sizeof(input));

    printf("\n\n Enter a string (SIZE b/w %d and %d) : ", MIN_LEN, MAX_LEN);
    fgets(input, sizeof(input), stdin);
    int len = strlen(input) - 1;

    if ( len < MIN_LEN || len > MAX_LEN ) {
        printf("Size restrictions not obeyed.\n");
        r_close(M1);
		exit(-1);
    }

    uint16_t user2_port = 50000 + 2 * 53 + 1;
    struct sockaddr_in	user2_addr;

    user2_addr.sin_family = AF_INET; 
    user2_addr.sin_port = htons(user2_port); 
    inet_aton("127.0.0.1", &user2_addr.sin_addr);

    int char_i = 0;
    
    while ( 1 ) {
        if ( char_i >= len )    continue;
        // the loop should keep running indefinitely because if it terminates,
        // the socket will be closed that would in turn kill the threads S and
        // R and consequently, the unacknowledged messages would not be re-sent
        // if they are lost.

        int b = r_sendto(M1, (const char *)&(input[char_i]), sizeof(char), 0, 
                        (const struct sockaddr *) &user2_addr, sizeof(user2_addr));
        if ( b < 0 ) {
            perror("Unable to send data.\n");
            r_close(M1);
		    exit(-1);
        }
        char_i ++;
    }
    
    printf("\n");
    r_close(M1);
}