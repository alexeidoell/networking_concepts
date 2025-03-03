// Alexei Doell cka067 11345652

// stdlib
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// posix
#include <netdb.h>
#include <unistd.h>

// io_uring
#include <linux/io_uring.h>
#include <liburing.h>

// list lib
#include <list.h>

#define MAX_NEIGHBORS 19
#define MAX_ROUTERS 20

static struct __kernel_timespec two_sec = {
    .tv_sec = 2
};

static struct __kernel_timespec five_sec = {
    .tv_sec = 5
};

enum {
    MSG_SENT,
    MSG_RECEIVED,
    GLOBAL_TIMEOUT,
    NEIGHBOR_TIMEOUT,
    REARM_TIMER,
};

union uring_user_data {
    __u64 casted_data;
    struct {
        __s32 type;
        __s32 port;
    } split_data;
};

struct distance_pair {
    int32_t id;
    int32_t cost;
};

struct netinfo {
    int32_t id;
    int fd;
    struct addrinfo* addrinfo;
};

struct router {
    int32_t id;
    int32_t cost;
    int32_t initial_cost;
    int32_t next_hop;
    bool down;
    bool known;
    bool acknowledged; // only for error message
};

struct msginfo {
    struct msghdr msghdr;
    struct iovec iov[1];
    struct sockaddr_in addr_buffer;
    struct distance_pair msg_buffer[20];
    struct cmsghdr control_buffer;
};

bool running = true;
static struct netinfo self;
static int known_router_count = 0;
static int neighbor_count;
static LIST* known_routers;
static struct router all_routers[10001];
static struct netinfo neighbor_info[10];
static struct msginfo msginfo;

void sig_handler(int sig __attribute__((unused))) {
    running = false;
}

/* takes a port as param and does all the udp connection boiler plate syscalls */
int bind_to(char* port, struct addrinfo** addr) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        printf("getaddrinfo: %s\n", gai_strerror(rv));
        printf("router: failed on port %s, please\
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
        printf("router: failed to bind socket on port %s, please "
                "try another\n", port);
        freeaddrinfo(servinfo);
        return -1;
    }

    *addr = p;

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
        printf("getaddrinfo: %s\n", gai_strerror(rv));
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
        printf("sender: failed to get socket for %s, please "
                "try another port\n", port);
        return -1;
    }

    *addr = p;

    return sockfd;
}

void cleanup_network_fds(void) {
    for (int i = 0; i < neighbor_count; ++i) {
        freeaddrinfo(neighbor_info[i].addrinfo);
        if (neighbor_info[i].fd > 0) {
            close(neighbor_info[i].fd);
        }
    }
    freeaddrinfo(self.addrinfo);
    if (self.fd > 0) {
        close(self.fd);
    }
}

int init_sig_handler(void handler(int)) {
    struct sigaction sa;
    sa.sa_handler = handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        printf("router %d: failed to set up signal handler\n", self.id);
        return -1;
    }
    return 0;
}

void init_msginfo_struct(struct msginfo* msginfo) {
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
}

void print_distance_table(void) {
    struct router* curr;
    printf("\n----------------  Router Table  --------------\n");
    printf("  Destination  |      Cost      |  Next Hop  \n");
    printf("----------------------------------------------\n");

    curr = ListFirst(known_routers);
    while(curr != NULL) {
        printf("     %-10d|", curr->id);
        if (curr->cost != -1) {
            printf("       %-9d|", curr->cost);
        } else {
            printf("    infinity    |");
        }
        if (curr->next_hop == 0) {
            printf("      -\n");
        } else {
            printf("%9d    \n", curr->next_hop);
        }
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
        curr = ListNext(known_routers);
        i += 1;
    }

    // need to find entry of neighbor being sent to and set its cost to -1
    for (int i = 0; i < neighbor_count; ++i) {
        sqe = io_uring_get_sqe(ring);
        if (!sqe) {
            printf("router %d: failed to grab sqe\n", self.id);
            // tbh i could make it keep running but just for consistency i do not
            running = false;
            return;
        }
        user_data.split_data.type = MSG_SENT;
        // setting the port is only necessary for better error reporting later
        user_data.split_data.port = ntohs(((struct sockaddr_in*)neighbor_info[i].addrinfo->ai_addr)->sin_port);
        io_uring_sqe_set_data64(sqe, user_data.casted_data);
        // have to do this stupid o(n^2) method because it has to all be in one
        // single udp segment
        for (int j = 0; j < known_router_count; ++j) {
            if (all_routers[ntohl(msg_buf[j].id) - 30000].next_hop == neighbor_info[i].id) {
                msg_buf[j].cost = htonl(-1);
            } else {
                msg_buf[j].cost = htonl(all_routers[ntohl(msg_buf[j].id) - 30000].cost);
            }
        }
        io_uring_prep_sendto(sqe, recvfd, msg_buf, known_router_count * sizeof(struct distance_pair), 0, 
                neighbor_info[i].addrinfo->ai_addr, neighbor_info[i].addrinfo->ai_addrlen);
        // need to submit early here, otherwise the io_uring will try to use the same 
        // buffer contents to send, but we need to send different buffer contents
        // unfortunately this means extra syscalls. perhaps the conclusion to make is that
        // io_uring is better when sending out static contents like a web server
        if (io_uring_submit(ring) < 1) {
            printf("router %d: failed to send to %d\n", self.id, user_data.split_data.port);
            running = false;
            return;
        }
    }
}

