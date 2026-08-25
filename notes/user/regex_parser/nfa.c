#include <stdlib.h>
#include <stdio.h>
#include "nfa.h"

int nfaid;

int reset_nfavisited(NfaNode *nfa) {
    if (!nfa || nfa->visited == 0) {
        return 0;
    }
    switch (nfa->kind) {
        case NFA_SP:
            nfa->visited = 0;
            reset_nfavisited(nfa->next1);
            reset_nfavisited(nfa->next2);
            break;
        case NFA_NOR:
            nfa->visited = 0;
            reset_nfavisited(nfa->next1);
            break;
        case NFA_END:
            nfa->visited = 0;
            break;
        default:
            exit(7);
    }
    return 0;
}

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
    if (indent == 0) {
        reset_nfavisited(nfa);
    }
    return 0;
}

int concat(DanNfa *list1, DanNfa *list2) {
    //defensive check
    if (!list1 || !list2) {
        exit(7);
    }
    while (list1->dan) {
        list1 = list1->dan;
    }
    list1->dan = list2;
    return 0;
}

int connect(DanNfa *list, NfaNode *node) {
    if (!list) {
        exit(7);
    }
    while (list) {
        *(list->node) = node;
        list = list->dan;
    }
    return 0;
}

void show_dang(DanNfa *dang) {
    while (dang) {
        printf("node pointer of dang: %p\n", *(dang->node));
        dang = dang->dan;
    }
}

int free_dannfat(DanNfa *dang) {
    DanNfa *tmp;
    while (dang) {
        tmp = dang->dan;
        free(dang);
        dang = tmp;
    }
    return 0;
}

// Contract:
// 1. ast is not NULL
// 2. ast type is AST_ATOM
int nfa_atom(AstNode *ast, NfaNode **start, DanNfa **dang) {
    //defensive check
    if (!ast || ast->kind != AST_ATOM) {
        exit(7);
    }

    if (ast->atom.is_char) {
        NfaNode *node1 = malloc(sizeof(NfaNode));
        node1->kind = NFA_NOR;
        node1->id = nfaid++;
        node1->visited = 0;
        node1->c1 = ast->atom.ch;
        node1->c2 = 0;
        node1->next1 = NULL;
        node1->next2 = NULL;

        *start = node1;

        DanNfa *nfanode = malloc(sizeof(DanNfa));
        nfanode->node = &(node1->next1);
        nfanode->dan = NULL;

        *dang = nfanode;
    } else {
        if (nfa_alt(ast->atom.alt, start, dang) == -1) {
            return -1;
        }
    }
    return 0;
}

int nfa_repeat(AstNode *ast, NfaNode **start, DanNfa **dang) {
    //defensive check
    if (!ast || ast->kind != AST_REPEAT) {
        exit(7);
    }
    // 1. construct the nfa of atom
    NfaNode *start_atom;
    DanNfa *dang_atom;

    nfa_atom(ast->repeat.atom, &start_atom, &dang_atom);

    NfaNode *node1 = malloc(sizeof(NfaNode));
    DanNfa *nfanode = malloc(sizeof(DanNfa));
    switch (ast->repeat.qkind) {
        case '*':
            node1->kind = NFA_SP;
            node1->id = nfaid++;
            node1->visited = 0;
            node1->c1 = 0;
            node1->c2 = 0;
            node1->next1 = NULL;
            node1->next2 = NULL;

            node1->next1 = start_atom;
            if (connect(dang_atom, node1) == -1) {
                return -1;
            }
            
            *start = node1;

            nfanode->node = &(node1->next2);
            nfanode->dan = NULL;

            *dang = nfanode;

            break;
        case '?':
            node1->kind = NFA_SP;
            node1->id = nfaid++;
            node1->visited = 0;
            node1->c1 = 0;
            node1->c2 = 0;
            node1->next1 = NULL;
            node1->next2 = NULL;

            node1->next1 = start_atom;

            *start = node1;

            nfanode->node = &(node1->next2);
            concat(nfanode, dang_atom);

            *dang = nfanode;

            break;
        case '+':
            node1->kind = NFA_SP;
            node1->id = nfaid++;
            node1->visited = 0;
            node1->c1 = 0;
            node1->c2 = 0;
            node1->next1 = NULL;
            node1->next2 = NULL;
            node1->next1 = start_atom;
            node1->next2 = NULL;

            if (connect(dang_atom, node1) == -1) {
                free_dannfat(dang_atom);
                return -1;
            }
            free_dannfat(dang_atom);

            *start = start_atom;

            nfanode->node = &(node1->next2);
            nfanode->dan = NULL;

            break;
        default:
            *start = start_atom;
            *dang = dang_atom;
            break;
    }

    return 0;
}

int nfa_concat(AstNode *ast, NfaNode **start, DanNfa **dang) {
    //defensive check
    if (!ast || ast->kind != AST_CONCAT) {
        exit(7);
    }
    DanNfa *tmp_dang;
    NfaNode *tmp_start;
    for (int i = 0; i < ast->concat.count; i++) {
        if (i == 0) {
            nfa_repeat(ast->concat.repeat[i], start, dang);
        } else {
            nfa_repeat(ast->concat.repeat[i], &tmp_start, &tmp_dang);
            if (connect(*dang, tmp_start) == -1) {
                return -1;
            }
            *dang = tmp_dang;
        }
    }
    return 0;
}

int nfa_alt(AstNode *ast, NfaNode **start, DanNfa **dang) {
    //defensive check
    if (!ast || ast->kind != AST_ALT) {
        exit(7);
    }
    DanNfa *tmp_dang;
    NfaNode *tmp_start;
    for (int i = 0; i < ast->alt.count; i++) {
        if (i == 0) {
            if (nfa_concat(ast->alt.concat[i], start, dang) == -1) {
                return -1;
            }
        } else {
            NfaNode *new_node;
            new_node = malloc(sizeof(NfaNode));
            new_node->kind = NFA_SP;
            new_node->id = nfaid++;
            new_node->visited = 0;
            new_node->c1 = 0;
            new_node->c2 = 0;
            new_node->next1 = NULL;
            new_node->next2 = NULL;

            new_node->next1 = *start;
            *start = new_node;

            if (nfa_concat(ast->alt.concat[i], &tmp_start, &tmp_dang) == -1) {
                return -1;
            }

            new_node->next2 = tmp_start;
            if (concat(*dang, tmp_dang) == -1) {
                return -1;
            }
        }
    }
    return 0;
}

int nfa(AstNode *ast, NfaNode **start) {
    //defensive check
    if (!ast || ast->kind != AST_REG) {
        exit(7);
    }
    DanNfa *dang;
    NfaNode *end_node = malloc(sizeof(NfaNode));
    end_node->kind = NFA_END;
    end_node->id = nfaid++;
    end_node->visited = 0;
    end_node->c1 = 0;
    end_node->c2 = 0;
    end_node->next1 = NULL;
    end_node->next2 = NULL;
    if (nfa_alt(ast->regex.alt, start, &dang) == -1) {
        return -1;
    }
    if (connect(dang, end_node) == -1) {
        return -1;
    }
    return 0;
}






