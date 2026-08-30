#include <stdio.h>
#include <stdlib.h>

#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "nfa.h"


int
main(int argc, char **argv)
{
    TokenStream ts = {0};
    Parser parser = {0};
    AstNode *ast = NULL;
    NfaNode *start = NULL;

    int matched;
    int ret = 1;

    if (argc != 3) {
        fprintf(stderr, "usage: regex PATTERN STRING\n");
        goto cleanup;
    }

    /*
     * 1. Lex
     */
    if (lex(argv[1], &ts) == -1) {
        fprintf(stderr, "regex: lex error\n");
        goto cleanup;
    }

    /*
     * 2. Parse
     */
    parser.ts = ts;
    parser.pos = 0;
    if (parse(&parser, &ast) == -1) {
        fprintf(stderr, "regex: parse error\n");
        goto cleanup;
    }

    /*
     * 3. Construct NFA
     */
    if (nfa(ast, &start) == -1) {
        fprintf(stderr, "regex: nfa construction error\n");
        goto cleanup;
    }

    /*
     * 4. Match
     */
    matched = match(start, argv[2]);

    if (matched == -1) {
        fprintf(stderr, "regex: match error\n");
        goto cleanup;
    }

    if (matched) {
        printf("match\n");
    } else {
        printf("no match\n");
    }

    ret = 0;

cleanup:
    free_ast(ast);
    free_tokens(&ts);

    /*
     * NfaNode / DanNfa are owned by arena.
     * Do not free individual nodes.
     */
    free_nfa_arena();

    return ret;
}
