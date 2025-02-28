// Alexei Doell cka067 11345652

// stdlib
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// posix
#include <sys/timerfd.h>
#include <netdb.h>
#include <unistd.h>

// io_uring
#include <liburing.h>
#include <linux/io_uring.h>

// list lib
#include <list.h>

#define MAX_ROUTERS 20

enum {
    MSG_SENT,
    MSG_RECEIVED,
    STATUS_TIMER,
    NEIGHBOR_TIMEOUT,
};

union uring_user_data {
        __u64 casted_data;
        struct {
            int32_t type;
            int32_t port;
        } splitdata;
};

struct distance_pair {
    int32_t id;
    int32_t cost;
};

struct netinfo {
    int fd;
    struct addrinfo* addrinfo;
};

struct router {
    int32_t id;
    int32_t cost;
    int32_t next_hop;
    bool down;
    bool neighbor;
    bool known;
    struct distance_pair distance_table[20];
};

struct msginfo {
    struct msghdr msghdr;
    struct iovec iov[1];
    struct sockaddr_in addr_buffer;
    struct distance_pair msg_buffer[20];
    struct cmsghdr control_buffer;
};

static int known_router_count = 0;
static int neighbor_count;
static LIST* known_routers;
static struct router all_routers[10001];
static struct netinfo neighbor_info[20];
static struct msginfo msginfo;
static int timerfds[20];

/* takes a port as param and does all the udp connection boiler plate syscalls */
int bind_to(char* port) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        fprintf(stderr, "router: failed on port %s, please\
                try another port\n", port);
        return -1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
            perror("router: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("router: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "router: failed to bind socket on port %s, please "
                "try another\n", port);
        return -1;
    }

    return sockfd;
}

int get_neighbor_fd(char* port, struct addrinfo** addr) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
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
        fprintf(stderr, "sender: failed to get socket for %s, please "
                "try another port\n", port);
        return -1;
    }

    *addr = p;

    return sockfd;
}

int init_msginfo_struct(struct msginfo* msginfo) {
    memset(&msginfo->msg_buffer, 0, sizeof(msginfo->msg_buffer));
    memset(msginfo->iov, 0, sizeof(msginfo->iov));
    memset(&msginfo->msghdr, 0, sizeof(msginfo->msghdr));
    msginfo->iov[0].iov_base = msginfo->msg_buffer;
    msginfo->iov[0].iov_len = sizeof(msginfo->msg_buffer);

    msginfo->msghdr.msg_iov = msginfo->iov;
    msginfo->msghdr.msg_iovlen = 1;
    msginfo->msghdr.msg_name = &msginfo->addr_buffer;
    msginfo->msghdr.msg_namelen = sizeof(msginfo->addr_buffer);
    msginfo->msghdr.msg_control = &msginfo->control_buffer;
    msginfo->msghdr.msg_controllen = sizeof(msginfo->control_buffer);
    msginfo->msghdr.msg_flags = 0;

    return 0;
}

void print_distance_table(void) {
    struct router* curr;
    printf("\n----------------  Router Table  --------------\n");
    printf("  Destination  |      Cost      |  Next Hop  \n");
    printf("----------------------------------------------\n");

    curr = ListFirst(known_routers);
    while(curr != NULL) {
        printf("     %-10d|       %-9d|%9d    \n", curr->id, curr->cost, curr->next_hop);
        curr = ListNext(known_routers);
    }
    printf("----------------------------------------------\n");
}

void send_to_neighbors(int recvfd, struct io_uring* ring, struct distance_pair msg_buf[]) {
    union uring_user_data user_data;
    struct io_uring_sqe *sqe;
    struct router* curr;
    int i = 0;
    curr = ListFirst(known_routers);
    while(curr != NULL) {
        msg_buf[i].id = htonl(curr->id);
        msg_buf[i].cost = htonl(all_routers[curr->id - 30000].cost);
        curr = ListNext(known_routers);
        i += 1;
    }

    // need to find entry of neighbor being sent to and set its cost to -1
    for (int i = 1; i < neighbor_count; ++i) {
        sqe = io_uring_get_sqe(ring);
        user_data.splitdata.type = MSG_SENT;
        io_uring_sqe_set_data64(sqe, user_data.casted_data);
        io_uring_prep_sendto(sqe, recvfd, msg_buf, known_router_count * sizeof(struct distance_pair), 0, 
                neighbor_info[i].addrinfo->ai_addr, neighbor_info[i].addrinfo->ai_addrlen);
    }
}

