// Alexei Doell cka067 11345642

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <list.h>
#include <time.h>
#include <netdb.h>

#include <shared.h>

#define POLL_FD_COUNT 2
#define SENDING_WINDOW 5

enum {
    INPUT,
    ACK,
    TIMEOUT,
    ERR
};

/* needed for my list library */
int free_wrapper(void* msg) {
    free(msg);
    return 0;
}

int connect_to_receiver(char* hostname, char* port) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
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

    freeaddrinfo(servinfo);

    return sockfd;
}

int enqueue_packet(LIST * queue) {
    char * input = NULL;
    size_t len = 0;
    struct fake_packet * msg;

    static int packet_num = 0;

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
    if (strlen(input) > MAXLEN) {
        fprintf(stderr, "sender: please input a msg less than %d characters long\n", MAXLEN);
        free(input);
        free(msg);
        return 0;
    }

    strcpy(msg->msg, input);
    if (input[0] == '\n') {
        free(input);
        free(msg);
        return 1;
    }

    free(input);
    ListPrepend(queue, msg);
    packet_num += 1;
    printf("sender: packet #%d added to sending queue\n", msg->sequence_num);
    return 0;
}


int send_packet(int dest, struct fake_packet* msg) {
    struct timespec timestamp;
    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    msg->timestamp = (long)timestamp.tv_sec * 1000 + timestamp.tv_nsec / 1000000;

    if (send(dest, msg, sizeof(struct fake_packet), 0) == -1) {
        perror("sender: send");
        fprintf(stderr, "sender: send failed\n");
        return -1;
    }
    printf("sender: sent packet #%d\n", msg->sequence_num);


    return 0;
}

int send_new_packet(int dest, LIST* msg_q, LIST* outstanding) {
    struct fake_packet * msg = ListTrim(msg_q);

    if (msg == NULL) {
        fprintf(stderr, "sender: failed to get message from queue\n");
        return -1;
    }
    send_packet(dest, msg);
    ListPrepend(outstanding, msg);

    return 0;
}


int resend_window(int dest, LIST* queue) {
    struct fake_packet * msg = ListLast(queue);
    if (msg == NULL) {
        fprintf(stderr, "sender: failed to get message from queue\n");
        return -1;
    }
    while (msg != NULL) {
        send_packet(dest, msg);
        msg = ListPrev(queue);
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

int remove_ack_packets(int ack_num, LIST* queue) {
    struct fake_packet * msg = ListLast(queue);
    if (msg == NULL) {
        fprintf(stderr, "sender: failed to get message from queue\n");
        return -1;
    }
    while (msg != NULL && msg->sequence_num < ack_num) {
        printf("sender: packet #%d now ACKed\n", msg->sequence_num);
        free(msg);
        ListRemove(queue);
        msg = ListLast(queue);
    }
 

    return 0;
}

int update_timeout(long timeout, LIST* queue) {
    struct timespec current_time;
    long current_ms;

    if (ListCount(queue) == 0) {
        return -1;
    }

    clock_gettime(CLOCK_MONOTONIC, &current_time);
    current_ms = current_time.tv_nsec / 1000000 + (long)current_time.tv_sec * 1000;
    return timeout - (current_ms - ((struct fake_packet*)ListLast(queue))->timestamp);
}

int poll_handler(struct pollfd* poll_fds, int fd_cnt, long timeout) {
    int poll_rv;

    poll_rv = poll(poll_fds, fd_cnt, timeout);
    if (poll_rv == -1) {
        printf("sender: poll failed\n");
        return ERR;
    } else if (poll_rv == 0) { // timeout
        return TIMEOUT;
    }

    if ((poll_fds[0].revents & POLLIN) > 0) {
        return INPUT;
    }

    if ((poll_fds[1].revents & POLLIN) > 0) {
        return ACK;
    }

    if ((poll_fds[1].revents & POLLERR) > 0) {
        printf("sender: receiving end is not open, please start the receiver and try again\n");
        return ERR;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int sockfd;
    long timeout, adjusted_timeout;
    int ack_num, last_ack_num = -1;
    int repeats = 0;

    LIST* msg_q;
    LIST* outstanding_q;

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

    outstanding_q = ListCreate();
    if (outstanding_q == NULL) {
        fprintf(stderr, "sender: failed to allocate message queue, closing sender\n");
    }

    /* init poll fields */
    poll_fds[0].fd = 0;
    poll_fds[0].events = POLLIN;
    poll_fds[1].fd = sockfd;
    poll_fds[1].events = POLLIN;

    printf("sender: please enter message contents then hit return key to send\n");
    while(1) { 

        adjusted_timeout = update_timeout(timeout, outstanding_q);

        switch (poll_handler(poll_fds, POLL_FD_COUNT, adjusted_timeout)) {

            case INPUT:
                if (enqueue_packet(msg_q) != 0) {
                    goto cleanup;
                };
                while (ListCount(outstanding_q) < SENDING_WINDOW && ListCount(msg_q) > 0) {
                    send_new_packet(sockfd, msg_q, outstanding_q);
                }
                break;

            case ACK:
                ack_num = get_ack(sockfd);
                printf("sender: got ack for %d\n", ack_num);
                if (ack_num == last_ack_num) {
                    repeats += 1;
                } else {
                    repeats = 0;
                }
                last_ack_num = ack_num;
                if (repeats >= 2) {
                    printf("sender: received 3 repeated ACKs\n");
                    if (ListCount(outstanding_q) > 0) {
                        printf("sender: resending window\n");
                        resend_window(sockfd, outstanding_q);
                    } else {
                        printf("sender: no need to resend, no outstanding messages\n");
                    }
                    repeats = 0;
                }
                if (ListCount(outstanding_q) > 0) {
                    remove_ack_packets(ack_num, outstanding_q);
                }
                while (ListCount(outstanding_q) < SENDING_WINDOW && ListCount(msg_q) > 0) {
                    send_new_packet(sockfd, msg_q, outstanding_q);
                }
                break;

            case TIMEOUT:
                printf("sender: timeout on ACK\n");
                printf("sender: resending window\n");
                resend_window(sockfd, outstanding_q);
                break;

            case ERR:
            default:
                goto cleanup;
        }
    }

cleanup:
    ListFree(msg_q, free_wrapper);
    ListFree(outstanding_q, free_wrapper);
    printf("sender: closing sender\n");

    return 0;
}

