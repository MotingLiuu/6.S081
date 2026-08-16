#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_TOKENS 1024
#define MAX_CHILDREN 16

// error handling part
typedef struct {
    int pos;
    const char *message;
} ParseError;

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
    int pos;
} TokenStream;

typedef struct {
    char *str;
} Lexer;

// TokenStream Part
Token *peek(TokenStream *ts) {
    return &ts->tokens[ts->pos];
}

Token *advance(TokenStream *ts) {
    Token *tok = peek(ts);
    if (tok->kind != TOK_EOF)
        ts->pos++;
    return tok;
}

int match(TokenStream *ts, TokenKind kind) {
    if (peek(ts)->kind == kind) {
        advance(ts);
        return 1;
    }
    return 0;
} 

Token *expect(TokenStream *ts, TokenKind kind) {
    Token *tok = peek(ts);
    if (tok->kind != kind) {
        fprintf(stderr, "syntax error at %d: unexpected token\n", tok->pos);
        exit(1);
    }
    return advance(ts);
}

TokenStream lex_regex(Lexer *lex) {
    Token *tokens = malloc(MAX_TOKENS * sizeof(Token));
    int count = 0;
    int pos = 0;

    char *curp = lex->str;
    while (*curp) {
        if (count >= MAX_TOKENS - 1) {
            fprintf(stderr, "syntax error at %d: too many tokens\n", count);
            exit(1);
        } 
        switch (*curp) {
            default:
                tokens[count].kind = TOK_CHAR;
                tokens[count].ch = *curp;
                tokens[count].pos = (int)(curp - lex->str);
                break;
            case '|':
                tokens[count].kind = TOK_PIPE;
                tokens[count].pos = (int)(curp - lex->str);
                break;
            case '*':
                tokens[count].kind = TOK_STAR;
                tokens[count].pos = (int)(curp - lex->str);
                break;
            case '+':
                tokens[count].kind = TOK_PLUS;
                tokens[count].pos = (int)(curp - lex->str);
                break;
            case '?':
                tokens[count].kind = TOK_QMARK;
                tokens[count].pos = (int)(curp - lex->str);
                break;
            case '(':
                tokens[count].kind = TOK_LPAREN;
                tokens[count].pos = (int)(curp - lex->str);
                break;
            case ')':
                tokens[count].kind = TOK_RPAREN;
                tokens[count].pos = (int)(curp - lex->str);
                break;
            }
        count++;
        curp++;
        }
    tokens[count].kind = TOK_EOF;
    tokens[count].pos = (int)(curp - lex->str);
    count++;

    TokenStream result;
    result.tokens = tokens;
    result.count = count;
    result.pos = 0;

    return result;
}

int starts_atom(TokenKind kind) {
    return kind == TOK_CHAR || kind == TOK_LPAREN;
}

int follows_atom(TokenKind kind) {
    return kind == TOK_STAR  ||
           kind == TOK_PLUS  ||
           kind == TOK_QMARK ||
           kind == TOK_CHAR  ||
           kind == TOK_LPAREN ||
           kind == TOK_PIPE ||
           kind == TOK_RPAREN ||
           kind == TOK_EOF;
}

int starts_quantifier(TokenKind kind) {
    return kind == TOK_STAR || kind == TOK_PLUS || kind == TOK_QMARK;
}

int follows_quantifier(TokenKind kind) {
    return kind == TOK_EOF || kind == TOK_RPAREN || kind == TOK_CHAR || kind == TOK_LPAREN || kind == TOK_PIPE;
}

int starts_repetition(TokenKind kind) {
    return kind == TOK_CHAR || kind == TOK_LPAREN;
}

int follows_repetition(TokenKind kind) {
    return kind == TOK_EOF || kind == TOK_RPAREN || kind == TOK_CHAR || kind == TOK_LPAREN || kind == TOK_PIPE;
}

int starts_concatenation(TokenKind kind) {
    return kind == TOK_CHAR || kind == TOK_LPAREN;
}

int follows_concatenation(TokenKind kind) {
    return kind == TOK_EOF || kind == TOK_RPAREN || kind == TOK_PIPE;
}

int starts_alternation(TokenKind kind) {
    return kind == TOK_CHAR || kind == TOK_LPAREN;
}

int follows_alternation(TokenKind kind) {
    return kind == TOK_EOF || kind == TOK_RPAREN;
}

