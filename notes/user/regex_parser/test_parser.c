#include <stdio.h>
#include <stdlib.h>
#include "parser.h"
#include "lexer.h"

int main(int argc, char **argv) {
    char *regex1 = "ab";

    TokenStream ts1;
    lex(regex1, &ts1);

    Parser p1;
    p1.ts = ts1;
    p1.pos = 0;
    AstNode *result;
    parse(&p1, &result);
    show_ast(result, 0);

    char *regex2 = "a(b|c*d)";

    TokenStream ts2;
    lex(regex2, &ts2);

    Parser p2;
    p2.ts = ts2;
    p2.pos = 0;
    AstNode *result2;
    parse(&p2, &result2);
    show_ast(result2, 0);

    char *regex3 = "a*b";

    TokenStream ts3;
    lex(regex3, &ts3);

    Parser p3;
    p3.ts = ts3;
    p3.pos = 0;
    AstNode *result3;
    parse(&p3, &result3);
    show_ast(result3, 0);

    return 0;
}
