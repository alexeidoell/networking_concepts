// Alexei Doell cka067 11345642

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define PORT "34920" // the port client will be connecting to 

#define MAXDATASIZE 100 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    int sockfd;
    int32_t num_bytes;
    ssize_t recvstatus;
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    int return_code = 0;

    if (argc != 2) {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    char* line = NULL;
    int32_t len = 0;
    size_t alloc = 0;

    while ((len = getline(&line, &alloc, stdin)) != EOF) {
        if (line[0] == '\n') {
            printf("client: closing program\n");
            goto cleanup;
        }
        // send length of message first
        int32_t networklen = htonl(len);
        if (send(sockfd, &networklen, sizeof networklen, 0) == -1) {
            perror("send: length");
            goto cleanup;
        }
        if (send(sockfd, line, len, 0) == -1) {
            perror("send: buffer");
            goto cleanup;
        }
        recvstatus = recv(sockfd, &len, sizeof len, 0);
        switch (recvstatus) {
            case -1:
                perror("recv");
                return_code = 1;
                goto cleanup;
            case 0:
                printf("client: connection closed from server\n");
                goto cleanup;
        }
        len = ntohl(len);
        num_bytes = recv(sockfd, buf, len, 0);
        switch (num_bytes) {
            case -1:
                perror("recv");
                return_code = 1;
                goto cleanup;
            case 0:
                printf("client: connection closed from server\n");
                goto cleanup;
            default:
                buf[num_bytes] = '\0';
                printf("%s", buf);
                free(line);
                line = NULL;
        }
    }

cleanup:

    free(line);
    close(sockfd);

    return return_code;
}
