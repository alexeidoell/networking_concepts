// Alexei Doell cka067 11345642

#include <stdio.h>
#define OFFSET 15

int cipher(char *str, size_t len);
ssize_t replacement(char* str, size_t len, char** result);
int recvloop(int fd, void* buf, size_t expected);
