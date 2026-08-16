typedef enum {
    TOK_CHAR,
    TOK_STAR,
    TOK_PLUS,
    TOK_QUESTION,
    TOK_PIPE,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_EOF
} TokenKind;

typedef struct {
    TokenKind kind;
    char ch;
    int pos;
} Token;
