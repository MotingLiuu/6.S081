#include <stdio.h>
#include <stdlib.h>

/* ============================================================
 * ast.h
 * ============================================================ */

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

/* ============================================================
 * ast.c
 * ============================================================ */

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

/* ============================================================
 * lexer.h
 * ============================================================ */

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

/* ============================================================
 * lexer.c
 * ============================================================ */

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

/* ============================================================
 * parser.h
 * ============================================================ */

typedef struct {
    TokenStream ts;
    int pos;
} Parser;

int parse(Parser *p, AstNode **node);
int parse_alt(Parser *p, AstNode **node);
int parse_concat(Parser *p, AstNode **node);
int parse_repeat(Parser *p, AstNode **node);
int parse_atom(Parser *p, AstNode **node);

/* ============================================================
 * parser.c
 * ============================================================ */

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

        if (!first_alt(p)) {
            return -1;
        }

        parse_alt(p, &((*node)->atom.alt));

        if (!expect(p, TOK_RPAREN)) {
            return -1;
        }

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

    if (!first_atom(p)) {
        return -1;
    }

    parse_atom(p, &((*node)->repeat.atom));
    if (expect(p, TOK_STAR)) {
        (*node)->repeat.qkind = '*';
    } else if (expect(p, TOK_PLUS)) {
        (*node)->repeat.qkind = '+';
    } else if (expect(p, TOK_QMARK)) {
        (*node)->repeat.qkind = '?';
    } else {
        (*node)->repeat.qkind = 0;
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

        if (!first_repeat(p)) {
            return -1;
        }

        parse_repeat(p, &((*node)->concat.repeat[(*node)->concat.count]));
        (*node)->concat.count++;
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

        if (!first_concat(p)) {
            return -1;
        }

        parse_concat(p, &((*node)->alt.concat[(*node)->alt.count]));
        (*node)->alt.count++;

    } while (expect(p, TOK_PIPE));
    return 0;
}

int parse(Parser *p, AstNode **node) {
    (*node) = malloc(sizeof(AstNode));
    (*node)->kind = AST_REG;

    if (!first_alt(p)) {
        return -1;
    }

    parse_alt(p, &((*node)->regex.alt));
    if (!expect(p, TOK_EOF)) {
        return -1;
    }
    return 0;
}

/* ============================================================
 * nfa.h
 * ============================================================ */

typedef enum {
    NFA_SP,
    NFA_NOR,
    NFA_END,
} NfaKind;

typedef struct NfaNode NfaNode;
typedef struct DanNfa DanNfa;

struct NfaNode {
    NfaKind kind;
    int id;
    int visited;
    char c1, c2;
    NfaNode *next1, *next2;
};

struct DanNfa {
    NfaNode **node;
    DanNfa *dan;
};

int append(DanNfa *list, DanNfa *node);
int show_nfa(NfaNode *nfa, int indent);
int nfa(AstNode *ast, NfaNode **start);
int nfa_alt(AstNode *ast, NfaNode **start, DanNfa **dang);
int nfa_atom(AstNode *ast, NfaNode **start, DanNfa **dang);
int match(NfaNode *start, char *str);
void free_nfa_arena();

/* ============================================================
 * nfa.c
 * ============================================================ */

#define NFA_ARENA_SIZE 1024

int nfaid;
struct NfaArena {
    NfaNode nodes[NFA_ARENA_SIZE];
    int p;
} nfa_arena;

NfaNode *new_nfa_node() {
    if (nfa_arena.p >= NFA_ARENA_SIZE) {
        return NULL;
    }
    return &(nfa_arena.nodes[nfa_arena.p++]);
}

void free_nfa_arena() {
    nfa_arena.p = 0;
}

struct DanNfaArena {
    DanNfa nodes[NFA_ARENA_SIZE];
    int p;
} dannfa_arena;

DanNfa *new_dannfa_node() {
    if (dannfa_arena.p >= NFA_ARENA_SIZE) {
        return NULL;
    }
    return &(dannfa_arena.nodes[dannfa_arena.p++]);
}

