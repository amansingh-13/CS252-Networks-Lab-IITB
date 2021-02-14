#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAXBUFLEN 100

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa){    
    if (sa->sa_family == AF_INET) {        
        return &(((struct sockaddr_in*)sa)->sin_addr);    
    }    
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int sendACK(int SEQ_NO, int sockfd, struct addrinfo* p){
    int dup=SEQ_NO;
    int numbytes;
    int count=0;
    while (dup != 0) {
        dup /= 10;
        ++count;
    }
    char* MESSAGE = (char*)malloc((17+count)*sizeof(char));
    snprintf(MESSAGE, 17+count,"Acknowledgement:%d",SEQ_NO); 
    if ((numbytes = sendto(sockfd, MESSAGE, strlen(MESSAGE), 0,
                           p->ai_addr, p->ai_addrlen)) == -1)
    {
        perror("receiver: sendto");
        exit(1);
    }

    return numbytes;
}   

int receivePacket(struct sockaddr_storage their_addr_rcv,
                int sockrcv){
    printf("receiver: waiting to recvfrom...\n");    
    
    char buf[MAXBUFLEN]; 
    char s[INET6_ADDRSTRLEN];  
    socklen_t addr_len_rcv = sizeof their_addr_rcv; 
    int numbytes_rcv;   
    if ((numbytes_rcv = recvfrom(sockrcv, buf, MAXBUFLEN-1 , 0,        
            (struct sockaddr *)&their_addr_rcv, &addr_len_rcv)) == -1) {        
        perror("recvfrom");        
        exit(1);
    }    

    printf("receiver: got packet from %s\n",        
            inet_ntop(their_addr_rcv.ss_family,            
            get_in_addr((struct sockaddr *)&their_addr_rcv),            
            s, sizeof s));    
    printf("receiver: packet is %d bytes long\n", numbytes_rcv);    
    
    buf[numbytes_rcv] = '\0';    
    printf("receiver: packet contains \"%s\"\n", buf);

    return atoi(&buf[7]);
}

int main(int argc, char* argv[]){    
    if(argc != 4){
        fprintf(stderr, "usage: receiver <ReceiverPort> <SenderPort> <PacketDropProbability>\n");
        exit(1);
    }

    /////////////////////
    // Argument mapping//
    /////////////////////
    char* RECEIVER_PORT     =   argv[1];
    char* SENDER_PORT       =   argv[2];
    int DROP_PROBABILITY    =   atoi(argv[3]);

    ///////////////////////////////
    //Receiver utility variables //
    ///////////////////////////////
    int sockrcv;    
    struct addrinfo hints_rcv, *servinfo_rcv, *p_rcv;    
    int rv_rcv;    
    struct sockaddr_storage their_addr_rcv;  

    int EXPECTED_SEQ_NO=1;

    /////////////////////////
    // Setup Receiver Stuff//
    /////////////////////////
    memset(&hints_rcv, 0, sizeof hints_rcv);    
    hints_rcv.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4    
    hints_rcv.ai_socktype = SOCK_DGRAM;    
    hints_rcv.ai_flags = AI_PASSIVE; // use my IP    

    if ((rv_rcv = getaddrinfo(NULL, SENDER_PORT, &hints_rcv, &servinfo_rcv)) != 0) {        
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv_rcv));        
        return 1;    
    }    
    
    
    // Get the first socket we can bind to!
    for(p_rcv = servinfo_rcv; p_rcv != NULL; p_rcv = p_rcv->ai_next) {        
        if ((sockrcv = socket(p_rcv->ai_family, p_rcv->ai_socktype,                
                p_rcv->ai_protocol)) == -1) {            
            perror("sender: receiving socket error");            
            continue;        
        }        
        if (bind(sockrcv, p_rcv->ai_addr, p_rcv->ai_addrlen) == -1) {            
            close(sockrcv);            
            perror("sender: receiving bind error");            
            continue;        
        }        
        break;    
    } 

    if (p_rcv == NULL) {        
        fprintf(stderr, "sender: failed to bind socket for ACK\n");        
        return 2;    
    }    
    freeaddrinfo(servinfo_rcv);
    //////////////////////////////////////////////////

    ////////////////////////
    //ACK Sender variables//
    ////////////////////////
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    char* HOSTNAME = "localhost"; // or "127.0.0.1"

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    ///////////////////////////
    // ACK Sender Setup stuff//
    ///////////////////////////
    if ((rv = getaddrinfo(HOSTNAME, RECEIVER_PORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // Get the first socket we can bind to!
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("can't find: socket");
            continue;
        }
        break;
    }

    if (p == NULL){
        fprintf(stderr, "receiver: failed to bind socket\n");
        return 2;
    }
    else
        printf("receiver: found and bound a socket\n");
    //////////////////////////////////////////////////

    /////////////////////////
    //All Socket setup done//
    /////////////////////////


    //////////////////////////////
    //Packet Receiving component//
    //////////////////////////////
    int PACKET_NO = receivePacket(their_addr_rcv,sockrcv);
    printf("Packet received: %d\n",PACKET_NO);
    ///////////////////////////////////
    //Packet Receiving component ends//
    ///////////////////////////////////

    //////////////////////////
    // Build ACK for Sending//
    //////////////////////////
    int numbytes = sendACK(EXPECTED_SEQ_NO, sockfd, p);        
    printf("receiver: sent %d bytes to %s\n", numbytes, HOSTNAME);
    freeaddrinfo(servinfo);
    ///////////////////////////
    // ACK sending setup ends//
    ///////////////////////////

    //////////////////
    // Close Sockets//
    //////////////////
    close(sockrcv);
    close(sockfd); 

    return 0;
}