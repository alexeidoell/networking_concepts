// Alexei Doell cka067 11345642

#include <caesar.h>
#include <stdio.h>

int cipher(char *str, size_t len) {

    if (str == NULL || len == 0) {
        return -1;
    }

    char curr;

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

int main(void) {
    char teststr[11];
    strcpy(teststr, "Blah, blah");
    cipher(teststr, strlen(teststr));

    printf("%s\n", teststr);
}
