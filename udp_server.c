// Alexei Doell cka067 11345642

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <shared.h>

/* takes a port as param and does all the udp connection boiler plate syscalls */
int bind_to(char* port) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        fprintf(stderr, "receiver: failed on port %s, please\
                try another port\n", port);
        return -1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("receiver: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("receiver: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "receiver: failed to bind socket on port %s, please "
                "try another\n", port);
        return -1;
    }

    return sockfd;
}

int main(int argc, char* argv[])
{

    int numbytes;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    char msg[MAXLEN];
    // argv is the port given to bind to by the user
    int sockfd = bind_to(argv[1]);

    if (sockfd == -1) {
        fprintf(stderr, "receiver: failed to bind, closing receiver\n");
        return -1;
    }

    printf("receiver: waiting to recvfrom on %s\n", argv[1]);

    addr_len = sizeof their_addr;


    while (1) {
        numbytes = recvfrom(sockfd, msg, MAXLEN, 0,
                (struct sockaddr*)&their_addr, &addr_len);
        switch (numbytes) {
            case -1:
                perror("recvfrom");
                close(sockfd);
                exit(1);
            default:
                cipher(msg, numbytes);
                if (sendto(sockfd, msg, numbytes, 0,
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
