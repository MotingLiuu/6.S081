#include "lexer.h"
#include <stdlib.h>
#include <stdio.h>

int lex(const char *src, TokenStream *out) {
    out->tokens = malloc(sizeof(Token) * MAX_TOKENS);
    int pos = 0, count = 0;
    while (*src) {
        switch (*src) {
            default:
                out->tokens[count].kind = TOK_CHAR;
                out->tokens[count].ch = *src;
                out->tokens[count].pos = pos;
                pos++;
                break;
            case '|':
                out->tokens[count].kind = TOK_PIPE;
                out->tokens[count].ch = *src;
                out->tokens[count].pos = pos;
                pos++;
                break;
            case '*':
                out->tokens[count].kind = TOK_STAR;
                out->tokens[count].ch = *src;
                out->tokens[count].pos = pos;
                pos++;
                break;
            case '+':
                out->tokens[count].kind = TOK_PLUS;
                out->tokens[count].ch = *src;
                out->tokens[count].pos = pos;
                pos++;
                break;
            case '?':
                out->tokens[count].kind = TOK_QMARK;
                out->tokens[count].ch = *src;
                out->tokens[count].pos = pos;
                pos++;
                break;
            case '(':
                out->tokens[count].kind = TOK_LPAREN;
                out->tokens[count].ch = *src;
                out->tokens[count].pos = pos;
                pos++;
                break;
            case ')':
                out->tokens[count].kind = TOK_RPAREN;
                out->tokens[count].ch = *src;
                out->tokens[count].pos = pos;
                pos++;
                break;
        }
        src++;
        count++;
    }
    out->tokens[count].kind = TOK_EOF;
    out->tokens[count].pos = pos;
    count++;
    out->count = count;

    return 0;
}

int free_tokens(TokenStream *ts) {
    free(ts->tokens);
    return 0;
}

int show_tokens(const TokenStream *ts) {
    for (int i = 0; i < ts->count; i++) {
        printf("Type: %d, Pos: %d, Ch: %c\n", ts->tokens[i].kind, ts->tokens[i].pos, ts->tokens[i].ch);
    }
    return 0;
}






























