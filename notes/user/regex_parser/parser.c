#include "parser.h"
#include "lexer.h"
#include <stdlib.h>
#include <stdio.h>

#define MAX_CHILD 10

// Contract:
// The caller should promise that p is a vaild Parser
int peek(Parser *p, TokenKind *kind) {
    if (p->pos >= p->ts.count) {
        *kind = TOK_EOF;
    } else {
        *kind = p->ts.tokens[p->pos].kind;
    }
    return 0;
}

// Contract: The caller should promise that p is a vaild Parser
int advance(Parser *p) {
    p->pos++;
    return 0;
}

int expect(Parser *p, TokenKind kind) {
    TokenKind tmp;
    peek(p, &tmp);
    if (tmp == kind) {
        advance(p);
        return 1;
    } 
    return 0;
}

int first_atom(Parser *p) {
    TokenKind kind;
    peek(p, &kind);
    if (kind == TOK_CHAR) {
        return 1;
    } else if (kind == TOK_LPAREN) {
        return 1;
    }
    return 0;
}

int first_repeat(Parser *p) {
    return first_atom(p);
}

int first_concat(Parser *p) {
    if (first_repeat(p)) {
        return 1;
    }  
    TokenKind kind;
    peek(p, &kind);
    if (kind == TOK_PIPE) {
        return 1;
    }
    return 0;
}

int first_alt(Parser *p) {
    if (first_concat(p)) {
        return 1;
    }
    TokenKind kind;
    peek(p, &kind);
    if (kind == TOK_LPAREN) {
        return 1;
    }
    return 0;
}

int first_regex(Parser *p) {
    if (first_alt(p)) {
        return 1;
    }
    return 0;
}

// Contract:
// The caller should promise that *p the first token of *p is ( or char.
// callee should malloc and create a AstNode
int parse_atom(Parser *p, AstNode **node) {
    (*node) = malloc(sizeof(AstNode));
    (*node)->kind = AST_ATOM;
    if (expect(p, TOK_LPAREN)) {
        (*node)->atom.is_char = 0;
        (*node)->atom.ch = '(';
        (*node)->atom.alt = NULL;
    } else {
        (*node)->atom.is_char = 1;
        (*node)->atom.ch = p->ts.tokens[p->pos].ch;
        advance(p);
    }
    return 0;
}

int parse_repeat(Parser *p, AstNode **node) {
    (*node) = malloc(sizeof(AstNode));
    (*node)->kind = AST_REPEAT;
    parse_atom(p, &((*node)->repeat.atom));
    if (expect(p, TOK_STAR)) {
        (*node)->repeat.qkind = '*';
    } else if (expect(p, TOK_PLUS)) {
        (*node)->repeat.qkind = '+';
    } else if (expect(p, TOK_QMARK)) {
        (*node)->repeat.qkind = '?';
    } else {
        return -1;
    }
    return 0;
}

int parse_concat(Parser *p, AstNode **node) {
    (*node) = malloc(sizeof(AstNode));
    (*node)->kind = AST_CONCAT;
    (*node)->concat.count = 0;
    (*node)->concat.repeat = malloc(sizeof(AstNode *) * MAX_CHILD);
    do {
        if ((*node)->concat.count >= MAX_CHILD) {
            exit(8);
        }
        parse_repeat(p, &((*node)->concat.repeat[(*node)->concat.count]));
    } while (first_repeat(p));
    return 0;
}

int parse_alt(Parser *p, AstNode **node) {
    (*node) = malloc(sizeof(AstNode));
    (*node)->kind = AST_ALT;
    (*node)->alt.count = 0;
    (*node)->alt.concat = malloc(sizeof(AstNode *) * MAX_CHILD);
    do {
        if ((*node)->alt.count >= MAX_CHILD) {
            exit(8);
        }
        parse_concat(p, &((*node)->alt.concat[(*node)->alt.count]));
    } while (expect(p, TOK_PIPE));
    return 0;
}

int parse(Parser *p, AstNode **node) {
    (*node) = malloc(sizeof(AstNode));
    (*node)->kind = AST_REG;
    parse_alt(p, &((*node)->regex.alt));
    if (!expect(p, TOK_EOF)) {
        return 0;
    }
    return -1;
}


