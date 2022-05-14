// CS39006 COMPUTER NETWORKS LABORATORY
// Assignment 5
// Implementation of Traceroute using Raw Sockets
// Nakul Aggarwal 19CS10044
// Hritaban Ghosh 19CS30053

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#define MAX_HOP                         16
#define TIMEOUT                         1   // in seconds
#define MAX_REPEAT                      3

#define SOURCE_PORT                     8000
#define DESTINATION_PORT                32164
#define ICMP_MESSAGE_SIZE               2048
#define PAYLOAD_SIZE                    52
#define IP_ADDRESS_SIZE                 32

#define IP_HEADER_LENGTH                5
#define IPV4_VERSION_NUMBER             4
#define ROUTINE_NORMAL_SERVICE          0
#define IP_PACKET_IDENTIFICATION_ID     54321

// gcc -o mytraceroute mytraceroute_19CS10044.c
// sudo ./mytraceroute www.iitkgp.ac.in

void generate_random_payload(char *);
void hostname_to_ip(char *, char *);
unsigned short calculate_checksum(unsigned short *, int);


void generate_random_payload(char *payload)
{
    for (int i = 0; i < PAYLOAD_SIZE; i++)
    {
        payload[i] = rand() % 256;
    }
    payload[PAYLOAD_SIZE-1] = '\0';
}

void hostname_to_ip(char *hostname, char *ip)
{
    struct hostent *he = gethostbyname(hostname);
    if (he == NULL) 
    {  
       herror("Error in gethostbyname function.");
       exit(EXIT_FAILURE); 
    }
    else
    {
        int i=0;
        struct in_addr **addr_list = (struct in_addr **)he->h_addr_list;

        while(addr_list[i]!=0)
        {
            char *IP = inet_ntoa(*addr_list[i]);
            i++;
        }

        strcpy(ip, inet_ntoa(*addr_list[0]));
    }

}

