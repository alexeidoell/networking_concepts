// Alexei Doell cka067 11345652

// stdlib
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
    struct distance_pair distance_table[20];
};

static int known_router_count = 0;
static struct router known_routers[20];
static struct netinfo neighbor_info[20];
static int timerfds[20];

/* takes a port as param and does all the udp connection boiler plate syscalls */
int bind_to(char* port) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

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

    freeaddrinfo(servinfo);

    return sockfd;
}

void print_distance_table(void) {
    struct router* curr;
    printf("\n----------------  Router Table  --------------\n");
    printf("  Destination  |      Cost      |  Next Hop  \n");
    printf("----------------------------------------------\n");

    for (int i = 0; i < known_router_count; ++i) {
        curr = &known_routers[i];
        printf("     %-10d|       %-9d|%9d    \n", curr->id, curr->cost, curr->next_hop);
    }
    printf("----------------------------------------------\n");
}

int main(int argc, char* argv[]) {

    struct io_uring ring;
    struct io_uring_cqe* cqe;
    struct io_uring_sqe* sqe;
    union uring_user_data user_data;
    struct sockaddr_in addr_buffer;
    struct cmsghdr control_buffer;
    struct msghdr msghdr;
    struct iovec iov[1];
    char msg_buffer[512];

    int recvfd;

    if (argc % 2 != 0) {
        printf("usage: ./router <self port> <other port> <cost> ...\n");
    }

    int neighbor_count = (argc - 2) / 2;
    known_router_count = neighbor_count;

    // initialize all neighbors
    for (int i = 0; i < neighbor_count; ++i) {
        known_routers[i].id = strtol(argv[(2 * i) + 2], NULL, 10);
        known_routers[i].cost = strtol(argv[(2 * i) + 3], NULL, 10);
        known_routers[i].next_hop = known_routers[i].id;
        known_routers[i].neighbor = true;
    }

    if (io_uring_queue_init(1 + 2 * neighbor_count, &ring, 0) != 0) {
        printf("router: failed to init io_uring, exiting\n");
        return -1;
    }

    recvfd = bind_to(argv[1]);
    if (recvfd == -1) {
        printf("router: failed to bind to port, exiting\n");
        return -1;
    }
    
    printf("List of Neighbors:\n");
    for (int i = 0; i < neighbor_count; ++i) {
        printf("Neighbor #%d\n", i);
        printf("ID: %d\n", known_routers[i].id);
        printf("Cost: %d\n", known_routers[i].cost);
        printf("Next hop: %d\n\n", known_routers[i].next_hop);
    }

    for (int i = 0; i < neighbor_count; ++i) {
        sqe = io_uring_get_sqe(&ring);
        neighbor_info[i].fd = get_neighbor_fd(argv[(2 * i) + 2], &neighbor_info[i].addrinfo);
        user_data.splitdata.type = MSG_SENT;
        user_data.splitdata.port = known_routers[i].id;
        io_uring_sqe_set_data64(sqe, user_data.casted_data);
        io_uring_prep_sendto(sqe, neighbor_info[i].fd, "hi", 3, 0, neighbor_info[i].addrinfo->ai_addr, neighbor_info[i].addrinfo->ai_addrlen);
    }
    printf("submitted %d entries\n", io_uring_submit(&ring));

    io_uring_wait_cqe(&ring, &cqe);
    io_uring_cqe_seen(&ring, cqe);

    print_distance_table();

    sqe = io_uring_get_sqe(&ring);
    user_data.splitdata.type = MSG_RECEIVED;
    user_data.splitdata.port = 0;
    io_uring_sqe_set_data64(sqe, user_data.casted_data);
    memset(&msg_buffer, 0, sizeof(msg_buffer));
    memset(iov, 0, sizeof(iov));
    memset(&msghdr, 0, sizeof(msghdr));
    iov[0].iov_base = msg_buffer;
    iov[0].iov_len = sizeof(msg_buffer);

    msghdr.msg_iov = iov;
    msghdr.msg_iovlen = 1;
    msghdr.msg_name = &addr_buffer;
    msghdr.msg_namelen = sizeof(addr_buffer);
    msghdr.msg_control = &control_buffer;
    msghdr.msg_controllen = sizeof(control_buffer);
    msghdr.msg_flags = 0;
    io_uring_prep_recvmsg(sqe, recvfd, &msghdr, 0);
    io_uring_submit(&ring);
    io_uring_wait_cqe(&ring, &cqe);
    io_uring_cqe_seen(&ring, cqe);
    


    io_uring_queue_exit(&ring);
    return 0;
}
