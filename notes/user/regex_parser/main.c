#include <stdio.h>
#include <stdlib.h>
#include "regex.h"

int
main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: regex PATTERN STRING\n");
        return 1;
    }
    return matchstr(argv[1], argv[2]);
}
