#include "ast.h"
#include <stdlib.h>
#include <stdio.h>

int ast_node(AstNode **node) {
    *node = malloc(sizeof(AstNode));
    return 0;
}

int free_ast(AstNode *node) {
    if (!node)
        return 0;
    switch (node->kind) {
        default:
            fprintf(stderr, "free_ast: unknown node kind %d\n", node->kind);
            exit(7);
            break;
        case AST_REG:
            free_ast(node->regex.alt);
            break;
        case AST_ALT:
            for (int i = 0; i < node->alt.count; i++) {
                free_ast(node->alt.concat[i]);
            }
            break;
        case AST_CONCAT:
            for (int i = 0; i < node->concat.count; i++) {
                free_ast(node->concat.repeat[i]);
            }
            break;
        case AST_REPEAT:
            free_ast(node->repeat.atom);
            break;
        case AST_ATOM:
            if (!node->atom.is_char)
                free_ast(node->atom.alt);
            break;
    }
    free(node);
    return 0;
}

int show_ast(AstNode *node, int indent) {
    if (!node) {
        return 0;
    }
    switch (node->kind) {
        case AST_REG:
            for (int i = 0; i < indent; i++) {
                printf("  ");
            }
            printf("AST_REG\n");
            show_ast(node->regex.alt, indent + 1);
            break;
        case AST_ALT:
            for (int i = 0; i < indent; i++) {
                printf("  ");
            }
            printf("AST_ALT\n");
            for (int i = 0; i < node->alt.count; i++) {
                show_ast(node->alt.concat[i], indent + 1);
            }
            break;
        case AST_CONCAT:
            for (int i = 0; i < indent; i++) {
                printf("  ");
            }
            printf("AST_CONCAT\n");
            for (int i = 0; i < node->concat.count; i++) {
                show_ast(node->concat.repeat[i], indent + 1);
            }
            break;
        case AST_REPEAT:
            for (int i = 0; i < indent; i++) {
                printf("  ");
            }
            printf("AST_REPEAT(%c)\n", node->repeat.qkind);
            show_ast(node->repeat.atom, indent + 1);
            break;
        case AST_ATOM:
            for (int i = 0; i < indent; i++) {
                printf("  ");
            }
            if (node->atom.is_char) {
                printf("AST_ATOM(char '%c')\n", node->atom.ch);
            } else {
                printf("AST_ATOM(alt)\n");
                show_ast(node->atom.alt, indent + 1);
            }
            break;
    }
    return 0;
}
