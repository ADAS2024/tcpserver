#include <stdio.h>
#include <ctype.h>


void encrypt(char *message, int shift) {
    for (int i = 0; message[i] != '\0'; i++) {
        if (isalpha(message[i])) {
            char base = isupper(message[i]) ? 'A' : 'a';
            message[i] = (message[i] - base + shift) % 26 + base;
        }
    }
}

void decrypt(char *message, int shift) {
    encrypt(message, 26 - shift);
}