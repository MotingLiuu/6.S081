#ifndef LEXER_H
#define LEXER_H

#define MAX_TOKENS 1024

typedef enum {
    TOK_CHAR,
    TOK_PIPE,
    TOK_STAR,
    TOK_PLUS,
    TOK_QMARK,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_EOF,
} TokenKind;

typedef struct {
    TokenKind kind;
    char ch;
    int pos;
} Token;

typedef struct {
    Token *tokens;
    int count;
} TokenStream;

int lex(const char *src, TokenStream *out);
int free_tokens(TokenStream *ts);

int show_tokens(const TokenStream *ts);

#endif
