#include <stdlib.h>
#include <stdio.h>
#include "nfa.h"

int nfaid;

int show_nfa(NfaNode *nfa, int indent) {
    if (!nfa) {
        return -1;
    }
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
    switch (nfa->kind) {
        case NFA_SP:
            printf("[ID:%d]: %c, %c\n",nfa->id ,nfa->c1, nfa->c2);
            if (nfa->visited) {
                return 0;
            } else {
                nfa->visited = 1;
            }
            show_nfa(nfa->next1, indent + 1);
            show_nfa(nfa->next2, indent + 1);
            break;
        case NFA_NOR:
            printf("(ID:%d): %c\n",nfa->id ,nfa->c1);
            if (nfa->visited) {
                return 0;
            } else {
                nfa->visited = 1;
            }
            show_nfa(nfa->next1, indent + 1);
            break;
        case NFA_END:
            printf("{ID:%d}\n", nfa->id);
            break;
        default:
            exit(7);
    }
    return 0;
}

// Contract:
// 1. ast is not NULL
// 2. ast is a valid AST
int nfa(AstNode *ast, NfaNode *pre, DanNfa *danfa) {
    // defensive check
    if (!ast) {
        exit(7);
    }

    // create an empty NfaNode
    switch (ast->kind) {
        case AST_REG:
            break;
        case AST_ALT:
            break;
        case AST_CONCAT:
            break;
        case AST_REPEAT:
            break;
        case AST_ATOM:
            break;
    }

    return 0;
}