void free_dannfa_arena() {
    dannfa_arena.p = 0;
}

int reset_nfavisited(NfaNode *nfa) {
    if (!nfa || nfa->visited == 0) {
        return 0;
    }
    switch (nfa->kind) {
        case NFA_SP:
            nfa->visited = 0;
            reset_nfavisited(nfa->next1);
            reset_nfavisited(nfa->next2);
            break;
        case NFA_NOR:
            nfa->visited = 0;
            reset_nfavisited(nfa->next1);
            break;
        case NFA_END:
            nfa->visited = 0;
            break;
        default:
            exit(7);
    }
    return 0;
}

int show_nfa(NfaNode *nfa, int indent) {
    if (!nfa) {
        return -1;
    }
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
    switch (nfa->kind) {
        case NFA_SP:
            printf("[ID:%d]: %c, %c\n",nfa->id ,nfa->c1, nfa->c2);
            if (nfa->visited) {
                return 0;
            } else {
                nfa->visited = 1;
            }
            show_nfa(nfa->next1, indent + 1);
            show_nfa(nfa->next2, indent + 1);
            break;
        case NFA_NOR:
            printf("(ID:%d): %c\n",nfa->id ,nfa->c1);
            if (nfa->visited) {
                return 0;
            } else {
                nfa->visited = 1;
            }
            show_nfa(nfa->next1, indent + 1);
            break;
        case NFA_END:
            printf("{ID:%d}\n", nfa->id);
            break;
        default:
            exit(7);
    }
    if (indent == 0) {
        reset_nfavisited(nfa);
    }
    return 0;
}

int concat(DanNfa *list1, DanNfa *list2) {
    //defensive check
    if (!list1 || !list2) {
        exit(7);
    }
    while (list1->dan) {
        list1 = list1->dan;
    }
    list1->dan = list2;
    return 0;
}

int connect(DanNfa *list, NfaNode *node) {
    if (!list) {
        exit(7);
    }
    while (list) {
        *(list->node) = node;
        list = list->dan;
    }
    return 0;
}

void show_dang(DanNfa *dang) {
    while (dang) {
        printf("node pointer of dang: %p\n", *(dang->node));
        dang = dang->dan;
    }
}

// Contract:
// 1. ast is not NULL
// 2. ast type is AST_ATOM
int nfa_atom(AstNode *ast, NfaNode **start, DanNfa **dang) {
    //defensive check
    if (!ast || ast->kind != AST_ATOM) {
        exit(7);
    }

    if (ast->atom.is_char) {
        NfaNode *node1 = new_nfa_node();
        node1->kind = NFA_NOR;
        node1->id = nfaid++;
        node1->visited = 0;
        node1->c1 = ast->atom.ch;
        node1->c2 = 0;
        node1->next1 = NULL;
        node1->next2 = NULL;

        *start = node1;

        DanNfa *nfanode = new_dannfa_node();
        nfanode->node = &(node1->next1);
        nfanode->dan = NULL;

        *dang = nfanode;
    } else {
        if (nfa_alt(ast->atom.alt, start, dang) == -1) {
            return -1;
        }
    }
    return 0;
}

