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
#include <netinet/tcp.h>
#include <sys/time.h>

#define MAXDATASIZE (8*1024*1024) // allocate buffer (set to 8 MB)
#define FILESIZE (5*1024*1024)    // size of incoming file (set to 5 MB)
#define PORT "1337"

// we may be unable to receive full file in one go due to MTU
// See https://beej.us/guide/bgnet/html/#sendall

// A function to receive the incoming file from the server.
// Params:
//      sockfd
//          The socket descriptor for the socket on which data will be received.
//      buf
//          The buffer to be used for received data.
// Returns:
//      The number of bytes received if received successfully.
int recv_full_file(int sockfd, char* buf)
{
    int bytes_recvd = 0, diff = 0;
    // The following loop runs until the entire file has been received by the receiving socket.
    while (bytes_recvd < FILESIZE) {
        // Receiving a packet of data and storing it in the buffer.
    	diff = recv(sockfd, buf+bytes_recvd, MAXDATASIZE-bytes_recvd-1, 0);
        // Break loop if receiving is unsuccessful.
    	if (diff == -1) { break; }
        // Updating the bytes received so far.
    	bytes_recvd += diff;
    }
    buf[bytes_recvd] = '\0';

    // Return the number of bytes received only if all packets have been received successfully, else return -1.
    return diff == -1 ? -1 : bytes_recvd;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "usage: ./client <reno|cubic> <path-to-file>\n");
        exit(1);
    }

    int sockfd; // connect using sockfd
    FILE *file = fopen(argv[2], "w"); // open file to be sent
    // handling file opening errors
    if (file == NULL) {
        fprintf(stderr, "Error opening file!\n");
        exit(1);
    }

    //------------------------------ RECEIVER SETUP ------------------------------// 
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // use IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // use TCP
    hints.ai_flags = AI_PASSIVE; // use my IP
    
    // fill up struct servinfo
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
		p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        
        // set TCP variant to be used based on argv[1]
        // must ensure that only one of the available variants is selected
        if (setsockopt(sockfd, IPPROTO_TCP, TCP_CONGESTION,
		argv[1], sizeof argv[1]) == -1) {
            fprintf(stderr, "Choose from available TCP variants\n");
            perror("setsockopt");
            exit(1);
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: bind");
            continue;
        }
        break;
    }

    // client binding errors
    if (p == NULL) {
        fprintf(stderr, "client: failed to bind\n");
        exit(1);
    }
    //--------------------------- RECEIVER SETUP DONE ----------------------------// 

    // allocating the buffer to store received data
    char* buf = malloc(MAXDATASIZE*sizeof(char));
    // timeval objects to measure time taken for receiving data, so that throughput can be computed using this data
    struct timeval stop, start;

    freeaddrinfo(servinfo); // free data in servinfo

    // start time of receiving
    gettimeofday(&start, NULL);

    // receives a file over the connection
    if (recv_full_file(sockfd, buf) == -1)
        perror("send");
    
    // end time of receiving
    gettimeofday(&stop, NULL);

    // calculation of time taken from beginning of receiving till the end
    printf("%.6f\n", stop.tv_sec-start.tv_sec + (stop.tv_usec-start.tv_usec)/1000000.0);

    fprintf(file, "%s", buf);

    // close files
    close(sockfd);
    fclose(file);
    free(buf);
    return 0;
}
