#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

int main(int argc, char **argv) {
    AstNode *regex1 = malloc(sizeof(AstNode));
    regex1->kind = AST_REG;
    regex1->regex.alt = malloc(sizeof(AstNode));

    AstNode *alt1 = regex1->regex.alt;
    alt1->kind = AST_ALT;
    alt1->alt.count = 2;
    alt1->alt.concat = malloc(sizeof(AstNode *) * 2);
    alt1->alt.concat[0] = malloc(sizeof(AstNode));
    alt1->alt.concat[1] = malloc(sizeof(AstNode));

    AstNode *concat1 = alt1->alt.concat[0];
    AstNode *concat2 = alt1->alt.concat[1];
    concat1->kind = AST_CONCAT;
    concat1->concat.count = 2;
    concat1->concat.repeat = malloc(sizeof(AstNode *) * 2);
    concat1->concat.repeat[0] = malloc(sizeof(AstNode));
    concat1->concat.repeat[1] = malloc(sizeof(AstNode));
    concat2->kind = AST_CONCAT;
    concat2->concat.count = 2;
    concat2->concat.repeat = malloc(sizeof(AstNode *) * 2);
    concat2->concat.repeat[0] = NULL;
    concat2->concat.repeat[1] = NULL;

    AstNode *repeat1 = concat1->concat.repeat[0];
    repeat1->kind = AST_REPEAT;
    repeat1->repeat.qkind = '*';
    repeat1->repeat.atom = malloc(sizeof(AstNode));

    AstNode *repeat2 = concat1->concat.repeat[1];
    repeat2->kind = AST_REPEAT;
    repeat2->repeat.qkind = '?';
    repeat2->repeat.atom = malloc(sizeof(AstNode));

    AstNode *atom1 = repeat1->repeat.atom;
    atom1->kind = AST_ATOM;
    atom1->atom.is_char = 1;
    atom1->atom.ch = 'a';

    AstNode *atom2 = repeat2->repeat.atom;
    atom2->kind = AST_ATOM;
    atom2->atom.is_char = 1;
    atom2->atom.ch = 'b';

    show_ast(regex1, 0);
    free_ast(regex1);

    return 0;
}

