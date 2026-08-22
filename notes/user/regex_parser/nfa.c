#include <stdlib.h>
#include <stdio.h>
#include "nfa.h"

int show_nfa(NfaNode *nfa, int indent) {
    if (!nfa) {
        return -1;
    }
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
    switch (nfa->kind) {
        case NFA_SP:
            printf("[ID:%d]: %c, %c\n",nfa->id ,nfa->split.c1, nfa->split.c2);
            if (nfa->visited) {
                return 0;
            } else {
                nfa->visited = 1;
            }
            show_nfa(nfa->split.next1, indent + 1);
            show_nfa(nfa->split.next2, indent + 1);
            break;
        case NFA_NOR:
            printf("(ID:%d): %c\n",nfa->id ,nfa->normal.c);
            if (nfa->visited) {
                return 0;
            } else {
                nfa->visited = 1;
            }
            show_nfa(nfa->normal.next, indent + 1);
            break;
        case NFA_END:
            printf("{ID:%d}\n", nfa->id);
            break;
        default:
            exit(7);
    }
    return 0;
}
