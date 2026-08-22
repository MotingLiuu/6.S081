#include <stdio.h>
#include <stdlib.h>
#include "parser.h"
#include "lexer.h"

int main(int argc, char **argv) {
    char *regex1 = "a(b|c*d)";

    TokenStream ts1;
    lex(regex1, &ts1);
    show_tokens(&ts1);

    Parser p1;
    p1.ts = ts1;
    p1.pos = 0;
    AstNode *result;
    parse(&p1, &result);
    show_ast(result, 0);

    return 0;
}
