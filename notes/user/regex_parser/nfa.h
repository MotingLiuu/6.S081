#ifndef NFA_H
#define NFA_H
#include "ast.h"

typedef enum {
    NFA_SP,
    NFA_NOR,
    NFA_END,
} NfaKind;

typedef struct NfaNode NfaNode;
typedef struct DanNfa DanNfa;

struct NfaNode {
    NfaKind kind;
    int id;
    int visited;
    char c1, c2;
    NfaNode *next1, *next2;
};

struct DanNfa {
    NfaNode **node;
    DanNfa *dan;
};

int append(DanNfa *list, DanNfa *node);
int show_nfa(NfaNode *nfa, int indent);
int nfa(AstNode *ast, NfaNode **start);
int nfa_alt(AstNode *ast, NfaNode **start, DanNfa **dang);
int nfa_atom(AstNode *ast, NfaNode **start, DanNfa **dang);

#endif
