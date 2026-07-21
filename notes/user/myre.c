#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXSTACK 100

enum {
    Match = 256,
    Split = 257
};
typedef struct State State;
struct State {
    int c;
    State *out;
    State *out1;
    int lastlist;
};
State matchstate = { Match };
int nstate;

State *state(int c, State *out, State *out1) {
    State *s;

    nstate++;
    s = malloc(sizeof(*s));
    s->lastlist = 0;
    s->c = c;
    s->out = out;
    s->out1 = out1;
    return s;
}

typedef struct Frag Frag;
typedef union Ptrlist Ptrlist;
struct Frag {
    State *start;
    Ptrlist *out;
};

Frag frag(State *start, Ptrlist *out) {
    Frag n = {start, out};
    return n;
}

union Ptrlist {
    Ptrlist *next;
    State *s;
}; // Ptrlist is composed of a pointer to another Ptrlist or a State
  // ---
  // Ptrlist            Ptrlist             State s
  // *next(64bits) --> *s(64bits)  --> 
  //
  // or
  //
  // Ptrlist        State s
  // *s(State) -->  

Ptrlist *list1(State **outp) {
    Ptrlist *l;

    // outp         pointer to State    State
    // |||||||| ->  |||||||| ->     State s
    //
    // l (Ptrlist *) pointer to State   
    // |||||||| -> NULL(0)(*s) ->    .... 
    l = (Ptrlist *)outp;  //convert outp to pointer Ptrlist
    l->next = NULL;
    return l;
} // This function would convert outp to a Pointer to Ptrlist. And the original pointer of State that outp pointing to would be set to NULL. This would return a Pointer of Ptrlist whose next pointing to NULL.
  // Question: Why we use outp to create a pointer to Ptrlist? We can do the same thing without passing an outp into list1
  // Answer: see the example of '+'
  // This would conver outp to a pointer to Ptrlist, then save it to frag.

void patch(Ptrlist *l, State *s) {
    Ptrlist *next;

    for (; l; l=next) {
        next = l->next;
        l->s = s;
    }
} // patch s to all of the nodes in l

Ptrlist *append(Ptrlist *l1, Ptrlist *l2) {
    Ptrlist *old1;

    old1 = l1;
    while (l1->next)
        l1 = l1->next;
    l1->next = l2;
    return old1;
}

// highest priority: char, ()
// operator with priority 2 '+', '*', '?'
// operator with priority 1 '|'
State *re2nfa(char *re) {
    char *p = re;
    int lstack[MAXSTACK], cur_level = 1;
    memset(lstack, -1, sizeof(lstack));
    Frag fstack[MAXSTACK], e1, e2, e;
    int sp = 0;
    State *s;

    if (re == NULL) 
        return NULL;

    for (p=re; *p; p++) {
        switch (*p) {
            default:
                if (sp > MAXSTACK - 1) {
                    fprintf(stderr, "stack overflow\n");
                    exit(1);
                }
                s = state(*p, NULL, NULL);
                fstack[sp] = frag(s, list1(&s->out));
                lstack[sp] = cur_level;
                sp++;
                break;
            case '(':
                cur_level++;
                break;
            case ')':
                while (lstack[sp-1] == cur_level)
                    sp--;
                for (int i=1; lstack[sp+i] == cur_level || lstack[sp+i] == -cur_level; i++) {
                    if (lstack[sp+i] == -1) {
                        e1 = fstack[sp];
                        e2 = fstack[sp+(++i)];
                        s = state(Split, e1.start, e2.start);
                        fstack[sp] = frag(s, append(e1.out, e2.out));
                    } else {
                        e1 = fstack[sp];
                        e2 = fstack[sp+i];
                        patch(e1.out, e2.start);
                        fstack[sp] = frag(e1.start, e2.out);
                    }
                }
                lstack[sp++] = --cur_level;
                break;
            case '?':
                if (sp == 0) {
                    fprintf(stderr, "syntax error: ? not preceded by an unit");
                    exit(1);
                }
                e = fstack[sp-1];
                s = state(Split, e.start, NULL);
                fstack[sp-1] = frag(s, append(e.out, list1(&s->out1)));
                break;
            case '*':
                if (sp == 0) {
                    fprintf(stderr, "syntax error: * not preceded by an unit");
                    exit(1);
                }
                e = fstack[sp-1];
                s = state(Split, e.start, NULL);
                patch(e.out, s);
                fstack[sp-1] = frag(s, list1(&s->out1));
                break;
            case '+':
                if (sp == 0) {
                    fprintf(stderr, "syntax error: + not preceded by an unit");
                    exit(1);
                }
                e = fstack[sp-1];
                s = state(Split, e.start, NULL);
                patch(e.out, s);
                fstack[sp-1] = frag(e.start, list1(&s->out1));
                break;
            case '|':
                if (sp == 0) {
                    fprintf(stderr, "syntax error: | not preceded by an unit");
                    exit(1);
                }
                lstack[sp++] = -cur_level;
                break;
        }
    }   
    for (int i=1; i<sp; i++) {
        if (lstack[i] == -1) {
            e1 = fstack[0];
            e2 = fstack[++i];
            s = state(Split, e1.start, e2.start);
            fstack[0] = frag(s, append(e1.out, e2.out));
        } else {
            e1 = fstack[0];
            e2 = fstack[i];
            patch(e1.out, e2.start);
            fstack[0] = frag(e1.start, e2.out);
        }
    }
    patch(fstack[0].out, &matchstate);
    return fstack[0].start;
}

