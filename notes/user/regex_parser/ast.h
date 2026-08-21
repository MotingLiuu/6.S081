#ifndef AST_H
#define AST_H

#define MAX_

typedef enum {
    AST_REG,
    AST_ALT,
    AST_CONCAT,
    AST_REPEAT,
    AST_ATOM,
} AstKind;

typedef struct AstNode AstNode;

struct AstNode {
    AstKind kind;
    union {
        struct {
            AstNode *alt;
        } regex;
        struct {
            AstNode **concat;
            int count;
        } alt;
        struct {
            AstNode **repeat;
            int count;
        } concat;
        struct {
            char qkind;
            AstNode *atom;
        } repeat;
        struct {
            int is_char;
            char ch;
            AstNode *alt;
        } atom;
    };
};

int ast_node(AstNode **node);
int free_ast(AstNode *node);
int show_ast(AstNode *node, int indent);

#endif
