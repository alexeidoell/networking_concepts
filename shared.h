// Alexei Doell cka067 11345642

#include <netdb.h>

#define MAXLEN 100

struct fake_packet {
    int32_t sequence_num;
    long timestamp;
    char msg[MAXLEN];
};

void *get_in_addr(struct sockaddr *sa);