void prep_recv_from_neighbors(int recvfd, struct io_uring* ring, struct msginfo *msginfo) {
    union uring_user_data user_data;
    struct io_uring_sqe *sqe;
    sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        printf("router %d: failed to grab sqe\n", self.id);
        running = false;
        return;
    }
    user_data.split_data.type = MSG_RECEIVED;
    user_data.split_data.port = 0;
    io_uring_sqe_set_data64(sqe, user_data.casted_data);
    io_uring_prep_recvmsg(sqe, recvfd, &msginfo->msghdr, 0);
}


void arm_neighbor_timer(struct io_uring* ring, int32_t id) {
    struct io_uring_sqe *sqe;
    union uring_user_data user_data;
    sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        printf("router %d: failed to grab sqe\n", self.id);
        running = false;
        return;
    }
    user_data.split_data.type = NEIGHBOR_TIMEOUT;
    user_data.split_data.port = id;
    io_uring_sqe_set_data64(sqe, user_data.casted_data);
    io_uring_prep_timeout(sqe, &five_sec, 0, 0);

}

void rearm_neighbor_timer(struct io_uring* ring, int32_t id) {
    struct io_uring_sqe *sqe;
    union uring_user_data user_data, target_timer;
    sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        printf("router %d: failed to grab sqe\n", self.id);
        running = false;
        return;
    }
    target_timer.split_data.type = NEIGHBOR_TIMEOUT;
    target_timer.split_data.port = id;
    user_data.split_data.type = REARM_TIMER;
    user_data.split_data.port = id;
    io_uring_sqe_set_data64(sqe, user_data.casted_data);
    io_uring_prep_timeout_update(sqe, &five_sec, target_timer.casted_data, 0);
}

void prep_neighbor_timers(struct io_uring* ring) {
    struct io_uring_sqe *sqe;
    union uring_user_data user_data;
    for (int i = 0; i < neighbor_count; ++i) {
        sqe = io_uring_get_sqe(ring);
        if (!sqe) {
            printf("router %d: failed to grab sqe\n", self.id);
            return;
        }
        user_data.split_data.type = NEIGHBOR_TIMEOUT;
        user_data.split_data.port = neighbor_info[i].id;
        io_uring_sqe_set_data64(sqe, user_data.casted_data);
        io_uring_prep_timeout(sqe, &five_sec, 0, IORING_TIMEOUT_ETIME_SUCCESS);
    }
}

void prep_global_timer(struct io_uring* ring) {
    struct io_uring_sqe *sqe;
    union uring_user_data user_data;

    sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        printf("router %d: failed to grab sqe\n", self.id);
        return;
    }
    user_data.split_data.type = GLOBAL_TIMEOUT;
    user_data.split_data.port = 0;
    io_uring_sqe_set_data64(sqe, user_data.casted_data);
    io_uring_prep_timeout(sqe, &two_sec, 0, IORING_TIMEOUT_MULTISHOT | IORING_TIMEOUT_ETIME_SUCCESS);
}

int uring_init(struct io_uring* ring) {
    init_msginfo_struct(&msginfo);
    // initial recvmsg request
    prep_recv_from_neighbors(self.fd, ring, &msginfo);
    // prep global timeout sqe request
    prep_global_timer(ring);
    // prep neighbor timer sqe requests
    prep_neighbor_timers(ring);

    if (io_uring_submit(ring) != neighbor_count + 2) { // initialization failed
        printf("router %d: failed to initialize all needed uring requests\n", self.id);
        return -1;
    }
    return 0;
}

