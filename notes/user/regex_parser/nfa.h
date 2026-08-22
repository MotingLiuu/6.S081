#ifndef NFA_H
#define NFA_H

typedef enum {
    NFA_SP,
    NFA_NOR,
    NFA_END,
} NfaKind;

typedef struct NfaNode NfaNode;

struct NfaNode {
    NfaKind kind;
    int id;
    int visited;
    union {
        struct {
            char c;
            NfaNode *next;
        } normal;
        struct {
            char c1, c2;
            NfaNode *next1, *next2;
        } split;
    };
};

int show_nfa(NfaNode *nfa, int indent);


#endif
