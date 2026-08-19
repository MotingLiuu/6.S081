#include "lexer.h"
#include <stdio.h>

int main(int argc, char **argv) {
    char *s1 = "abc|(a)?b*c?(d|e)f";
    TokenStream ts;
    if (lex(s1, &ts) != 0) {
        printf("lex error\n");
        return -1;
    }
    if (show_tokens(&ts) != 0) {
        printf("show_tokens error\n");
        return -1;
    }
    return 0;
}