int network_init(char* args[], int neighbor_count) {
    char* port;
    self.fd = bind_to(args[1], &self.addrinfo);
    if (self.fd == -1) {
        printf("router: failed to bind to port %d\n", self.id);
        return -1;
    }
    for (int i = 0; i < neighbor_count; ++i) {
        port = args[(2 * i) + 2];
        neighbor_info[i].fd = get_neighbor_fd(port, &neighbor_info[i].addrinfo);
        if (neighbor_info[i].fd <= 0) {
            printf("router %d: failed to bind to neighbor %s\n", self.id, port);
            return -1;
        }
    }
    return 0;
}

void purge_neighbor(int32_t neighbor_id) {
    struct router *neighbor = &all_routers[neighbor_id - 30000], *curr;
    neighbor->down = true;
    curr = ListFirst(known_routers);
    while(curr != NULL) {
        if (curr->next_hop == neighbor_id) {
            curr->cost = -1;
            curr->next_hop = 0;
        }
        curr = ListNext(known_routers);
    }
}

void handle_received_data(LIST* list, int32_t neighbor, struct distance_pair msg_buffer[], int len, struct io_uring* ring) {
    int32_t id, cost, dw_xy;
    struct router* curr;

    if (!all_routers[neighbor - 30000].down) {
        rearm_neighbor_timer(ring, neighbor);
    }

    for (unsigned int i = 0; i < len / sizeof(struct distance_pair); ++i) {
        id = ntohl(msg_buffer[i].id);
        dw_xy = ntohl(msg_buffer[i].cost);

        cost = all_routers[neighbor - 30000].initial_cost + dw_xy;
        curr = &all_routers[id - 30000];
        if (!curr->known) {
            if (ListCount(list) >= MAX_ROUTERS) {
                if (!curr->acknowledged) {
                    printf("received information about router %d,\
                            however max router count has been reached so ignoring info\n", id);
                    curr->acknowledged = true;
                }
            } else {
                known_router_count += 1;
                curr->id = id;
                if (dw_xy == -1) {
                    curr->cost = -1;
                    curr->next_hop = 0;
                } else {
                    curr->cost = cost;
                    curr->next_hop = neighbor;
                }
                curr->known = true;
                ListAppend(list, curr);
            }
        } else {
            if (curr->id == neighbor) { // info about neighbor itself
                if (curr->down) {
                    arm_neighbor_timer(ring, neighbor);
                    curr->down = false;
                }
                if (curr->cost > cost || curr->cost == -1) {
                    curr->cost = curr->initial_cost;
                    curr->next_hop = neighbor;
                }
            } else if (curr->next_hop == 0 && curr->cost == -1) { // no route currently known
                if (dw_xy != -1) {
                    curr->next_hop = neighbor;
                    curr->cost = cost;
                }
            } else if (curr->next_hop != neighbor) { // route known through different next hop
                if (dw_xy != -1 && curr->cost > cost) {
                    curr->next_hop = neighbor;
                    curr->cost = cost;
                }
            } else if (curr->next_hop == neighbor) { // route known through this router already
                if (dw_xy != -1) {
                    curr->cost = cost;
                } else {
                    curr->cost = -1;
                    curr->next_hop = 0;
                }
            }

        }
    }
}

int init_neighbors(LIST* list, char* args[]) {
    struct router* curr;
    int id;
    id = strtol(args[1], NULL, 10);
    self.id = id;
    if (id < 30000 || id > 40000) {
        printf("router: invalid port number %d\n", id);
        return -1;
    }
    curr = &all_routers[id - 30000];
    curr->id = id;
    curr->known = true;
    curr->cost = 0;
    curr->next_hop = curr->next_hop;
    ListAppend(list, curr);
    // initialize all neighbors
    for (int i = 0; i < neighbor_count; ++i) {
        id = strtol(args[(2 * i) + 2], NULL, 10);
        if (id < 30000 || id > 40000) {
            printf("router %d: invalid port number %d\n", self.id, id);
            return -1;
        }
        curr = &all_routers[id - 30000];
        curr->id = id;
        curr->cost = strtol(args[(2 * i) + 3], NULL, 10);
        curr->initial_cost = curr->cost;
        curr->known = true;
        curr->next_hop = curr->id;
        neighbor_info[i].id = curr->id;
        ListAppend(list, curr);
    }

    return 0;
}


