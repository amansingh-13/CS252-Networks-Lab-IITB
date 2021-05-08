// Resource used : https://beej.us/guide/bgnet/html/

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
#include <sys/sendfile.h>

#define BACKLOG 10 // maximum pending connections
#define PORT "1338"

// we may be unable to send full file in one go due to MTU
// See https://beej.us/guide/bgnet/html/#sendall


// A function to receive the incoming file from the server.
// Params:
//      connfd
//          File descriptor of socket connection to the client
//      filefd
//          File descriptor of file to be sent to the client
// Returns:
//      -1 if there is an error, 0 if everything is successful
int send_full_file(int connfd, int filefd)
{
    // store size of file in `bytesleft`
    int bytesleft = lseek(filefd, 0, SEEK_END);
    lseek(filefd, 0, SEEK_SET);
    int size = bytesleft, diff = 0;

    off_t offset = 0;
    while (bytesleft > 0)
    {
        diff = sendfile(connfd, filefd, &offset, bytesleft);
        // exit if there is error
        if (diff == -1) break;
        bytesleft -= diff;
    }

    return diff == -1 ? -1 : 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: ./server <reno|cubic> <path-to-file>\n");
        exit(1);
    }

    int sockfd, connfd;               // listen on sockfd, send on connfd
    FILE *file = fopen(argv[2], "r"); // open file to be sent
    if (file == NULL)
    {
        fprintf(stderr, "Error opening file!\n");
        exit(1);
    }

    //------------------------------ SENDER SETUP ------------------------------//
    struct addrinfo hints, *servinfo, *p;
    int rv, yes = 1;

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
        // try creating a socket with addrinfo `p`
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }
        // allow bind to local addresses
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                       &yes, sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }
        // set TCP variant to be used based on argv[1]
        if (setsockopt(sockfd, IPPROTO_TCP, TCP_CONGESTION,
                       argv[1], sizeof argv[1]) == -1)
        {
            fprintf(stderr, "Choose from available TCP variants\n");
            perror("setsockopt");
            exit(1);
        }
        // bind sockfd to addrinfo `p`
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo); // free servinfo as it not needed anymore

    // exit if we were not able to bind
    if (p == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    // start listening on sockfd
    if (listen(sockfd, BACKLOG) == -1)
    {
        perror("listen");
        exit(1);
    }

    //--------------------------- SENDER SETUP DONE ----------------------------//

    struct sockaddr_storage their_addr; // client's address information
    socklen_t sin_size = sizeof their_addr; 

    // accepts connection from client, sends file, waits for another connection 
    while (1)
    {
        // accepts connection from client 
        connfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (connfd == -1)
        {
            perror("accept");
            continue;
        }

        // sends file provided in argv[2] over the connection
        // fileno() returns the file descriptor corresponding to the `file` struct
        if (send_full_file(connfd, fileno(file)) == -1)
        {
            perror("send");
            continue;
        }

        // close connection to client
        close(connfd);
    }

    // close files
    close(sockfd);
    fclose(file);
    return 0;
}
