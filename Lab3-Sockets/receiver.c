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
#include <time.h>
time_t mytime;

#define MAXBUFLEN 100

// idea : https://gist.github.com/mokumus/bdd9d4fa837345f01b35e0cd03d67f35
#define printf(...) mytime = time(NULL), printf("[%02d:%02d:%02d] ",\
		localtime(&mytime)->tm_hour, localtime(&mytime)->tm_min,\
		localtime(&mytime)->tm_sec),\
		printf(__VA_ARGS__)

#define fprintf(f_, ...) mytime = time(NULL), fprintf((f_), "[%02d:%02d:%02d] ", localtime(&mytime)->tm_hour, localtime(&mytime)->tm_min, localtime(&mytime)->tm_sec),\
                    fprintf((f_), __VA_ARGS__)

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{    
    if (sa->sa_family == AF_INET) {        
        return &(((struct sockaddr_in*)sa)->sin_addr);    
    }    
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int sendACK(int SEQ_NO, int sockfd, struct addrinfo* p, FILE* file)
{
    int dup = SEQ_NO;
    int numbytes;
    int count = 0;
    while (dup != 0) {
        dup /= 10;
        ++count;
    }
    char* MESSAGE = (char*)malloc((16+count+1)*sizeof(char));
    snprintf(MESSAGE, 16+count+1, "Acknowledgement:%d", SEQ_NO); 
    if ((numbytes = sendto(sockfd, MESSAGE, strlen(MESSAGE), 0,
                           p->ai_addr, p->ai_addrlen)) == -1)
    {
        perror("receiver: sendto");
        exit(1);
    }

    // hardcoded 127.0.0.1
    printf("receiver: sent ACK: %d (%d bytes) to 127.0.0.1\n\n", SEQ_NO, numbytes);
    fprintf(file,"receiver: sent ACK: %d (%d bytes) to 127.0.0.1\n\n", SEQ_NO, numbytes);

    return numbytes;
}   

int receivePacket(int sockrcv, FILE* file)
{
    
    char buf[MAXBUFLEN]; 
    char s[INET6_ADDRSTRLEN];  
    struct sockaddr_storage their_addr_rcv;  
    socklen_t addr_len_rcv = sizeof their_addr_rcv; 
    int numbytes_rcv;   

    if ((numbytes_rcv = recvfrom(sockrcv, buf, MAXBUFLEN-1 , 0,        
            (struct sockaddr *)&their_addr_rcv, &addr_len_rcv)) == -1) {        
        perror("recvfrom");        
        exit(1);
    }    

    printf("receiver: got message from %s\n",        
            inet_ntop(their_addr_rcv.ss_family,            
            get_in_addr((struct sockaddr *)&their_addr_rcv),
            s, sizeof s));    
    fprintf(file,"receiver: got message from %s\n",        
            inet_ntop(their_addr_rcv.ss_family,            
            get_in_addr((struct sockaddr *)&their_addr_rcv),
            s, sizeof s));

    printf("receiver: message is %d bytes long\n", numbytes_rcv);    
    fprintf(file,"receiver: message is %d bytes long\n", numbytes_rcv);

    buf[numbytes_rcv] = '\0';    
    printf("receiver: message contains \"%s\"\n", buf);
    fprintf(file,"receiver: message contains \"%s\"\n", buf);

    return atoi(&buf[7]);
}

int main(int argc, char* argv[])
{    
    if(argc != 4){
        fprintf(stderr, "usage: receiver <ReceiverPort> <SenderPort> <PacketDropProbability>\n");
        exit(1);
    }

    FILE *file = fopen("receiver.txt", "w");
    if (file == NULL) // Might fail to create file due to directory permissions
    {
        fprintf(stderr, "Error opening file!\n");
        exit(1);
    }

      //////////////////////
     // Argument mapping //
    //////////////////////
    char* RECEIVER_PORT     =   argv[1];
    char* SENDER_PORT       =   argv[2];
    double DROP_PROBABILITY    =   atof(argv[3]);

      ////////////////////////////////
     // Receiver utility variables //
    ////////////////////////////////
    int sockrcv;    
    struct addrinfo hints_rcv, *servinfo_rcv, *p_rcv;    
    int rv_rcv;    

      //////////////////////////
     // Setup Receiver Stuff //
    //////////////////////////
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

      //////////////////////////
     // ACK Sender variables //
    //////////////////////////
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    char* HOSTNAME = "localhost"; // or "127.0.0.1"

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

      ////////////////////////////
     // ACK Sender Setup stuff //
    ////////////////////////////
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
    else{
        printf("receiver: found and bound a socket\n\n");
        fprintf(file,"receiver: found and bound a socket\n\n");
    }

    freeaddrinfo(servinfo);

    printf("sender: All socket setup done!\n\n");
    fprintf(file,"sender: All socket setup done!\n\n");
      ///////////////////////////
     // All Socket setup done //
    ///////////////////////////

    int EXPECTED_SEQ_NO = 1;
    srand(420);

    while(1)
    {
	      ////////////////////////////////
	     // Packet Receiving component //
	    ////////////////////////////////
        printf("receiver: waiting to recvfrom...\n"); 
        fprintf(file,"receiver: waiting to recvfrom...\n");

	    int PACKET_NO = receivePacket(sockrcv,file);

	    if(PACKET_NO == EXPECTED_SEQ_NO && rand()/((double)RAND_MAX) >= DROP_PROBABILITY)
	    {
            printf("receiver: Expected packet received (Packet: %d)\n", PACKET_NO);
            fprintf(file,"receiver: Expected packet received (Packet: %d)\n", PACKET_NO);
		      ///////////////////////////
		     // Build ACK for Sending //
		    ///////////////////////////
		    sendACK(++EXPECTED_SEQ_NO, sockfd, p, file);
	    } 
	    else if (PACKET_NO == 0)
	    {
		    printf("receiver: Recieved END message, exiting!\n");
            fprintf(file,"receiver: Recieved END message, exiting!\n");
		    break;

	    }
	    else if(PACKET_NO != EXPECTED_SEQ_NO)
	    { 
		    printf("receiver: Wrong packet received (Packet: %d), expecting %d\n", PACKET_NO, EXPECTED_SEQ_NO);
            fprintf(file,"receiver: Wrong packet received (Packet: %d), expecting %d\n", PACKET_NO, EXPECTED_SEQ_NO);
		    sendACK(EXPECTED_SEQ_NO, sockfd, p, file);
	    }
	    else
	    {
            printf("receiver: Expected packet received (Packet: %d)\n", PACKET_NO);
		    printf("receiver: Dropping Packet: %d\n", PACKET_NO);
            fprintf(file,"receiver: Expected packet received (Packet: %d)\n", PACKET_NO);
		    fprintf(file,"receiver: Dropping Packet: %d\n", PACKET_NO);
	    }
    }

      ///////////////////
     // Close Sockets //
    ///////////////////
    close(sockrcv);
    close(sockfd); 
    fclose(file);

    return 0;
}

