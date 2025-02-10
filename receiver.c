// Alexei Doell cka067 11345642

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>

#include <shared.h>



enum {
    ACK,
    ACK_DROPPED,
    MSG_DROPPED,
};

int ack_check(void) {
    char * input = NULL;
    size_t len = 0;
    int rv = ACK;
    printf("receiver: was the sender's message successfully received? (Y/N)\n");
    getline(&input, &len, stdin);
    if (input[0] != 'Y') {
        rv = MSG_DROPPED;
    } else {
        printf("receiver: was the ACK successfully sent back (Y/N)\n");
        getline(&input, &len, stdin);
        if (input[0] != 'Y') {
            rv = ACK_DROPPED;
        }
    }

    free(input);
    return rv;
}

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
    struct fake_packet msg;
    int sockfd;
    int ack_status;

    int current_packet = 0;

    if (argc != 2) {
        fprintf(stderr,"usage: receiver <port to bind on>\n");
        exit(1);
    }

    // argv is the port given to bind to by the user
    sockfd = bind_to(argv[1]);
    addr_len = sizeof their_addr;

    if (sockfd == -1) {
        fprintf(stderr, "receiver: failed to bind, closing receiver\n");
        return -1;
    }

    printf("receiver: waiting to recvfrom on %s\n", argv[1]);

    while (1) {
        numbytes = recvfrom(sockfd, &msg, MAXLEN, 0,
                (struct sockaddr*)&their_addr, &addr_len);

        if (numbytes == -1) {
            perror("recvfrom");
            printf("receiver: receive failed, exiting\n");
            close(sockfd);
            exit(1);
        }
        printf("receiver: received packet #%d\n", msg.sequence_num);
        if (msg.sequence_num != current_packet) {
            printf("receiver: received out of order packet\n");
            printf("receiver: expecting %d, got %d\n", current_packet, msg.sequence_num);
            printf("receiver: if current message gets ACKed, %d will be requested\n", current_packet);
        }

        ack_status = ack_check();

        if (ack_status == MSG_DROPPED) {
            printf("receiver: message being treated as not received, still expecting %d\n", 
                    current_packet);
        } else {
            if (msg.sequence_num == current_packet) {
                current_packet += 1;
            }
            if (ack_status == ACK) {
                msg.sequence_num = current_packet;
                if (sendto(sockfd, &msg, numbytes, 0,
                            (struct sockaddr*)&their_addr, addr_len) == -1) {
                    perror("send");
                    printf("receiver: sendto failed, exiting\n");
                    close(sockfd);
                    exit(1);
                } else {
                    printf("receiver: sent ACK to sender\n");
                    printf("receiver: now expecting %d\n", current_packet);
                }
            } else {
                printf("receiver: message being treated as received, now expecting %d however ACK will not be sent\n",
                        current_packet);
            }
        }
    }

    return 0;
}