int nfa_repeat(AstNode *ast, NfaNode **start, DanNfa **dang) {
    //defensive check
    if (!ast || ast->kind != AST_REPEAT) {
        exit(7);
    }
    // 1. construct the nfa of atom
    NfaNode *start_atom;
    DanNfa *dang_atom;

    if (nfa_atom(ast->repeat.atom, &start_atom, &dang_atom) == -1) {
        return -1;
    }

    NfaNode *node1 = new_nfa_node();
    DanNfa *nfanode = new_dannfa_node();

    if (!node1 || !nfanode) {
        return -1;
    }

    switch (ast->repeat.qkind) {
        case '*':
            node1->kind = NFA_SP;
            node1->id = nfaid++;
            node1->visited = 0;
            node1->c1 = 0;
            node1->c2 = 0;
            node1->next1 = NULL;
            node1->next2 = NULL;

            node1->next1 = start_atom;
            if (connect(dang_atom, node1) == -1) {
                return -1;
            }

            *start = node1;

            nfanode->node = &(node1->next2);
            nfanode->dan = NULL;

            *dang = nfanode;

            break;
        case '?':
            node1->kind = NFA_SP;
            node1->id = nfaid++;
            node1->visited = 0;
            node1->c1 = 0;
            node1->c2 = 0;
            node1->next1 = NULL;
            node1->next2 = NULL;

            node1->next1 = start_atom;

            *start = node1;

            nfanode->node = &(node1->next2);
            nfanode->dan = NULL;

            if (concat(nfanode, dang_atom) == -1) {
                return -1;
            };

            *dang = nfanode;

            break;
        case '+':
            node1->kind = NFA_SP;
            node1->id = nfaid++;
            node1->visited = 0;
            node1->c1 = 0;
            node1->c2 = 0;
            node1->next1 = NULL;
            node1->next2 = NULL;

            node1->next1 = start_atom;
            node1->next2 = NULL;

            if (connect(dang_atom, node1) == -1) {
                return -1;
            }

            *start = start_atom;

            nfanode->node = &(node1->next2);
            nfanode->dan = NULL;

            *dang = nfanode;

            break;
        default:
            *start = start_atom;
            *dang = dang_atom;
            break;
    }

    return 0;
}

int nfa_concat(AstNode *ast, NfaNode **start, DanNfa **dang) {
    //defensive check
    if (!ast || ast->kind != AST_CONCAT) {
        exit(7);
    }
    DanNfa *tmp_dang = NULL;
    NfaNode *tmp_start = NULL;
    for (int i = 0; i < ast->concat.count; i++) {
        if (i == 0) {
            nfa_repeat(ast->concat.repeat[i], start, dang);
        } else {
            nfa_repeat(ast->concat.repeat[i], &tmp_start, &tmp_dang);
            if (connect(*dang, tmp_start) == -1) {
                return -1;
            }
            *dang = tmp_dang;
        }
    }
    return 0;
}

int nfa_alt(AstNode *ast, NfaNode **start, DanNfa **dang) {
    //defensive check
    if (!ast || ast->kind != AST_ALT) {
        exit(7);
    }
    DanNfa *tmp_dang;
    NfaNode *tmp_start;
    for (int i = 0; i < ast->alt.count; i++) {
        if (i == 0) {
            if (nfa_concat(ast->alt.concat[i], start, dang) == -1) {
                return -1;
            }
        } else {
            NfaNode *new_node;
            new_node = new_nfa_node();
            new_node->kind = NFA_SP;
            new_node->id = nfaid++;
            new_node->visited = 0;
            new_node->c1 = 0;
            new_node->c2 = 0;
            new_node->next1 = NULL;
            new_node->next2 = NULL;

            new_node->next1 = *start;
            *start = new_node;

            if (nfa_concat(ast->alt.concat[i], &tmp_start, &tmp_dang) == -1) {
                return -1;
            }

            new_node->next2 = tmp_start;
            if (concat(*dang, tmp_dang) == -1) {
                return -1;
            }
        }
    }
    return 0;
}

int
nfa(AstNode *ast, NfaNode **start)
{
    /* defensive check */
    if (!ast || ast->kind != AST_REG || !start) {
        exit(7);
    }

    nfaid = 0;

    /*
     * arena checkpoint
     */
    int nfa_mark = nfa_arena.p;
    int dannfa_mark = dannfa_arena.p;
    int id_mark = nfaid;

    *start = NULL;

    DanNfa *dang = NULL;

    NfaNode *end_node = new_nfa_node();
    if (!end_node) {
        goto fail;
    }

    end_node->kind = NFA_END;
    end_node->id = nfaid++;
    end_node->visited = 0;
    end_node->c1 = 0;
    end_node->c2 = 0;
    end_node->next1 = NULL;
    end_node->next2 = NULL;

    if (nfa_alt(ast->regex.alt, start, &dang) == -1) {
        goto fail;
    }

    if (connect(dang, end_node) == -1) {
        goto fail;
    }

    dannfa_arena.p = dannfa_mark;

    return 0;


fail:
    nfa_arena.p = nfa_mark;
    dannfa_arena.p = dannfa_mark;
    nfaid = id_mark;

    *start = NULL;

    return -1;
}

