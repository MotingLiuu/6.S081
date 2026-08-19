#ifndef LEXER_H
#define LEXER_H

#include "token.h"

typedef struct {
    Token *tokens;
    int count;
    int pos;
} TokenStream;

typedef struct {
    char *str;
    int has_error;
    char *error_msg;
} Lexer;

int lex(const char *src, )

#endif
