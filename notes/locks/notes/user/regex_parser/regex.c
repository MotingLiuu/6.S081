#include <stdio.h>
#include <stdlib.h>

#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "nfa.h"

int matchstr(char *pattern, char *str) {
    TokenStream ts = {0};
    Parser parser = {0};
    AstNode *ast = 0;
    NfaNode *start = 0;

    int matched;
    int ret = 1;

    if(lex(pattern, &ts) == -1) {
        fprintf(stderr, "regex: lex error\n");
        goto cleanup;
    }

    parser.ts = ts;
    parser.pos = 0;
    if (parse(&parser, &ast) == -1) {
        fprintf(stderr, "regex: parse error\n");
        goto cleanup;
    }

    if (nfa(ast, &start) == -1) {
        fprintf(stderr, "regex: nfa construction error\n");
        goto cleanup;
    }

    matched = match(start, str);

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

    free_nfa_arena();

    return ret;


}