int main(int argc, char* argv[]) {

    struct io_uring ring;
    struct io_uring_cqe* cqe;
    int size;
    int32_t neighbor_id;
    struct distance_pair msg_buf[MAX_ROUTERS];
    union uring_user_data user_data;

    if (argc % 2 != 0) {
        printf("usage: ./router <self_port> <neighbor_port> <neighbor_cost> ...\n");
        return -1;
    }
    neighbor_count = (argc - 2) / 2;
    if (neighbor_count > MAX_NEIGHBORS) {
        printf("the maximum allowed number of neighbors for one router is %d\n", MAX_NEIGHBORS);
        return -1;
    } else if (neighbor_count < 1) {
        printf("the minimum allowed number of neighbors for one router is 1\n");
        return -1;
    }
    known_router_count = neighbor_count + 1;

    known_routers = ListCreate();
    if (init_neighbors(known_routers, argv) == -1) {
        printf("router: failed to init neighbors\n");
        ListFree(known_routers, NULL);
        printf("\nrouter exiting\n");
        return -1;
    }

    if (io_uring_queue_init(1 + 2 * neighbor_count, &ring, 0) != 0) {
        printf("router %d: failed to init io_uring, exiting\n", self.id);
        ListFree(known_routers, NULL);
        printf("\nrouter %d exiting\n", self.id);
        return -1;
    }

    if (network_init(argv, neighbor_count) == -1) {
        goto cleanup;
    }

    if (init_sig_handler(sig_handler) == -1) {
        goto cleanup;
    }

    // initial submit
    if (uring_init(&ring) == -1) {
        goto cleanup;
    };

    // initial send to all neighbors
    // send_to_neighbors submits itself
    // on any error, running will be set to false so it will just go straight to cleanup
    send_to_neighbors(self.fd, &ring, msg_buf);

    print_distance_table();
    while (running) {
        io_uring_wait_cqe(&ring, &cqe);
        if (!running) break; // signal handler was called
        user_data.casted_data = io_uring_cqe_get_data64(cqe);
        switch (user_data.split_data.type) {
            case MSG_RECEIVED:
                size = cqe->res;
                if (size < 0) {
                    printf("router %d: receive failed\n", self.id);
                    running = false;
                } else {
                    prep_recv_from_neighbors(self.fd, &ring, &msginfo);
                    neighbor_id = ntohs(((struct sockaddr_in*)msginfo.msghdr.msg_name)->sin_port);
                    handle_received_data(known_routers, neighbor_id, msginfo.msg_buffer, size, &ring);
                    io_uring_submit(&ring);
                }
                break;
            case GLOBAL_TIMEOUT:
                if (cqe->res != -ETIME) { 
                    printf("router %d: setting global timer failed\n", self.id);
                    running = false;
                } else {
                    send_to_neighbors(self.fd, &ring, msg_buf);
                    print_distance_table();
                }
                break;
            case NEIGHBOR_TIMEOUT:
                if (cqe->res == -ETIME) { // -ETIME means the timeout fired successfully
                    purge_neighbor(user_data.split_data.port);
                } else {
                    printf("router %d: setting timer for neighbor %d failed\n", self.id,user_data.split_data.port);
                    running = false;
                }
                break;
            case REARM_TIMER:
                if (cqe->res < 0 && cqe->res != -EALREADY) { // can ignore EALREADY because that just means the timer fired before the cancel went through
                    printf("router %d: resetting timer for neighbor %d failed\n", self.id, user_data.split_data.port);
                    running = false;
                }
                break;
            case MSG_SENT:
                if (cqe->res < 0) { // same error codes will be returned as in sendto
                    printf("router %d: sending message to neighbor %d failed\n", self.id, user_data.split_data.port);
                }
                break;
            default: // shouldn't ever happen, but just in case
                printf("router %d: unknown error occured\n", self.id);
                running = false;
                break;

        }
        io_uring_cqe_seen(&ring, cqe);
    }

cleanup:
    cleanup_network_fds();
    io_uring_queue_exit(&ring);
    ListFree(known_routers, NULL);
    printf("\nrouter %d exiting\n", self.id);

    return 0;
}
