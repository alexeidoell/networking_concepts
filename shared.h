// Alexei Doell cka067 11345642
#pragma once
#include <netdb.h>

#define MAXLEN 5

struct fake_packet {
    int32_t sequence_num;
    long timestamp;
    char msg[MAXLEN];
};

void *get_in_addr(struct sockaddr *sa);
