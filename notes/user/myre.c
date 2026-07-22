#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
char opstack[MAXSTACK];
Frag fragstack[MAXSTACK];
int opstack_pointer = 0, fragstack_pointer = 0;

int pushop(char op) {
    if (opstack_pointer == MAXSTACK) {
        // printf("DEBUG: opstack overflow, op:%c\n", op);
        return -1;
    }
    opstack[opstack_pointer++] = op;
    return 0;
}

int popop() {
    if (opstack_pointer == 0) {
        // printf("DEBUG: opstack underflow\n");
        return -1;
    }
    return opstack[--opstack_pointer];
}

int checkop() {
    if (opstack_pointer == 0) {
        // printf("DEBUG: opstack underflow\n");
        return -1;
    }
    return opstack[opstack_pointer-1];
}

int pushfrag(Frag f) {
    if (fragstack_pointer == MAXSTACK) {
        // printf("DEBUG: fragstack overflow\n");
        return -1;
    }
    fragstack[fragstack_pointer++] = f;
    return 0;
}

int popfrag(Frag *f) {
    if (fragstack_pointer == 0) {
        // printf("DEBUG: fragstack underflow\n");
        return -1;
    }
    *f = fragstack[--fragstack_pointer];
    return 0;
}

static int priority(char op) {
    switch (op) {
        case '|':
            return 1;
        case '.':
            return 2;
        default:
            return 0;
    }
}

