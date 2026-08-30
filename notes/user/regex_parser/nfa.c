#include <stdlib.h>
#include <stdio.h>
#include "nfa.h"
#define NFA_ARENA_SIZE 1024

int nfaid;
struct NfaArena {
    NfaNode nodes[NFA_ARENA_SIZE];
    int p;
} nfa_arena;

NfaNode *new_nfa_node() {
    if (nfa_arena.p >= NFA_ARENA_SIZE) {
        return NULL;
    }
    return &(nfa_arena.nodes[nfa_arena.p++]);
}

void free_nfa_arena() {
    nfa_arena.p = 0;
}

struct DanNfaArena {
    DanNfa nodes[NFA_ARENA_SIZE];
    int p;
} dannfa_arena;

DanNfa *new_dannfa_node() {
    if (dannfa_arena.p >= NFA_ARENA_SIZE) {
        return NULL;
    }
    return &(dannfa_arena.nodes[dannfa_arena.p++]);
}

void free_dannfa_arena() {
    dannfa_arena.p = 0;
}

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

// Contract:
// 1. ast is not NULL
// 2. ast type is AST_ATOM
int nfa_atom(AstNode *ast, NfaNode **start, DanNfa **dang) {
    //defensive check
    if (!ast || ast->kind != AST_ATOM) {
        exit(7);
    }

    if (ast->atom.is_char) {
        NfaNode *node1 = new_nfa_node();
        node1->kind = NFA_NOR;
        node1->id = nfaid++;
        node1->visited = 0;
        node1->c1 = ast->atom.ch;
        node1->c2 = 0;
        node1->next1 = NULL;
        node1->next2 = NULL;

        *start = node1;

        DanNfa *nfanode = new_dannfa_node();
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

    if (nfa_atom(ast->repeat.atom, &start_atom, &dang_atom) == -1) {
        return -1;
    }

    NfaNode *node1 = new_nfa_node();
    DanNfa *nfanode = new_dannfa_node();

    if (!node1 || !nfanode) {
        return -1;
    }

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
            nfanode->dan = NULL;

            if (concat(nfanode, dang_atom) == -1) {
                return -1;
            };

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
                return -1;
            }
    
            *start = start_atom;
    
            nfanode->node = &(node1->next2);
            nfanode->dan = NULL;
    
            *dang = nfanode;       
    
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
    DanNfa *tmp_dang = NULL;
    NfaNode *tmp_start = NULL;
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
            new_node = new_nfa_node();
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

int
nfa(AstNode *ast, NfaNode **start)
{
    /* defensive check */
    if (!ast || ast->kind != AST_REG || !start) {
        exit(7);
    }

    nfaid = 0;

    /*
     * arena checkpoint
     */
    int nfa_mark = nfa_arena.p;
    int dannfa_mark = dannfa_arena.p;
    int id_mark = nfaid;

    *start = NULL;

    DanNfa *dang = NULL;

    NfaNode *end_node = new_nfa_node();
    if (!end_node) {
        goto fail;
    }

    end_node->kind = NFA_END;
    end_node->id = nfaid++;
    end_node->visited = 0;
    end_node->c1 = 0;
    end_node->c2 = 0;
    end_node->next1 = NULL;
    end_node->next2 = NULL;

    if (nfa_alt(ast->regex.alt, start, &dang) == -1) {
        goto fail;
    }

    if (connect(dang, end_node) == -1) {
        goto fail;
    }

    dannfa_arena.p = dannfa_mark;

    return 0;


fail:
    nfa_arena.p = nfa_mark;
    dannfa_arena.p = dannfa_mark;
    nfaid = id_mark;

    *start = NULL;

    return -1;
}

typedef struct MatchList MatchList;
struct MatchList {
    NfaNode *list[NFA_ARENA_SIZE];
    int count;
};

MatchList list1, list2;

int addstate(MatchList *list, NfaNode *node)
{
    if (!list) {
        exit(7);
    }

    if (!node) {
        return 0;
    }

    if (node->kind == NFA_SP) {
        if (addstate(list, node->next1) == -1) {
            return -1;
        }

        if (addstate(list, node->next2) == -1) {
            return -1;
        }

        return 0;
    }

    for (int i = 0; i < list->count; i++) {
        if (list->list[i] == node) {
            return 0;
        }
    }

    if (list->count >= NFA_ARENA_SIZE) {
        return -1;
    }

    list->list[list->count++] = node;

    return 0;
}

int step(MatchList *list1, MatchList *list2, char c)
{
    // defensive check
    if (!list1 || !list2 || c == 0) {
        exit(7);
    }

    list2->count = 0;

    for (int i = 0; i < list1->count; i++) {

        NfaNode *node = list1->list[i];

        if (!node) {
            exit(7);
        }

        switch (node->kind) {

        case NFA_NOR:
            if (node->c1 == c) {
                if (addstate(list2, node->next1) == -1) {
                    return -1;
                }
            }

            break;

        case NFA_END:
            break;

        case NFA_SP:
            exit(7);

        default:
            exit(7);
        }
    }

    return 0;
}

int match(NfaNode *start, char *str)
{
    if (!start || !str) {
        exit(7);
    }

    MatchList *clist = &list1;
    MatchList *nlist = &list2;

    clist->count = 0;
    nlist->count = 0;

    if (addstate(clist, start) == -1) {
        return -1;
    }

    for (int i = 0; str[i] != '\0'; i++) {

        if (step(clist, nlist, str[i]) == -1) {
            return -1;
        }

        MatchList *tmp = clist;
        clist = nlist;
        nlist = tmp;
    }

    for (int i = 0; i < clist->count; i++) {

        if (clist->list[i]->kind == NFA_END) {
            return 1;
        }
    }

    return 0;
}






