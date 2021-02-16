/*
 * TO_CONSIDER :
 * [ ] how much logging is required ?
 * [ ] dynamic extra waiting if ACK received is wrong ?
 * [ ] implement creating and writing to sender.txt, receiver.txt
 * [ ] Why exit() in receiveACK and sendMessage? Can we handle the error instead ?
 */ 

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
#include <poll.h>
#include <time.h>
time_t mytime;

#define MAXBUFLEN 100

// idea : https://gist.github.com/mokumus/bdd9d4fa837345f01b35e0cd03d67f35
#define printf(...) mytime = time(NULL), printf("[%02d:%02d:%02d] ",\
		localtime(&mytime)->tm_hour, localtime(&mytime)->tm_min,\
		localtime(&mytime)->tm_sec),\
		printf(__VA_ARGS__)

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{    
    if (sa->sa_family == AF_INET) {        
        return &(((struct sockaddr_in*)sa)->sin_addr);    
    }    
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int sendMessage(int SEQ_NO, int sockfd, struct addrinfo* p)
{
    int dup = SEQ_NO;
    int count = 0;
    while (dup != 0) {
        dup /= 10; 
        ++count;
    }
    if(SEQ_NO==0) count=1; // END message case

    char* MESSAGE = (char*) malloc((7+count+1)*sizeof(char));
    snprintf(MESSAGE, 7+count+1, "Packet:%d", SEQ_NO);

    int numbytes;
    if ((numbytes = sendto(sockfd, MESSAGE, strlen(MESSAGE), 0,
                           p->ai_addr, p->ai_addrlen)) == -1)
    {
        perror("sender: sendto");
        exit(1); 
    }
    
    // hardcoded 127.0.0.1
    printf("sender: sent PACKET: %d (%d bytes) to 127.0.0.1\n", SEQ_NO, numbytes);

    return numbytes;
}

int receiveACK(int sockrcv)
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

    printf("sender: got PACKET from %s\n",        
            inet_ntop(their_addr_rcv.ss_family,            
            get_in_addr((struct sockaddr *)&their_addr_rcv),            
            s, sizeof s));    
    printf("sender: PACKET is %d bytes long\n", numbytes_rcv);    
    
    buf[numbytes_rcv] = '\0';
    printf("sender: PACKET contains \"%s\"\n", buf);

    return atoi(&buf[16]);
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        fprintf(stderr, "usage: sender <SenderPort> <ReceiverPort> <RetransmissionTimer> <NoOfPacketsToBeSent>\n");
        exit(1);
    }
  
      //////////////////////
     // Argument mapping //
    //////////////////////
    char* SENDER_PORT           =   argv[1];
    char* RECEIVER_PORT         =   argv[2];
    int RETRANSMISSION_TIMER    =   atoi(argv[3]);
    int NO_OF_PACKETS_TO_SEND   =   atoi(argv[4]);
    
      //////////////////////////////
     // Sender utility variables //
    //////////////////////////////
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    char* HOSTNAME = "localhost"; // or "127.0.0.1"

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

      ////////////////////////
     // Setup Sender Stuff //
    ////////////////////////
    if ((rv = getaddrinfo(HOSTNAME, SENDER_PORT, &hints, &servinfo)) != 0)
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
        fprintf(stderr, "sender: failed to bind socket\n");
        return 2;
    }
    else
        printf("sender: found and bound a socket\n\n");
    
      ////////////////////////////
     // ACK Receiver variables //
    ////////////////////////////
    int sockrcv;    
    struct addrinfo hints_rcv, *servinfo_rcv, *p_rcv;    
    int rv_rcv;    

      //////////////////////////////
     // ACK Receiver Setup stuff //
    //////////////////////////////
    memset(&hints_rcv, 0, sizeof hints_rcv);    
    hints_rcv.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4    
    hints_rcv.ai_socktype = SOCK_DGRAM;    
    hints_rcv.ai_flags = AI_PASSIVE; // use my IP    

    if ((rv_rcv = getaddrinfo(NULL, RECEIVER_PORT, &hints_rcv, &servinfo_rcv)) != 0) {        
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv_rcv));        
        return 1;    
    }    
    
    // Get the first socket we can bind to!   
    for(p_rcv = servinfo_rcv; p_rcv != NULL; p_rcv = p->ai_next) {        
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
    freeaddrinfo(servinfo);

      ///////////////////////////
     // All Socket setup done //
    ///////////////////////////

    int SEQ_NO = 1;

    struct pollfd pfds[1];
    pfds[0].fd = sockrcv;
    pfds[0].events = POLLIN;

    while(SEQ_NO <= NO_OF_PACKETS_TO_SEND)
    {
	      ////////////////////
	     //  Send Message  //
	    ////////////////////
	    sendMessage(SEQ_NO, sockfd, p);

	    int poll_count = poll(pfds, 1, RETRANSMISSION_TIMER);
    	    printf("sender: waiting to recvfrom ACK...\n");

	    if(poll_count == -1) {
			perror("poll");
			exit(1);
	    }
	    else if (poll_count == 0) 
		    printf("sender: PACKET: %d's retransmission timer expired !!\n", SEQ_NO);
	    else {
		      /////////////////////////////
                     // ACK Receiving component //
                    /////////////////////////////
		    int pollin_happened = pfds[0].revents & POLLIN;
		    if(!pollin_happened) {
		    	printf("sender: Unexpected event: %d\n", pfds[0].revents);
			continue;
		    }

		    int ACK_SEQ_NO = receiveACK(sockrcv);

		    if(ACK_SEQ_NO == SEQ_NO+1) {
		    	SEQ_NO++;
		        printf("sender: Expected ACK received (Acknowledgement: %d)\n\n",
					ACK_SEQ_NO);
		    } else {
			// Point 2 in TO_CONSIDER
		        printf("sender: Wrong ACK received (Acknowledgement: %d), expecting %d\n",
					ACK_SEQ_NO, SEQ_NO);
		    }
	    }
    }

    printf("sender: Sending END message and quitting!\n");
    sendMessage(0, sockfd, p);

      ///////////////////
     // Close Sockets //
    ///////////////////
    close(sockrcv); 
    close(sockfd);

    return 0;
}