void *state_visted[MAXSTACK];
int visited_pointer = 0;

int visted_num(State* s) {
    for (int i=0; i<visited_pointer; i++) {
        if (state_visted[i] == s)
            return i;
    }
    return -1;
}

int show_nfa(State *s, int level) {
    if (s == NULL)
        return -1;
    for (int i=0; i<level; i++) {
        printf("    ");
    }
    if (s->c == 256) {
        if (visted_num(s) != -1) {
            printf("[N%d](M)\n", visted_num(s));
            return 0;
        }
        printf("[N%d](M)\n", visited_pointer);
        state_visted[visited_pointer++] = s;
    } else if (s->c == 257) {
        if (visted_num(s) != -1) {
            printf("[N%d](S)\n", visted_num(s));
            return 0;
        }
        printf("[N%d](S)\n", visited_pointer);
        state_visted[visited_pointer++] = s;
        show_nfa(s->out, level+1);
        show_nfa(s->out1, level+1);
    } else {
        if (visted_num(s) != -1) {
            printf("[N%d](%c)\n", visted_num(s), s->c);
            return 0;
        }
        printf("[N%d](%c)\n", visited_pointer, s->c);
        state_visted[visited_pointer++] = s;
        show_nfa(s->out, level+1);
    }
    return 0;
}


int main(int argc, char *argv[]) {
    char *re1 = "abc";
    char *re2 = "a?";
    char *re3 = "a*";
    char *re4 = "a+";
    char *re5 = "ea?b+c*d";
    char *re6 = "a|b";
    char *re7 = "a|b|c";
    char *re8 = "a|(bc)|d";
    char *re9 = "a|(b?c)|d";
    char *re10 = "a|(b*c)|d";
    State *s1 = re2nfa(re1);
    printf("s1:\n");
    show_nfa(s1, 0);
    visited_pointer = 0;
    State *s2 = re2nfa(re2);
    printf("s2:\n");
    show_nfa(s2, 0);
    visited_pointer = 0;
    State *s3 = re2nfa(re3);
    printf("s3:\n");
    show_nfa(s3, 0);
    visited_pointer = 0;
    State *s4 = re2nfa(re4);
    printf("s4:\n");
    show_nfa(s4, 0);
    visited_pointer = 0;
    State *s5 = re2nfa(re5);
    printf("s5:\n");
    show_nfa(s5, 0);
    visited_pointer = 0;
    State *s6 = re2nfa(re6);
    printf("s6:\n");
    show_nfa(s6, 0);
    visited_pointer = 0;
    State *s7 = re2nfa(re7);
    printf("s7:\n");
    show_nfa(s7, 0);
    visited_pointer = 0;
    State *s8 = re2nfa(re8);
    printf("s8:\n");
    show_nfa(s8, 0);
    visited_pointer = 0;
    State *s9 = re2nfa(re9);
    printf("s9:\n");
    show_nfa(s9, 0);
    visited_pointer = 0;
    State *s10 = re2nfa(re10);
    printf("s10:\n");
    show_nfa(s10, 0);
    return 0;
}