int starts_regex(TokenKind kind) {
    return starts_alternation(kind);
}

int follows_regex(TokenKind kind) {
    return kind == TOK_EOF;
}

// parser part
typedef struct {
    TokenStream *ts;

    int has_error;
    ParseError error;
} Parser;

void parser_error(Parser *p, const char *message) {
    if (p->has_error)
        return;
    p->has_error = 1;
    p->error.pos = peek(p->ts)->pos;
    p->error.message = message;
}

typedef enum {
    CST_REGEX,
    CST_CHAR,
    CST_GROUP,
    CST_REPEAT,
    CST_CONCAT,
    CST_ALT,
} CstKind;

typedef struct CstNode CstNode;

struct CstNode {
    CstKind kind;
    union {

        struct {
            char ch;
        } character;

        struct {
            CstNode *atom;
            int has_quantifier;
            TokenKind quantifier;
        } repeat;

        struct {
            CstNode **items;
            int count;
        } concat;

        struct {
            CstNode **branches;
            int count;
        } alt;

        struct {
            CstNode *alt;
        } regex;

        struct {
            CstNode *alt;
        } group;

    };
};

CstNode *parse_regex(Parser *p);
CstNode *parse_alt(Parser *p);
CstNode *parse_concat(Parser *p);
CstNode *parse_repeat(Parser *p);
CstNode *parse_atom(Parser *p);

CstNode *parse_regex(Parser *p) {
    if (!starts_regex(peek(p->ts)->kind)) {
        fprintf(stderr, "syntax error at %d: unexpected token\n", p->ts->pos);
        exit(1);
    }
    CstNode *result = malloc(sizeof(CstNode));
    result->kind = CST_REGEX;
    result->regex.alt = parse_alt(p);
    if (!follows_regex(peek(p->ts)->kind)) {
        fprintf(stderr, "syntax error at %d: unexpected token\n", p->ts->pos);
        exit(1);
    }
    return result;
}

CstNode *parse_alt(Parser *p) {
    if (!starts_alternation(peek(p->ts)->kind)) {
        fprintf(stderr, "syntax error at %d: unexpected token\n", p->ts->pos);
        exit(1);
    }
    CstNode *result = malloc(sizeof(CstNode));
    result->alt.count = 0;
    result->alt.branches = malloc(MAX_CHILDREN * sizeof(CstNode *));
    result->kind = CST_ALT;

    if (result->alt.count >= MAX_CHILDREN) {
        fprintf(stderr, "syntax error at %d: too many branches\n", p->ts->pos);
        exit(1);
    }
    result->alt.branches[result->alt.count++] = parse_concat(p); 
    while (match(p->ts, TOK_PIPE)) {
        if (result->alt.count >= MAX_CHILDREN) {
            fprintf(stderr, "syntax error at %d: too many branches\n", p->ts->pos);
            exit(1);
        }
        result->alt.branches[result->alt.count++] = parse_concat(p);
    }
    
    if (!follows_alternation(peek(p->ts)->kind)) {
        fprintf(stderr, "syntax error at %d: unexpected token\n", p->ts->pos);
        exit(1);
    }

    return result;
}

CstNode *parse_concat(Parser *p) {
    if (!starts_concatenation(peek(p->ts)->kind)) {
        fprintf(stderr, "syntax error at %d: unexpected token\n", p->ts->pos);
        exit(1);
    }
    CstNode *result = malloc(sizeof(CstNode));
    result->kind = CST_CONCAT;
    result->concat.count = 0;
    result->concat.items = malloc(MAX_CHILDREN * sizeof(CstNode *));

    if (result->concat.count >= MAX_CHILDREN) {
        fprintf(stderr, "syntax error at %d: too many branches\n", p->ts->pos);
        exit(1);
    }
    result->concat.items[result->concat.count++] = parse_repeat(p);

    while (starts_repetition(peek(p->ts)->kind)) {
        if (result->concat.count >= MAX_CHILDREN) {
            fprintf(stderr, "syntax error at %d: too many branches\n", p->ts->pos);
            exit(1);
        }
        result->concat.items[result->concat.count++] = parse_repeat(p);
    }

    if (!follows_concatenation(peek(p->ts)->kind)) {
        fprintf(stderr, "syntax error at %d: unexpected token\n", p->ts->pos);
        exit(1);
    }

    return result;
}

