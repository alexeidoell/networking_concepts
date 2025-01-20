// Alexei Doell cka067 11345642

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
#include <shared.h>

#define MYPORT "34314"    // the port users will be connecting to

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("udp server: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("udp server: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "udp server: failed to bind socket\n");
        return 2;
    }


    printf("udp server: waiting to recvfrom on %s\n", MYPORT);

    addr_len = sizeof their_addr;

    char* msg = NULL;
    int32_t expected = 0;
    ssize_t recvstatus;

    while (1) {
        recvstatus = recvfrom(sockfd, &expected, sizeof expected, 0,
                (struct sockaddr*)&their_addr, &addr_len);
        expected = ntohl(expected);
        switch (recvstatus) {
            case -1:
                perror("recv");
                close(sockfd);
                exit(1);
        }
        if (!(msg = realloc(msg, expected))) {
            perror("realloc");
            close(sockfd);
            exit(1);
        }
        numbytes = recvfrom(sockfd, msg, expected, 0,
                (struct sockaddr*)&their_addr, &addr_len);
        int32_t networkbytes = htonl(expected);
        switch (numbytes) {
            case -1:
                perror("recv");
                close(sockfd);
                exit(1);
            default:
                cipher(msg, expected);
                if (sendto(sockfd, &networkbytes, sizeof networkbytes, 0,
                                (struct sockaddr*)&their_addr, addr_len) == -1) {
                    perror("send");
                    close(sockfd);
                    exit(1);
                }
                if (sendto(sockfd, msg, expected, 0,
                                (struct sockaddr*)&their_addr, addr_len) == -1) {
                    perror("send");
                    close(sockfd);
                    exit(1);
                }
        }
    }


    close(sockfd);

    return 0;
}
