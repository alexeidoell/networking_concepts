// Alexei Doell cka067 11345642

#include <shared.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

int cipher(char *str, size_t len) {

    if (str == NULL || len == 0) {
        return -1;
    }

    unsigned char curr;

    for (size_t i = 0; i < len; ++i) {
        curr = str[i];
        if (isupper(curr)) {
            curr += OFFSET;
            if (curr > 'Z') {
                curr -= 26;
            }
        } else if (islower(curr)) {
            curr += OFFSET;
            if (curr > 'z') {
                curr -= 26;
            }
        }
        str[i] = curr;
    }
    return 0;
}

// takes the address of a pointer so length and newly allocated string
// can be returned (*result) must be null
// str param needs to be freed after by caller
ssize_t replacement(char* str, size_t len, char** result) {

    if (str == NULL || len == 0) {
        return -1;
    }

    char* outstr = malloc(len);
    if (outstr == NULL) {
        return -1;
    }
    unsigned int count = 0;
    unsigned char curr;
    // flag set when R is found
    bool flag = false;

    for (size_t i = 0; i < len; ++i) {
        curr = str[i];
        if (!flag) {
            if (curr == 'R') {
                flag = true;
            }
            outstr[i + (count << 1)] = curr;
        } else {
            if (curr == 'H') {
                // found pattern to replace
                outstr = realloc(outstr, len + 2 + (count << 1));
                if (outstr == NULL) {
                    return -1;
                }
                strncpy(outstr + i + (count << 1), "BEI", 4);
                count += 1;
                flag = false;
            } else {
                if (curr != 'R') {
                    flag = false;
                }
                outstr[i + (count << 1)] = curr;
            }

        }
    }

    *result = outstr;
    return len + (count << 1);
}


int recvloop(int fd, void* buf, size_t expected) {
    size_t readbytes = 0;
    int recvstatus;
    while (readbytes != expected) {
        recvstatus = recv(fd, (char*)buf + readbytes, expected, 0);
        if (recvstatus <= 0) {
            return recvstatus;
        }
        readbytes += recvstatus;
    }
    return readbytes;
}