CstNode *parse_repeat(Parser *p) {
    if (!starts_repetition(peek(p->ts)->kind)) {
        fprintf(stderr, "syntax error at %d: unexpected token\n", p->ts->pos);
        exit(1);
    }
    CstNode *result = malloc(sizeof(CstNode));
    result->kind = CST_REPEAT;
    result->repeat.atom = parse_atom(p);
    if (starts_quantifier(peek(p->ts)->kind)) {
        result->repeat.has_quantifier = 1;
        result->repeat.quantifier = advance(p->ts)->kind;
    } else {
        result->repeat.has_quantifier = 0;
    }

    if (!follows_repetition(peek(p->ts)->kind)) {
        fprintf(stderr, "syntax error at %d: unexpected token\n", p->ts->pos);
        exit(1);
    }

    return result;
}

CstNode *parse_atom(Parser *p) {
    if (!starts_atom(peek(p->ts)->kind)) {
        fprintf(stderr, "syntax error at %d: unexpected token\n", p->ts->pos);
        exit(1);
    }
    CstNode *result = malloc(sizeof(CstNode));
    if (match(p->ts, TOK_LPAREN)) {
        result->kind = CST_GROUP;
        result->group.alt = parse_alt(p);
        expect(p->ts, TOK_RPAREN);
    } else {
        result->kind = CST_CHAR;
        result->character.ch = peek(p->ts)->ch;
        advance(p->ts);
    }
    if (!follows_atom(peek(p->ts)->kind)) {
        fprintf(stderr, "syntax error at %d: unexpected token\n", p->ts->pos);
        exit(1);
    }
    return result;
}

char quantifier_char(TokenKind kind) {
    switch (kind) {
        case TOK_STAR:
            return '*';
        case TOK_PLUS:
            return '+';
        case TOK_QMARK:
            return '?';
        default:
            return '\0';
    }
}

void show_cst(CstNode *node, int level) {
    for (int i = 0; i < level; i++) {
        printf("    ");
    }
    printf("----");
    switch (node->kind) {
        case CST_CHAR:
            printf("CST_CHAR: %c\n", node->character.ch);
            break;
        case CST_GROUP:
            printf("CST_GROUP\n");
            show_cst(node->group.alt, level + 1);
            break;
        case CST_REPEAT:
            printf("CST_REPEAT\n");
            if (node->repeat.has_quantifier) {
                for (int i = 0; i < level + 1; i++) {
                    printf("    ");
                }
                printf("----");
                printf("QUANTIFIER: %c\n", quantifier_char(node->repeat.quantifier));
            }
            show_cst(node->repeat.atom, level + 1);
            break;
        case CST_CONCAT:
            printf("CST_CONCAT\n");
            for (int i = 0; i < node->concat.count; i++) {
                show_cst(node->concat.items[i], level + 1);
            }
            break;
        case CST_ALT:
            printf("CST_ALT\n");
            for (int i = 0; i < node->alt.count; i++) {
                show_cst(node->alt.branches[i], level + 1);
            }
            break;
        case CST_REGEX:
            printf("CST_REGEX\n");
            show_cst(node->regex.alt, level + 1);
            break;
        default:
            printf("unknown node\n");
            break;
    }
}

int test_parser_legal1() {
    char *str = "(ab|c)*de?f";
    Lexer lex;
    lex.str = str;
    TokenStream ts = lex_regex(&lex);
    Parser p;
    p.ts = &ts;
    CstNode *regex = parse_regex(&p);
    show_cst(regex, 0);
    return 0;
}

int test_parser_legal2() {
    char *str = "ab+(cd|e)?f";
    Lexer lex;
    lex.str = str;
    TokenStream ts = lex_regex(&lex);
    Parser p;
    p.ts = &ts;
    CstNode *regex = parse_regex(&p);
    show_cst(regex, 0);
    return 0;
}


int test_lexer() {
    char *str = "(ab|c)*de?f";
    Lexer lex;
    lex.str = str;
    TokenStream ts = lex_regex(&lex);
    for (int i = 0; i < ts.count; i++) {
        printf("%d %d %c\n", ts.tokens[i].pos, ts.tokens[i].kind, ts.tokens[i].ch);
    }
    return 0;
}





int main(int argc, char *argv[])
{
    // test part
    test_lexer();
    // test parser
    test_parser_legal1();
    test_parser_legal2();
    return 0;
}