void prep_recv_from_neighbors(int recvfd, struct io_uring* ring, struct msginfo *msginfo) {
    union uring_user_data user_data;
    struct io_uring_sqe *sqe;
    sqe = io_uring_get_sqe(ring);
    user_data.splitdata.type = MSG_RECEIVED;
    user_data.splitdata.port = 0;
    io_uring_sqe_set_data64(sqe, user_data.casted_data);
    io_uring_prep_recvmsg(sqe, recvfd, &msginfo->msghdr, 0);
}

void handle_received_data(LIST* list, int32_t neighbor, struct distance_pair msg_buffer[], int len) {
    int32_t id, cost, dw_xy;
    struct router* curr;

    for (unsigned int i = 0; i < len / sizeof(struct distance_pair); ++i) {
        id = ntohl(msg_buffer[i].id);
        dw_xy = ntohl(msg_buffer[i].cost);

        cost = all_routers[neighbor - 30000].cost + dw_xy;
        curr = &all_routers[id - 30000];
        if (!curr->known) {
            curr->id = id;
            curr->cost = dw_xy == -1 ? -1 : cost;
            curr->next_hop = neighbor;
            curr->known = true;
            ListAppend(list, curr);
        } else {
            if (curr->next_hop == neighbor) {
                curr->cost = dw_xy == -1 ? -1 : cost;
            } else {
                if (dw_xy != -1 && curr->cost > cost) {
                    curr->cost = cost;
            curr->next_hop = neighbor;
                }
            }

        }
    }
}

int init_neighbors(LIST* list, char* args[]) {

    struct router* curr;
    int id;
    id = strtol(args[1], NULL, 10);
    curr = &all_routers[id - 30000];
    curr->id = id;
    curr->known = true;
    curr->cost = 0;
    curr->next_hop = curr->next_hop;
    ListAppend(list, curr);
    // initialize all neighbors
    for (int i = 0; i < neighbor_count - 1; ++i) {
        id = strtol(args[(2 * i) + 2], NULL, 10);
        curr = &all_routers[id - 30000];
        curr->id = id;
        curr->cost = strtol(args[(2 * i) + 3], NULL, 10);
        curr->known = true;
        curr->next_hop = curr->next_hop;
        // need to redo this stupid bullshit
        all_routers[curr->id - 30000] = *curr;
        ListAppend(list, curr);
    }

    return 0;

}

int main(int argc, char* argv[]) {

    struct io_uring ring;
    struct io_uring_cqe* cqe;
    int size;
    struct distance_pair msg_buf[MAX_ROUTERS];
    union uring_user_data user_data;

    int recvfd;

    if (argc % 2 != 0) {
        printf("usage: ./router <self port> <other port> <cost> ...\n");
    }
    neighbor_count = 1 + (argc - 2) / 2;
    known_router_count = neighbor_count;

    known_routers = ListCreate();
    init_neighbors(known_routers, argv);
    if (io_uring_queue_init(1 + 2 * neighbor_count, &ring, 0) != 0) {
        printf("router: failed to init io_uring, exiting\n");
        return -1;
    }

    recvfd = bind_to(argv[1]);
    if (recvfd == -1) {
        printf("router: failed to bind to port, exiting\n");
        return -1;
    }
    
    for (int i = 0; i < neighbor_count - 1; ++i) {
        neighbor_info[i + 1].fd = get_neighbor_fd(argv[(2 * i) + 2], &neighbor_info[i + 1].addrinfo);
    }
    init_msginfo_struct(&msginfo);
    // initial send to all neighbors
    send_to_neighbors(recvfd, &ring, msg_buf);
    // initial recvmsg request
    prep_recv_from_neighbors(recvfd, &ring, &msginfo);
    io_uring_submit(&ring);

    while (1) {
        io_uring_wait_cqe(&ring, &cqe);
        user_data.casted_data = io_uring_cqe_get_data64(cqe);
        io_uring_cqe_seen(&ring, cqe);
        switch (user_data.splitdata.type) {
            case MSG_RECEIVED:
                printf("received tye shi\n");
                size = cqe->res;
                prep_recv_from_neighbors(recvfd, &ring, &msginfo);
                handle_received_data(known_routers, ntohs(((struct sockaddr_in*)msginfo.msghdr.msg_name)->sin_port), msginfo.msg_buffer, size);
                print_distance_table();
                io_uring_submit(&ring);
                break;
            case MSG_SENT:
                printf("sent message to %d\n", user_data.splitdata.port);
        }
    }

    io_uring_queue_exit(&ring);
    return 0;
}
