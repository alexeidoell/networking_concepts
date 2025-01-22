// Alexei Doell cka067 11345642

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <shared.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#define PROXYPORT "34921"

#define BACKLOG 10   // how many pending connections queue will hold

int main(int argc, char *argv[])
{
    int servfd, listenfd, new_fd, mtxfd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    pthread_mutex_t *mutex;
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN], c[INET6_ADDRSTRLEN];
    int rv;
    int exitcode = 0;


    if (argc != 3) {
        fprintf(stderr,"usage: tcp_proxy hostname server_port\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        freeaddrinfo(servinfo);
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((servfd = socket(p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
            perror("tcp proxy: socket");
            continue;
        }

        if (connect(servfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(servfd);
            perror("tcp proxy: connect");
            continue;
        }

        break;
    }


    if (p == NULL) {
        fprintf(stderr, "tcp proxy: failed to connect\n");
        freeaddrinfo(servinfo);
        return 1;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("tcp proxy: connecting to server on %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PROXYPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        freeaddrinfo(servinfo);
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((listenfd = socket(p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
            perror("tcp proxy: socket");
            continue;
        }

        if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                    sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(listenfd);
            perror("tcp proxy: bind");
            continue;
        }

        break;
    }


    if (p == NULL)  {
        fprintf(stderr, "tcp proxy: failed to bind\n");
        freeaddrinfo(servinfo);
        exit(1);
    }

    if (listen(listenfd, BACKLOG) == -1) {
        perror("listen");
        freeaddrinfo(servinfo);
        exit(1);
    }

    freeaddrinfo(servinfo);

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    mtxfd = shm_open("cka067 tcp mutex", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (mtxfd == -1) {
        perror("shm_open");
        printf("tcp proxy: shm_open failed to create memory for mutex\n");
        exit(1);
    }
    if (ftruncate(mtxfd, sizeof(pthread_mutex_t)) == -1) {
        perror("ftruncate");
        printf("tcp proxy: ftruncate failed to create memory for mutex\n");
        exit(1);
    }
    mutex = mmap(NULL, sizeof(pthread_mutex_t), PROT_WRITE | PROT_READ, MAP_SHARED, mtxfd, 0);
    if (mutex == MAP_FAILED) {
        perror("mmap");
        printf("tcp proxy: failed to map memory for mutex\n");
        exit(1);
    }
    close(mtxfd);
    // fd no longer needed and the memory is mapped
    pthread_mutex_init(mutex, 0);

    printf("tcp proxy: waiting for connections on %s\n", PROXYPORT);

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
        printf("tcp proxy: got connection from %s\n", c);

        if (!fork()) { // this is the child process
            close(listenfd); // child doesn't need the listener
            char* msg = NULL;
            char* replacedstr = NULL;
            int32_t expected;
            while (1) {
                switch (recvloop(new_fd, &expected, sizeof expected)) {
                    case -1:
                        perror("recv");
                        exitcode = 1;
                        goto cleanup;
                    case 0:
                        printf("tcp proxy: connection from %s closed\n", c);
                        goto cleanup;
                }
                expected = ntohl(expected);
                // get msg from client
                if (!(msg = realloc(msg, expected + 1))) {
                    perror("realloc");
                        exitcode = 1;
                        goto cleanup;
                }
                switch (recvloop(new_fd, msg, expected)) {
                    case -1:
                        perror("recv");
                        exitcode = 1;
                        goto cleanup;
                    case 0:
                        printf("tcp proxy: connection from %s closed\n", c);
                        goto cleanup;
                }
                int32_t networkbytes = htonl(expected);
                // mutex on sends to server to ensure the two messages are
                // not potentially interleaved with other child processes'
                // sends
                pthread_mutex_lock(mutex);
                if (send(servfd, &networkbytes, sizeof networkbytes, MSG_NOSIGNAL) == -1) {
                    if (errno == EPIPE) {
                        printf("tcp proxy: lost server connection to %s\n", s);
                        exitcode = -1;
                    } else {
                        perror("send");
                        exitcode = 1;
                    }
                    pthread_mutex_unlock(mutex);
                    goto cleanup;
                }
                if (send(servfd, msg, expected, MSG_NOSIGNAL) == -1) {
                    if (errno == EPIPE) {
                        printf("tcp proxy: lost server connection to %s\n", s);
                        exitcode = -1;
                    } else {
                        perror("send");
                        exitcode = 1;
                    }
                    pthread_mutex_unlock(mutex);
                    goto cleanup;
                }
                switch (recvloop(servfd, &expected, sizeof expected)) {
                    case -1:
                        perror("recv");
                        exitcode = 1;
                        goto cleanup;
                    case 0:
                        printf("tcp proxy: server connection from %s closed\n", s);
                        exitcode = -1;
                        goto cleanup;
                }
                expected = ntohl(expected);
                if (!(msg = realloc(msg, expected + 1))) {
                    perror("realloc");
                    exitcode = 1;
                    goto cleanup;
                }
                switch (recvloop(servfd, msg, expected)) {
                    case -1:
                        perror("recv");
                        exitcode = 1;
                        goto cleanup;
                    case 0:
                        printf("tcp proxy: server connection from %s closed\n", s);
                        exitcode = -1;
                        goto cleanup;
                }
                pthread_mutex_unlock(mutex);
                expected = replacement(msg, expected, &replacedstr);
                if (expected == -1) {
                    printf("tcp proxy: character replacement failed\n");
                    exitcode = 1;
                    goto cleanup;
                }
                // need to actually check this return value
                networkbytes = htonl(expected);
                if (send(new_fd, &networkbytes, sizeof networkbytes, MSG_NOSIGNAL) == -1) {
                    perror("send");
                    if (errno == EPIPE) {
                        printf("tcp proxy: lost client connection to %s\n", c);
                    } else {
                        perror("send");
                        exitcode = 1;
                    }
                    goto cleanup;
                }
                if (send(new_fd, replacedstr, expected, MSG_NOSIGNAL) == -1) {
                    if (errno == EPIPE) {
                        printf("tcp proxy: lost client connection to %s\n", c);
                    } else {
                        perror("send");
                        exitcode = 1;
                    }
                    goto cleanup;
                }

            }

cleanup:
            close(servfd);
            close(new_fd);
            free(msg);
            if (replacedstr) {
                free(replacedstr);
            }
            munmap(mutex, sizeof(pthread_mutex_t));
            if (exitcode == -1) {
                // for some reason if i don't put \n it doesn't print this line
                // but there is still an empty line :(
                printf("tcp proxy: exiting due to loss of connection to server\n");
                // can destroy the mutex because im planning on killing the
                // entire proxy due to its loss of connection to the tcp server
                // but im not sure if it even matters at all because all the processes
                // under the proxy will be killed and the memory will be reclaimed
                // by the os so ill just unmap to avoid the ub that comes from
                // potentially destroying a locked mutex
                kill(0, SIGINT);
            } else {
                // if the issue has nothing to do with the server itself
                // ill just exit this singular child process gracefully
                exit(exitcode);
            }

        }
        close(new_fd);  // parent doesn't need this but also this will probably
                        // never actually ever be called
    }
    return 0;
}
