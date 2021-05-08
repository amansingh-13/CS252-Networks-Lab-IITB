// [_] Introduce a quiet mode so that this doesn't print anything to the stdout.
// [_] Introduce a non quiet mode which prints all useful info.

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

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

// we may be unable to send full file in one go due to MTU
// See https://beej.us/guide/bgnet/html/#sendall
int send_full_file(int connfd, int filefd)
{
    // get size of file
    int bytesleft = lseek(filefd, 0, SEEK_END);
    lseek(filefd, 0, SEEK_SET);
    int size = bytesleft, diff = 0;

    off_t offset = 0;
    while (bytesleft > 0)
    {
        diff = sendfile(connfd, filefd, &offset, bytesleft);
        if (diff == -1)
        {
            break;
        }
        bytesleft -= diff;
    }
    // printf("server: sent %d bytes\n", size-bytesleft);

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
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo); // free data in servinfo

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
    // printf("server: waiting for connections...\n");

    //--------------------------- SENDER SETUP DONE ----------------------------//

    struct sockaddr_storage their_addr; // client's address information
    socklen_t sin_size = sizeof their_addr;
    char s[INET6_ADDRSTRLEN];

    // accept loop
    while (1)
    {
        connfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (connfd == -1)
        {
            perror("accept");
            continue;
        }
        // convert client's address to string for printing
        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        // printf("server: got connection from %s\n", s);

        // sends file provided in argv[2] over the connection
        if (send_full_file(connfd, fileno(file)) == -1)
        {
            perror("send");
            continue;
        }

        close(connfd);
    }

    // close files (but we never reach here)
    close(sockfd);
    fclose(file);
    return 0;
}