unsigned short calculate_checksum(unsigned short *buf, int nwords)
{
    unsigned long sum;
    for (sum = 0; nwords > 0; nwords--)
    {
        sum += *buf++;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}


int main(int argv, char *argc[])
{
    srand(time(0));

    if(argv < 2)
    {
        printf("ERROR: Too few arguments. Command should be of the form --- mytraceroute <destination domain name> \n");
        exit(EXIT_FAILURE); 
    }

    if(argv > 2)
    {
        printf("ERROR: Too many arguments. Command should be of the form --- mytraceroute <destination domain name> \n");
        exit(EXIT_FAILURE); 
    }

    // Get destination IP
    char dest_IP[IP_ADDRESS_SIZE];
    memset(&dest_IP, 0, sizeof(dest_IP));
    hostname_to_ip(argc[1], dest_IP);
    
    // Create and bind sockets
    int sockfd_udp = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (sockfd_udp < 0)
    {
        perror("UDP Socket Creation Failed.");
        exit(EXIT_FAILURE); 
    }

    int sockfd_icmp = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd_icmp < 0)
    {
        perror("ICMP Socket Creation Failed.");
        close(sockfd_udp);
        exit(EXIT_FAILURE); 
    }

    // Source Information
    struct sockaddr_in src_addr_raw;
    socklen_t src_addr_raw_len;

    u_int16_t src_port              = SOURCE_PORT;
    src_addr_raw.sin_family         = AF_INET;
    src_addr_raw.sin_port           = htons(src_port);
    src_addr_raw.sin_addr.s_addr    = INADDR_ANY;
    src_addr_raw_len                = sizeof(src_addr_raw);  

    if(bind(sockfd_udp, (struct sockaddr*)&src_addr_raw, src_addr_raw_len)<0)
    {
        perror("UDP Socket bind failed.");
        close(sockfd_udp);
        close(sockfd_icmp);
        exit(EXIT_FAILURE);
    }

    // Set socket option
    const int on = 1;
    if (setsockopt(sockfd_udp, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0)
    {
        perror("Setting UDP Socket Option Failed.");
        close(sockfd_udp);
        close(sockfd_icmp);
        exit(EXIT_FAILURE);
    }

    // Destination Information
    struct sockaddr_in dest_addr;

    u_int16_t dest_port             = DESTINATION_PORT;
    u_int32_t dest_IP_addr          = inet_addr(dest_IP);
    dest_addr.sin_family            = AF_INET;
    dest_addr.sin_port              = htons(dest_port);
    dest_addr.sin_addr.s_addr       = dest_IP_addr;

    int TTL = 1;

    // Create a file descriptor set named rset
    fd_set rset;

    // Create a timeval structure named timeout to store the number of seconds to wait before timeout
    struct timeval timeout;
    memset(&timeout, 0, sizeof(timeout));
    timeout.tv_sec = TIMEOUT;

    // Get maxfd
    int maxfd = sockfd_icmp+1; 

    // nready is 0 incase of timeout 
    // nready>0 in case of error-free communication
    // nready<0 in case of some error in communication
    int nready;  

    // Tracks whether an ICMP packet is received or not
    int send_flag = 1;    

    // Tracks number of times a packet with the same TTL is sent
    int times_sent = 0;

    clock_t start_time, end_time;

    printf("mytraceroute to %s (%s), %d hops max, %d bytes payload size\n", argc[1], dest_IP, MAX_HOP, PAYLOAD_SIZE);
    
    while(1)
    {
        if(TTL > MAX_HOP)
        {
            break;
        }

        // datagram contains IP HEADER | UDP HEADER | UDP DATA
        char datagram[sizeof(struct iphdr) + sizeof(struct udphdr) + PAYLOAD_SIZE];

        // IP Header
        struct iphdr* IP_Header = (struct iphdr*) datagram;

        // UDP Header starts from datagram+sizeof(struct iphdr)
        struct udphdr* UDP_Header = (struct udphdr*)(datagram + sizeof(struct iphdr));

        // Data Part
        char* data = datagram + sizeof(struct iphdr) + sizeof(struct udphdr);
        

        if(send_flag == 1)
        {
            memset (datagram, 0, sizeof(datagram));
            generate_random_payload(data);

            // Fill in the IP Header
            IP_Header->ihl          = IP_HEADER_LENGTH;
            IP_Header->version      = IPV4_VERSION_NUMBER;
            IP_Header->tos          = ROUTINE_NORMAL_SERVICE;
            IP_Header->tot_len      = sizeof (struct iphdr) + sizeof (struct udphdr) + PAYLOAD_SIZE;
            IP_Header->id           = htonl(IP_PACKET_IDENTIFICATION_ID);	            
            IP_Header->frag_off     = 0;
            IP_Header->ttl          = TTL;
            IP_Header->protocol     = IPPROTO_UDP;
            IP_Header->check        = 0;		                
            IP_Header->saddr        = 0;
            IP_Header->daddr        = dest_IP_addr;

            // Fill in the UDP Header
            UDP_Header->source      = htons(src_port);
            UDP_Header->dest        = htons(dest_port);
            UDP_Header->len         = htons(sizeof(struct udphdr) + PAYLOAD_SIZE);

            // IP checksum
            IP_Header->check        = calculate_checksum((unsigned short *) datagram, sizeof(struct iphdr));
            
            // Send the packet
            if(sendto(sockfd_udp, datagram, IP_Header->tot_len, 0, (const struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0)
            {
                perror("sendto");
                close(sockfd_udp);
                close(sockfd_icmp);
                exit(EXIT_FAILURE);
            }

            times_sent++;
            start_time = clock();
            
        }

        // Clear the file descriptor set
        FD_ZERO(&rset);

        // Set sockfd_icmp in file descriptor set
        FD_SET(sockfd_icmp, &rset);
        
        // Select the ready descriptor
        nready = select(maxfd, &rset, NULL, NULL, &timeout);

        if(nready<0)
        {
            perror("Select failed.");
            close(sockfd_udp);
            close(sockfd_icmp);
            exit(EXIT_FAILURE);
        }
        else if(nready>0)
        {

            if(FD_ISSET(sockfd_icmp, &rset))
            {

                char ICMP_message[ICMP_MESSAGE_SIZE];
                memset(ICMP_message, 0, sizeof(ICMP_message));

                struct sockaddr_in router_addr;
                socklen_t router_addr_len = sizeof(router_addr);
                int message_len = recvfrom(sockfd_icmp, ICMP_message, ICMP_MESSAGE_SIZE, 0, (struct sockaddr*)&router_addr, &router_addr_len);

                end_time = clock();

                if(message_len <= 0)
                {
                    timeout.tv_sec = TIMEOUT;
                    send_flag = 1; // Repeat packet with same TTL
                    continue;
                }

                struct iphdr ICMP_message_IP_Header;
                memset(&ICMP_message_IP_Header, 0, sizeof(struct iphdr));
                memcpy(&ICMP_message_IP_Header, ICMP_message, sizeof(struct iphdr));

                if(ICMP_message_IP_Header.protocol == 1)
                {
                    struct icmphdr ICMP_message_ICMP_Header;
                    memset(&ICMP_message_ICMP_Header, 0, sizeof(struct icmphdr));
                    memcpy(&ICMP_message_ICMP_Header, ICMP_message+sizeof(struct iphdr), sizeof(struct icmphdr));
                    
                    struct in_addr router_ip;
                    router_ip.s_addr = ICMP_message_IP_Header.saddr;
                    if(ICMP_message_ICMP_Header.type == 3 && ICMP_message_IP_Header.saddr == dest_IP_addr)
                    {
                        // ICMP Destination Unreachable Message
                        // If type = 3, then it is a ICMP Destination Unreachable
                        // Message), then you have reached to the target destination, and received the
                        // response from there (to be safe, check that the source IP address of the ICMP
                        // Destination Unreachable Message matches with your target server IP address). 
                        
                        printf("Hop_Count(%d)\t\t%s\t%.3f ms\t\t Destination Reached : %s\n", TTL, inet_ntoa(router_ip), (float)(end_time - start_time) / CLOCKS_PER_SEC * 1000, dest_IP);
                        close(sockfd_udp);
                        close(sockfd_icmp);
                        return 0;
                    }
                    else if(ICMP_message_ICMP_Header.type == 11)
                    {
                        // ICMP Time Exceeded Message (check the type field in the ICMP
                        // header. If type = 11, then it is an ICMP Time Exceeded Message), then you
                        // have got the response from the Layer 3 device at an intermediate hop specified
                        // by the TTL value

                        printf("Hop_Count(%d)\t\t%s\t%.3f ms\n", TTL, inet_ntoa(router_ip), (float)(end_time - start_time) / CLOCKS_PER_SEC * 1000);
                        TTL++;
                        timeout.tv_sec = TIMEOUT;
                        times_sent = 0;
                        send_flag = 1; // Send packet with increased TTL
                        continue;

                    }
                    else
                    {
                        // received some other ICMP packet. It is a spurious packet not
                        // related to you. Ignore and go back to wait on the select call (Step 5) with the
                        // REMAINING value of timeout.
                        
                        send_flag = 0; // Wait for the remaining time
                        timeout.tv_sec -= (end_time - start_time);
                        continue;
                    }
                    
                }
                else
                {
                    // received some other ICMP packet. It is a spurious packet not
                    // related to you. Ignore and go back to wait on the select call (Step 5) with the
                    // REMAINING value of timeout.
                    
                    send_flag = 0; // Wait for the remaining time
                    timeout.tv_sec -= (end_time - start_time);
                    continue;

                    
                }

            }
        }

        // select call times out, repeat from step 4 again. Total number of repeats with the same TTL is 3.
        else
        {
            if(times_sent >= MAX_REPEAT)
            {
                printf("Hop_Count(%d)\t\t*\t*\t*\t*\n", TTL);
                times_sent = 0;
                TTL++;
            }
            timeout.tv_sec = TIMEOUT;
            send_flag = 1; // Send or repeat packet with increased or same TTL respectively depending on times_sent
            continue;
        }
    }

    close(sockfd_icmp);
    close(sockfd_udp);

    return 0;
}