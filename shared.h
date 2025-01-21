// Alexei Doell cka067 11345642

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#define OFFSET 15
#define MAXLEN 256

int cipher(char *str, size_t len);
int replacement(char* str, size_t len, char** result);
int recvloop(int fd, void* buf, size_t expected);
void *get_in_addr(struct sockaddr *sa);
void sigchld_handler(int s __attribute__((unused)));
