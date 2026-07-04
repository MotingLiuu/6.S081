#include <stdio.h>
#include <stdlib.h>

#define MAXSTACK 100

int hanoi(int n, char from, char to, char aux);
int hanoi_stack(int n, char from, char to, char aux);

int main(int argc, char *argv[]){
    printf("Result of recursive version:");
    hanoi(3, 'A', 'B', 'C');
    printf("\nResult of stack version:");
    hanoi(3, 'A', 'B', 'C');
    printf("\n");
}

int hanoi(int n, char from, char to, char aux){
    if (n <= 0) return -1;
    if (n == 1) {
        printf("%c --> %c\n", from, to);
    } else {
        hanoi(n - 1, from, aux, to);
        printf("%c --> %c\n", from, to);
        hanoi(n - 1, aux, to, from);
    }
    return 0;
}

int hanoi_stack(int n, char from, char to, char aux){
    char sfrom[MAXSTACK], sto[MAXSTACK], sau[MAXSTACK];
    int sn[MAXSTACK];
    int stop = 0;
    sfrom[0] = from;
    sto[0] = to;
    sau[0] = aux;
    sn[0] = n;
    stop++;
    while (stop) {
        if (sn[stop - 1] == 1) {
            printf("%c --> %c\n", sfrom[stop - 1], sto[stop - 1]);
            stop--;
        } else {
            if (sn[stop] == 1) {
                stop--;
                sn[stop] = 1;
                printf("%c --> %c\n", sfrom[stop], sto[stop]);
                sfrom[stop] = sau[stop - 1];
                sto[stop] = sto[stop - 1];
                sau[stop] = sfrom[stop - 1];
                sn[stop] = sn[stop - 1] - 1;
                stop++;
            }
            sfrom[stop] = sfrom[stop -1];
            sto[stop] = sau[stop - 1];
            sau[stop] = sto[stop - 1];
            sn[stop] = sn[stop - 1] - 1;
            stop++;
        }
    }
    
    return 0;
}
