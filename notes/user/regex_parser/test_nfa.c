#include <stdio.h>
#include <stdlib.h>
#include "nfa.h"
#include "parser.h"
#include "lexer.h"

int main(int argc, char *argv[]) {

    /* test1
    NfaNode *nfa1 = malloc(sizeof(NfaNode));
    nfa1->kind = NFA_NOR;
    nfa1->id = 0;
    nfa1->visited = 0;
    nfa1->c1 = 'a';
    nfa1->next1 = NULL;

    NfaNode *nfa2 = malloc(sizeof(NfaNode));
    nfa2->kind = NFA_SP;
    nfa2->id = 1;
    nfa2->visited = 0;
    nfa2->c1 = 'b';
    nfa2->c2 = 'c';
    nfa2->next1 = NULL;
    nfa2->next2 = NULL;

    nfa1->next1 = nfa2;

    NfaNode *nfa3 = malloc(sizeof(NfaNode));
    nfa3->kind = NFA_END;
    nfa3->id = 2;
    nfa3->visited = 0;
    
    nfa2->next1 = nfa3;
    nfa2->next2 = nfa3;

    show_nfa(nfa1, 0);

    // test2
    NfaNode *nfa21 = malloc(sizeof(NfaNode)); 
    nfa21->kind = NFA_SP;
    nfa21->id = 0;
    nfa21->visited = 0;
    nfa21->c1 = 'a';
    nfa21->c2 = 'c';
    nfa21->next1 = nfa21;
    nfa21->next2 = NULL;

    NfaNode *nfa22 = malloc(sizeof(NfaNode));
    nfa22->kind = NFA_NOR;
    nfa22->id = 1;
    nfa22->visited = 0;
    nfa22->c1 = 'b';
    nfa22->next1 = NULL;

    nfa21->next2 = nfa22;

    NfaNode *nfa23 = malloc(sizeof(NfaNode));
    nfa23->kind = NFA_END;
    nfa23->id = 2;
    nfa23->visited = 0;

    nfa22->next1 = nfa23;

    show_nfa(nfa21, 0);
    */

    //test construction of NFA
    char *regex1 = "ab";

    TokenStream ts1;
    lex(regex1, &ts1);

    Parser p1;
    p1.ts = ts1;
    p1.pos = 0;
    AstNode *result1;
    parse(&p1, &result1);
    show_ast(result1, 0);

    NfaNode *start_node1;
    nfa(result1, &start_node1);
    show_nfa(start_node1, 0);


    char *regex2 = "a(b|c*d)";

    TokenStream ts2;
    lex(regex2, &ts2);

    Parser p2;
    p2.ts = ts2;
    p2.pos = 0;
    AstNode *result2;
    parse(&p2, &result2);
    show_ast(result2, 0);

    NfaNode *start_node2;
    nfa(result2, &start_node2);
    show_nfa(start_node2, 0);

    char *regex3 = "a*b";

    TokenStream ts3;
    lex(regex3, &ts3);

    Parser p3;
    p3.ts = ts3;
    p3.pos = 0;
    AstNode *result3;
    parse(&p3, &result3);
    show_ast(result3, 0);
    
    NfaNode *start_node3;
    nfa(result3, &start_node3);
    show_nfa(start_node3, 0);

    return 0;
}