typedef struct MatchList MatchList;
struct MatchList {
    NfaNode *list[NFA_ARENA_SIZE];
    int count;
};

MatchList list1, list2;

int addstate(MatchList *list, NfaNode *node)
{
    if (!list) {
        exit(7);
    }

    if (!node) {
        return 0;
    }

    if (node->kind == NFA_SP) {
        if (addstate(list, node->next1) == -1) {
            return -1;
        }

        if (addstate(list, node->next2) == -1) {
            return -1;
        }

        return 0;
    }

    for (int i = 0; i < list->count; i++) {
        if (list->list[i] == node) {
            return 0;
        }
    }

    if (list->count >= NFA_ARENA_SIZE) {
        return -1;
    }

    list->list[list->count++] = node;

    return 0;
}

int step(MatchList *list1, MatchList *list2, char c)
{
    // defensive check
    if (!list1 || !list2 || c == 0) {
        exit(7);
    }

    list2->count = 0;

    for (int i = 0; i < list1->count; i++) {

        NfaNode *node = list1->list[i];

        if (!node) {
            exit(7);
        }

        switch (node->kind) {

        case NFA_NOR:
            if (node->c1 == c) {
                if (addstate(list2, node->next1) == -1) {
                    return -1;
                }
            }

            break;

        case NFA_END:
            break;

        case NFA_SP:
            exit(7);

        default:
            exit(7);
        }
    }

    return 0;
}

int match(NfaNode *start, char *str)
{
    if (!start || !str) {
        exit(7);
    }

    MatchList *clist = &list1;
    MatchList *nlist = &list2;

    clist->count = 0;
    nlist->count = 0;

    if (addstate(clist, start) == -1) {
        return -1;
    }

    for (int i = 0; str[i] != '\0'; i++) {

        if (step(clist, nlist, str[i]) == -1) {
            return -1;
        }

        MatchList *tmp = clist;
        clist = nlist;
        nlist = tmp;
    }

    for (int i = 0; i < clist->count; i++) {

        if (clist->list[i]->kind == NFA_END) {
            return 1;
        }
    }

    return 0;
}

int append(DanNfa *list, DanNfa *node) {
    (void)list;
    (void)node;
    return 0;
}

/* ============================================================
 * main.c
 * ============================================================ */

int
main(int argc, char **argv)
{
    TokenStream ts = {0};
    Parser parser = {0};
    AstNode *ast = NULL;
    NfaNode *start = NULL;

    int matched;
    int ret = 1;

    if (argc != 3) {
        fprintf(stderr, "usage: regex PATTERN STRING\n");
        goto cleanup;
    }

    /*
     * 1. Lex
     */
    if (lex(argv[1], &ts) == -1) {
        fprintf(stderr, "regex: lex error\n");
        goto cleanup;
    }

    /*
     * 2. Parse
     */
    parser.ts = ts;
    parser.pos = 0;
    if (parse(&parser, &ast) == -1) {
        fprintf(stderr, "regex: parse error\n");
        goto cleanup;
    }

    /*
     * 3. Construct NFA
     */
    if (nfa(ast, &start) == -1) {
        fprintf(stderr, "regex: nfa construction error\n");
        goto cleanup;
    }

    /*
     * 4. Match
     */
    matched = match(start, argv[2]);

    if (matched == -1) {
        fprintf(stderr, "regex: match error\n");
        goto cleanup;
    }

    if (matched) {
        printf("match\n");
    } else {
        printf("no match\n");
    }

    ret = 0;

cleanup:
    free_ast(ast);
    free_tokens(&ts);

    /*
     * NfaNode / DanNfa are owned by arena.
     * Do not free individual nodes.
     */
    free_nfa_arena();

    return ret;
}
