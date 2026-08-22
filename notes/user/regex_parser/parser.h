#ifndef PARSER_H
#define PARSER_H
#include "lexer.h"
#include "ast.h"

typedef struct {
    TokenStream ts;
    int pos;
} Parser;

int parse(Parser *p, AstNode **node);
int parse_alt(Parser *p, AstNode **node);
int parse_concat(Parser *p, AstNode **node);
int parse_repeat(Parser *p, AstNode **node);
int parse_atom(Parser *p, AstNode **node);

#endif
