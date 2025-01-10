// Alexei Doell cka067 11345642

#include <caesar.h>
#include <ctype.h>
#include <stdio.h>

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