int re2nfa(char *re, State **start_node) {
    int opstacktop;
    Frag e1, e2, e;
    State *s;
    char *p = re;
    for (; *p; p++) {
        switch (*p) {
            default:
                if (!isalpha(*p))
                    return -1;
                s = state(*p, NULL, NULL );
                if (pushfrag(frag(s, list1(&s->out))) == -1)
                    return -1;
                if (checkop() != -1) {
                    if (priority(checkop()) >= priority('.')) {
                        opstacktop = popop();
                        if (popfrag(&e2) == -1)
                            return -1;
                        if (popfrag(&e1) == -1)
                            return -1;
                        switch (opstacktop) {
                            default:
                                return -1;
                                break;
                            case '.':
                                patch(e1.out, e2.start);
                                pushfrag(frag(e1.start, e2.out));
                                break;
                        }
                    }
                }
                if (pushop('.') == -1)
                    return -1;
                break;
            case '?':
                if (popfrag(&e) == -1)
                    return -1;
                s = state(Split, e.start, NULL);
                if (pushfrag(frag(s, append(list1(&s->out1), e.out))) == -1)
                    return -1;
                break;
            case '*':
                if (popfrag(&e) == -1)
                    return -1;
                s = state(Split, e.start, NULL);
                patch(e.out, s);
                if (pushfrag(frag(s, list1(&s->out1))) == -1)
                    return -1;
                break;
            case '+':
                if (popfrag(&e) == -1)
                    return -1;
                s = state(Split, e.start, NULL);
                patch(e.out, s);
                if (pushfrag(frag(s, list1(&s->out1))) == -1)
                    return -1;
                break;
            case '|':
                if (popop() == -1)
                    return -1;
                if (checkop() != -1 ||priority(checkop()) >= priority('|')) {
                    opstacktop = popop();
                    switch (opstacktop) {
                        default:
                            return -1;
                            break;
                        case '.':
                            popfrag(&e1);
                            popfrag(&e2);
                            pushfrag(frag(e1.start, append(e1.out, e2.out)));
                            break;
                        case '|':
                            if (popfrag(&e2) == -1)
                                return -1;
                            if (popfrag(&e1) == -1)
                                return -1;
                            s = state(Split, e1.start, e2.start);
                            pushfrag(frag(s, append(e1.out, e2.out)));
                            break;
                    }
                }
                if (pushop('|') == -1)
                    return -1;
                break;
            case '(':
                if (pushop('(') == -1)
                    return -1;
                break;
            case ')':
                if (popop() == -1)
                    return -1;
                while (checkop() != '(' && checkop() != -1) {
                    opstacktop = popop();
                    if (popfrag(&e2) == -1)
                        return -1;
                    if (popfrag(&e1) == -1)
                        return -1;
                    switch (opstacktop) {
                        default:
                            return -1;
                            break;
                        case '.':
                            if (popfrag(&e2) == -1)
                                return -1;
                            if (popfrag(&e1) == -1)
                                return -1;
                            patch(e1.out, e2.start);
                            pushfrag(frag(e1.start, e2.out));
                            break;
                        case '|':
                            if (popfrag(&e2) == -1)
                                return -1;
                            if (popfrag(&e1) == -1)
                                return -1;
                            s = state(Split, e1.start, e2.start);
                            pushfrag(frag(s, append(e1.out, e2.out)));
                            break;
                    }
                }
                if ((opstacktop = popop()) == -1 || opstacktop != '(')
                    return -1;
                if (pushop('.') == -1)
                    return -1;
                break;
        }
    }
    if ((opstacktop = popop()) == -1 || opstacktop != '.')
        return -1;
    while ((opstacktop = popop()) != -1) {
        switch (opstacktop) {
            default:
                return -1;
                break;
            case '.':
                if (popfrag(&e2) == -1)
                    return -1;
                if (popfrag(&e1) == -1)
                    return -1;
                patch(e1.out, e2.start);
                pushfrag(frag(e1.start, e2.out));
                break;
            case '|':
                if (popfrag(&e2) == -1)
                    return -1;
                if (popfrag(&e1) == -1)
                    return -1;
                s = state(Split, e1.start, e2.start);
                pushfrag(frag(s, append(e1.out, e2.out)));
                break;
        }
    }
    if (popfrag(&e) == -1)
        return -1;
    patch(e.out, &matchstate);
    *start_node = e.start;
    if (fragstack_pointer != 0)
        return -1;
    return 0;
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
    char *re8 = "(ab)";
    char *re9 = "a|(b?c)|d";
    char *re10 = "a|(b*c)|d";
    char *re11 = "()";
    char *re12 = "|";
    char *re13 = ".";
    char *re14 = "ab|";
    char *re15 = "(a|";

    State *s1 = NULL;
    re2nfa(re1, &s1);
    printf("s1:\n");
    show_nfa(s1, 0);
    visited_pointer = 0;

    State *s2 = NULL;
    re2nfa(re2, &s2);
    printf("s2:\n");
    show_nfa(s2, 0);
    visited_pointer = 0;

    State *s3 = NULL;
    re2nfa(re3, &s3);
    printf("s3:\n");
    show_nfa(s3, 0);
    visited_pointer = 0;

    State *s4 = NULL;
    re2nfa(re4, &s4);
    printf("s4:\n");
    show_nfa(s4, 0);
    visited_pointer = 0;

    State *s5 = NULL;
    re2nfa(re5, &s5);
    printf("s5:\n");
    show_nfa(s5, 0);
    visited_pointer = 0;

    State *s6 = NULL;
    re2nfa(re6, &s6);
    printf("s6:\n");
    show_nfa(s6, 0);
    visited_pointer = 0;

    State *s7 = NULL;
    re2nfa(re7, &s7);
    printf("s7:\n");
    show_nfa(s7, 0);
    visited_pointer = 0; 

    State *s8 = NULL;
    re2nfa(re8, &s8);
    printf("s8:\n");
    show_nfa(s8, 0);
    visited_pointer = 0;

    State *s9 = NULL;
    re2nfa(re9, &s9);
    printf("s9:\n");
    show_nfa(s9, 0);
    visited_pointer = 0;

    State *s10 = NULL;
    re2nfa(re10, &s10);
    printf("s10:\n");
    show_nfa(s10, 0);
    visited_pointer = 0;

    State *s11 = NULL;
    if (re2nfa(re11, &s11) == -1)
        printf("syntax error\n");
    printf("s11:\n");
    show_nfa(s11, 0);
    visited_pointer = 0;

    State *s12 = NULL;
    if (re2nfa(re12, &s12) == -1)
        printf("syntax error\n");
    printf("s12:\n");
    show_nfa(s12, 0);
    visited_pointer = 0;

    State *s13 = NULL;
    if (re2nfa(re13, &s13) == -1)
        printf("syntax error\n");
    printf("s13:\n");
    show_nfa(s13, 0);
    visited_pointer = 0;

    State *s14 = NULL;
    if (re2nfa(re14, &s14) == -1)
        printf("syntax error\n");
    printf("s14:\n");
    show_nfa(s14, 0);
    visited_pointer = 0;

    State *s15 = NULL;
    if (re2nfa(re15, &s15) == -1)
        printf("syntax error\n");
    printf("s15:\n");
    show_nfa(s15, 0);
    visited_pointer = 0;

    return 0;
}










