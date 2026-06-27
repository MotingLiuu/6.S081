#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(2, "usage: sixfive pattern [sixfive sixfive.txt]\n");
        exit(1);
    }
    char *separators = "-\r\t\n./";
}