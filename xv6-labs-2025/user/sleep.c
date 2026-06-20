#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(2, "usage: sleep pattern [sleep (time in seconds)]\n");
    }
    int i = atoi(argv[1]);
    pause(i);
    exit(0);
}
