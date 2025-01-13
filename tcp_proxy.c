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

#define SERVERPORT "34920"  // the port users will be connecting to
#define PROXYPORT "34921"

#define BACKLOG 10   // how many pending connections queue will hold

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
    int servfd, listenfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN], c[INET6_ADDRSTRLEN];
    int rv;
    ssize_t num_bytes;


    if (argc != 2) {
        fprintf(stderr,"usage: tcp_proxy hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], SERVERPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
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
        return 2;
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

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "tcp proxy: failed to bind\n");
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

    printf("tcp proxy: waiting for connections...\n");

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
                    printf("tcp proxy: connection from %s closed\n", c);
                    close(new_fd);
                    exit(0);
                }
                // get msg from client
                if (!(msg = realloc(msg, expected))) {
                    perror("realloc");
                    close(new_fd);
                    exit(1);
                }
                num_bytes = recv(new_fd, msg, expected, 0);
                int32_t networkbytes = htonl(num_bytes);
                switch (num_bytes) {
                case -1:
                    perror("recv");
                    close(new_fd);
                    exit(1);
                case 0:
                    printf("tcp proxy: client connection from %s closed\n", c);
                    close(new_fd);
                    exit(0);
                default:
                    if (send(servfd, &networkbytes, sizeof networkbytes, MSG_NOSIGNAL) == -1) {
                        if (errno == EPIPE) {
                            printf("tcp proxy: lost server connection to %s\n", s);
                        } else {
                            perror("send");
                        }
                        goto cleanup;
                    }
                    if (send(servfd, msg, num_bytes, MSG_NOSIGNAL) == -1) {
                        if (errno == EPIPE) {
                            printf("tcp proxy: lost server connection to %s\n", s);
                        } else {
                            perror("send");
                        }
                        goto cleanup;
                    }
                }
                recvstatus = recv(servfd, &expected, sizeof expected, 0);
                expected = ntohl(expected);
                switch (recvstatus) {
                case -1:
                    perror("recv");
                    goto cleanup;
                case 0:
                    printf("tcp proxy: server connection from %s closed\n", s);
                    goto cleanup;
                }
                // get msg from server
                num_bytes = recv(servfd, msg, expected, 0);
                switch (num_bytes) {
                case -1:
                    perror("recv");
                    goto cleanup;
                case 0:
                    printf("tcp proxy: server connection from %s closed\n", s);
                    goto cleanup;
                default:
                    networkbytes = htonl(networkbytes);
                    if (send(new_fd, &networkbytes, sizeof networkbytes, MSG_NOSIGNAL) == -1) {
                        perror("send");
                        if (errno == EPIPE) {
                            printf("tcp proxy: lost client connection to %s\n", c);
                        } else {
                            perror("send");
                        }
                        close(new_fd);
                        exit(0);
                    }
                    if (send(new_fd, msg, num_bytes, MSG_NOSIGNAL) == -1) {
                        if (errno == EPIPE) {
                            printf("tcp proxy: lost client connection to %s\n", c);
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
            // for some reason if i don't put \n it doesn't print this line
            // but there is still an empty line :(
            printf("tcp proxy: exiting due to loss of connection to server\n");
            kill(0, SIGINT);

        }
        close(new_fd);  // parent doesn't need this
    }
    close(servfd);

    return 0;
}
