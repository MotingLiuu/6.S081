#include <stdio.h>
#include <stdlib.h>
#include "nfa.h"

int main(int argc, char *argv[]) {

    // test1
    NfaNode *nfa1 = malloc(sizeof(NfaNode));
    nfa1->kind = NFA_NOR;
    nfa1->id = 0;
    nfa1->visited = 0;
    nfa1->normal.c = 'a';
    nfa1->normal.next = NULL;

    NfaNode *nfa2 = malloc(sizeof(NfaNode));
    nfa2->kind = NFA_SP;
    nfa2->id = 1;
    nfa2->visited = 0;
    nfa2->split.c1 = 'b';
    nfa2->split.c2 = 'c';
    nfa2->split.next1 = NULL;
    nfa2->split.next2 = NULL;

    nfa1->normal.next = nfa2;

    NfaNode *nfa3 = malloc(sizeof(NfaNode));
    nfa3->kind = NFA_END;
    nfa3->id = 2;
    nfa3->visited = 0;
    
    nfa2->split.next1 = nfa3;
    nfa2->split.next2 = nfa3;

    show_nfa(nfa1, 0);

    // test2
    NfaNode *nfa21 = malloc(sizeof(NfaNode)); 
    nfa21->kind = NFA_SP;
    nfa21->id = 0;
    nfa21->visited = 0;
    nfa21->split.c1 = 'a';
    nfa21->split.c2 = 'c';
    nfa21->split.next1 = nfa21;
    nfa21->split.next2 = NULL;

    NfaNode *nfa22 = malloc(sizeof(NfaNode));
    nfa22->kind = NFA_NOR;
    nfa22->id = 1;
    nfa22->visited = 0;
    nfa22->normal.c = 'b';
    nfa22->normal.next = NULL;

    nfa21->split.next2 = nfa22;

    NfaNode *nfa23 = malloc(sizeof(NfaNode));
    nfa23->kind = NFA_END;
    nfa23->id = 2;
    nfa23->visited = 0;

    nfa22->normal.next = nfa23;

    show_nfa(nfa21, 0);

    return 0;
}
