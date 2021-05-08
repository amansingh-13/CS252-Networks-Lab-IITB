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

#define MAXDATASIZE (8 * 1024 * 1024) // allocate 8 MB of buffer
#define FILESIZE (5 * 1024 * 1024)    // size of incoming file 5 MB
#define PORT "1338"

// -------- I hardcoded 5 MB, can't think of a better way -------- //

// we may be unable to receive full file in one go due to MTU
// See https://beej.us/guide/bgnet/html/#sendall
int recv_full_file(int sockfd, char *buf)
{
    int bytes_recvd = 0, diff = 0;
    while (bytes_recvd < FILESIZE)
    {
        diff = recv(sockfd, buf + bytes_recvd, MAXDATASIZE - bytes_recvd - 1, 0);
        if (diff == -1)
            break;
        bytes_recvd += diff;
    }
    buf[bytes_recvd] = '\0';
    return diff == -1 ? -1 : bytes_recvd;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: ./client <reno|cubic> <path-to-file>\n");
        exit(1);
    }

    int sockfd;                       // connect using sockfd
    FILE *file = fopen(argv[2], "w"); // open file to be sent
    if (file == NULL)
    {
        fprintf(stderr, "Error opening file!\n");
        exit(1);
    }

    //------------------------------ RECEIVER SETUP ------------------------------//
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     // use IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // use TCP
    hints.ai_flags = AI_PASSIVE;     // use my IP

    // fill up struct servinfo
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("client: socket");
            continue;
        }

        // ------ do we need to set this in client too (as TCP is duplex) ? ------//

        // set TCP variant to be used based on argv[1]
        if (setsockopt(sockfd, IPPROTO_TCP, TCP_CONGESTION,
                       argv[1], sizeof argv[1]) == -1)
        {
            fprintf(stderr, "Choose from available TCP variants\n");
            perror("setsockopt");
            exit(1);
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("client: bind");
            continue;
        }
        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "client: failed to bind\n");
        exit(1);
    }
    //--------------------------- RECEIVER SETUP DONE ----------------------------//

    char *buf = malloc(MAXDATASIZE * sizeof(char));
    struct timeval stop, start;

    freeaddrinfo(servinfo); // free data in servinfo

    gettimeofday(&start, NULL);
    // receives a file over the connection
    int fileSize = recv_full_file(sockfd, buf); //fileSize in bytes
    if (fileSize == -1)
        perror("send");
    gettimeofday(&stop, NULL);

    float transferTime = stop.tv_sec - start.tv_sec + (stop.tv_usec - start.tv_usec) / 1000000.0; // in seconds
    printf("%.6f\n", ((float)fileSize * 8) / transferTime);                                       // in bits per second

    fprintf(file, "%s", buf);

    // close files
    close(sockfd);
    fclose(file);
    free(buf);
    return 0;
}
