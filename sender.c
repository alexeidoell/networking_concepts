// Alexei Doell cka067 11345642

#include <bits/time.h>
#include <fcntl.h>
#include <netdb.h>
#include <shared.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <list.h>
#include <time.h>

#define POLL_FD_COUNT 2
#define SENDING_WINDOW 3

int connect_to_receiver(char* hostname, char* port) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        fprintf(stderr, "sender: failed on %s:%s, please "
                "try another hostname/port\n", hostname, port);
        return -1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
            perror("sender: socket");
            continue;
        }
            break;
    }

    if (p == NULL) {
        fprintf(stderr, "sender: failed to get socket for %s:%s, please "
                "try another hostname/port\n", hostname, port);
        return -1;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
        perror("sender: connect");
        fprintf(stderr, "sender: failed to connect to %s:%s, please "
                "try another hostname/port\n", hostname, port);
        return -1;
    }


    return sockfd;
}

int enqueue_packet(int packet_num, LIST* queue) {
    char * input = NULL;
    size_t len = 0;
    struct fake_packet * msg;
    struct timespec timestamp;

    clock_gettime(CLOCK_MONOTONIC, &timestamp);

    msg = malloc(sizeof(struct fake_packet));
    if (msg == NULL) {
        perror("sender: malloc");
        fprintf(stderr, "sender: failed to allocate message\n");
        return -1;
    }

    msg->sequence_num = packet_num;
    if (getline(&input, &len, stdin) == -1) {
        perror("sender: getline");
        fprintf(stderr, "sender: getline failed\n");
        free(input);
        return -1;
    }

    strcpy(msg->msg, input);
    free(input);

    msg->timestamp = timestamp.tv_sec * 1000 + timestamp.tv_nsec / 1000;
    ListAppend(queue, msg);
    return 0;
}



int send_packet(int dest, LIST* list) {
    struct fake_packet * msg = ListLast(list);

    if (msg == NULL) {
        fprintf(stderr, "sender: failed to get message from queue\n");
        return -1;
    }


    if (send(dest, msg, sizeof(struct fake_packet), 0) == -1) {
        perror("sender: send");
        fprintf(stderr, "sender: send failed\n");
        return -1;
    }

    return 0;
}

int get_ack(int fd) {
    struct fake_packet ack;
    if (recv(fd, &ack, sizeof ack, 0) == -1) {
        perror("sender: recv:");
        fprintf(stderr, "sender: failed to recv ack\n");
        return -1;
    }

    return ack.sequence_num;
}


int main(int argc, char *argv[])
{
    int sockfd;
    int poll_rv;
    int timeout, adjusted_timeout;
    int current_packet = 0;
    int ack_num;
    struct timespec current_time;

    LIST* msg_q;

    struct pollfd poll_fds[POLL_FD_COUNT];

    if (argc != 4) {
        fprintf(stderr,"usage: sender <timeout> <hostname> <server_port>\n");
        exit(1);
    }

    sockfd = connect_to_receiver(argv[2], argv[3]);

    if (sockfd == -1) {
        fprintf(stderr, "sender: failed to connect, closing sender\n");
        return -1;
    }

    timeout = atoi(argv[1]) * 1000;
    if (timeout == 0) {
        fprintf(stderr, "sender: please give a valid timeout period in seconds, closing sender\n");
    }

    msg_q = ListCreate();
    if (msg_q == NULL) {
        fprintf(stderr, "sender: failed to allocate message queue, closing sender\n");
    }

    /* init poll fields */
    poll_fds[0].fd = 0;
    poll_fds[0].events = POLLIN;
    poll_fds[1].fd = sockfd;
    poll_fds[1].events = POLLIN;

    printf("sender: please enter message contents\n");
    while(1) { 
        if (ListCount(msg_q) == 0) {
            adjusted_timeout = -1;
        } else {
            clock_gettime(CLOCK_MONOTONIC, &current_time);
            adjusted_timeout = timeout - ((current_time.tv_nsec / 1000 + current_time.tv_sec * 1000) - ((struct fake_packet*)ListLast(msg_q))->timestamp);
        }
        poll_rv = poll(poll_fds, POLL_FD_COUNT, adjusted_timeout);

        if (poll_rv == -1) {
            printf("sender: poll failed\n");
            return -1;
        } else if (poll_rv == 0) { // timeout
            printf("sender: timeout on ACK\n");
            printf("sender: resending window\n");
            send_packet(sockfd, msg_q);
        }

        if ((poll_fds[0].revents & POLLIN) > 0) {
            enqueue_packet(current_packet++, msg_q);
            send_packet(sockfd, msg_q);
        }

        if ((poll_fds[1].revents & POLLIN) > 0) {
            ack_num = get_ack(sockfd);
            printf("sender: got ack for %d\n", ack_num);
            ListTrim(msg_q);
        }
        
    }
}

