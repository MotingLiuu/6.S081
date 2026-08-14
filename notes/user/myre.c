#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_TOKENS 1024

typedef enum {
    TOK_CHAR,

    TOK_PIPE,
    TOK_STAR,
    TOKEN_PLUS,
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
    int pos;
} TokenStream;

typedef struct {
    char *str;
} Lexer;

TokenStream lexe_regex(Lexer *lex) {
    Token *tokens = malloc(MAX_TOKENS * sizeof(Token));
    int count = 0;
    int pos = 0;

    char *curp = lex->str;
    while (*curp) {
        switch (*curp) {
            default:
                tokens[count].kind = TOK_CHAR;
                tokens[count].ch = *curp;
                tokens[count].pos = count;
            case '|':
                tokens[count].kind = TOK_PIPE;
                tokens[count].pos = count;
                break;
            case '*':
                tokens[count].kind = TOK_STAR;
                tokens[count].pos = count;
                break;
            case '+':
                tokens[count].kind = TOKEN_PLUS;
                tokens[count].pos = count;
                break;
            case '?':
                tokens[count].kind = TOK_QMARK;
                tokens[count].pos = count;
                break;
            case '(':
                tokens[count].kind = TOK_LPAREN;
                tokens[count].pos = count;
                break;
            case ')':
                tokens[count].kind = TOK_RPAREN;
                tokens[count].pos = count;
                break;
            }
        count++;
        curp++;
        }
    tokens[count].kind = TOK_EOF;
    tokens[count].pos = count;

    TokenStream result;
    result.tokens = tokens;
    result.count = count;
    result.pos = 0;

    return result;
}

int test_lexer() {
    char *str = "abc|*?+def";
    Lexer lex;
    lex.str = str;
    TokenStream ts = lexe_regex(&lex);
    for (int i = 0; i < ts.count; i++) {
        printf("%d %d %c\n", ts.tokens[i].pos, ts.tokens[i].kind, ts.tokens[i].ch);
    }
    return 0;
}



int main(int argc, char *argv[])
{
    // test part
    test_lexer();
    return 0;
}




