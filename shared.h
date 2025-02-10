// Alexei Doell cka067 11345642
#pragma once
#include <stdint.h>

#define MAXLEN 100

struct fake_packet {
    int32_t sequence_num;
    long timestamp;
    char msg[MAXLEN];
};

