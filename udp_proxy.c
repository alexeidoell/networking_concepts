// Alexei Doell cka067 11345642

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <shared.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#define PROXYPORT "34923"

#define BACKLOG 10

void sigchld_handler(int s __attribute__((unused)))
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

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
    int servfd;
    int listenfd, new_fd, mtxfd;  // listen on sock_fd, new connection on new_fd
    pthread_mutex_t* mutex;
    struct addrinfo hints, *servinfo, *p, *prx;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    int yes=1;
    struct sigaction sa;
    char c[INET6_ADDRSTRLEN];
    char s[INET6_ADDRSTRLEN];
    int rv;
    int numbytes;

    if (argc != 3) {
        fprintf(stderr,"usage: udp_proxy hostname server_port\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((servfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("udp proxy: socket");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "udp proxy: failed to create socket\n");
        return 2;
    }


    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PROXYPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(prx = servinfo; prx != NULL; prx = prx->ai_next) {
        if ((listenfd = socket(prx->ai_family, prx->ai_socktype,
                prx->ai_protocol)) == -1) {
            perror("udp proxy: socket");
            continue;
        }

        if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(listenfd, prx->ai_addr, prx->ai_addrlen) == -1) {
            close(listenfd);
            perror("udp proxy: bind");
            continue;
        }

        break;
    }


    if (prx == NULL)  {
        fprintf(stderr, "udp proxy: failed to bind\n");
        exit(1);
    }

    if (listen(listenfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    mtxfd = shm_open("cka067 udp mutex", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (mtxfd == -1) {
        perror("shm_open");
        printf("udp proxy: shm_open failed to create memory for mutex\n");
        exit(1);
    }
    if (ftruncate(mtxfd, sizeof(pthread_mutex_t)) == -1) {
        perror("ftruncate");
        printf("udp proxy: ftruncate failed to create memory for mutex\n");
        exit(1);
    }
    mutex = mmap(NULL, sizeof(pthread_mutex_t), PROT_WRITE | PROT_READ, MAP_SHARED, mtxfd, 0);
    if (mutex == MAP_FAILED) {
        perror("mmap");
        printf("udp proxy: failed to map memory for mutex\n");
        exit(1);
    }
    close(mtxfd);
    // fd no longer needed and the memory is mapped
    pthread_mutex_init(mutex, 0);



    printf("udp proxy: waiting for connections...\n");

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(listenfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            c, sizeof c);
        printf("udp proxy: got connection from %s\n", c);

        if (!fork()) { // this is the child process
            close(listenfd); // child doesn't need the listener
            char* msg = NULL;
            char* replacedstr = NULL;
            int32_t recvstatus;
            int32_t expected;
            while (1) {
                recvstatus = recv(new_fd, &expected, sizeof expected, 0);
                expected = ntohl(expected);
                switch (recvstatus) {
                case -1:
                    perror("recv");
                    close(new_fd);
                    exit(1);
                case 0:
                    printf("udp proxy: connection from %s closed\n", c);
                    close(new_fd);
                    exit(0);
                }
                // get msg from client
                if (!(msg = realloc(msg, expected))) {
                    perror("realloc");
                    close(new_fd);
                    exit(1);
                }
                numbytes = recv(new_fd, msg, expected, 0);
                int32_t networkbytes = htonl(numbytes);
                switch (numbytes) {
                case -1:
                    perror("recv");
                    close(new_fd);
                    exit(1);
                case 0:
                    printf("udp proxy: client connection from %s closed\n", c);
                    close(new_fd);
                    exit(0);
                default:
                    pthread_mutex_lock(mutex);
                    if (sendto(servfd, msg, expected, 0,
                                    p->ai_addr, p->ai_addrlen) == -1) {
                        if (errno == EPIPE) {
                            printf("udp proxy: lost server connection to %s\n", s);
                        } else {
                            perror("send");
                        }
                        goto cleanup;
                    }
                }
                // get msg from server
                numbytes = recvfrom(servfd, msg, expected, 0,
                        p->ai_addr, &p->ai_addrlen);
                switch (numbytes) {
                case -1:
                    perror("recvfrom");
                    goto cleanup;
                case 0:
                    printf("udp proxy: server connection from %s closed\n", s);
                    goto cleanup;
                default:
                    pthread_mutex_unlock(mutex);
                    if (expected != numbytes) {
                        printf("udp proxy: lost bytes in communication with server\n");
                        goto cleanup;
                    }
                    expected = replacement(msg, expected, &replacedstr);
                    if (expected == -1) {
                        printf("udp proxy: character replacement failed\n");
                        close(new_fd);
                        exit(0);
                    }
                    // need to actually check this return value
                    networkbytes = htonl(expected);
                    if (send(new_fd, &networkbytes, sizeof networkbytes, MSG_NOSIGNAL) == -1) {
                        perror("send");
                        if (errno == EPIPE) {
                            printf("udp proxy: lost client connection to %s\n", c);
                        } else {
                            perror("send");
                        }
                        close(new_fd);
                        exit(0);
                    }
                    if (send(new_fd, replacedstr, expected, MSG_NOSIGNAL) == -1) {
                        if (errno == EPIPE) {
                            printf("udp proxy: lost client connection to %s\n", c);
                        } else {
                            perror("send");
                        }
                        close(new_fd);
                        exit(0);
                    }
                }

            }

cleanup:
            close(servfd);
            close(new_fd);
            free(msg);
            if (replacedstr) {
                free(replacedstr);
            }
            // there is no safe time to destroy the mutex
            // but it should be fine as the parent process
            // will need to continue to give it to its children
            // until the entire proxy is killed and the memory
            // will be reclaimed by the os
            pthread_mutex_unlock(mutex);
            munmap(mutex, sizeof(pthread_mutex_t));
            exit(1);
        }
        close(new_fd);  // parent doesn't need this
    }
    close(servfd);

    return 0;
}
