# problems
1. `re2nfa()` 同时做了 字符提取，语法判断，优先级处理，nfa构造和错误处理。将前端和后端都压在了同一个控制流当中。

parser应该判断regex的合法性，并将其转换为AST。nfabuilder负责将合法的AST转换为NFA
2. 什么是LL Parser？LR Parser？ 为什么LL Parser与最左推导关系密切？为什么LR Parser可以看作最右推导的逆过程？
3. 什么是二义性？如果存在一个字符串，在这个文法下有两个以上的解析树，那么这个文法就是二义的吗？

# point

1. 现代编译器中，Grammar并没有完全决定字符串的语义结构，例如
```txt
E -> NUM {- NUM}
```
这种表示只规定了字符串的形式结构，但并没有规定其语义结构。Parser Tree也是syntactic结构。

很多实际编译器中的 Grammar 主要规定 syntactic structure，而不会把所有与语义相关的结构都编码进 Grammar。Parser 根据 Grammar 识别输入，AST builder 再把 syntactic structure 映射成更接近程序语义的 AST；随后 semantic analysis 进一步确定名字、类型、作用域等完整语义。

```txt
                 Grammar
                    │
          定义 syntactic language
          和允许的 derivation
                    │
                    ↓
Input ────────→   Parser
                    │
          根据 Grammar 解析输入
                    │
          ┌─────────┴──────────┐
          ↓                    ↓
      Parse Tree            直接构造 AST
        / CST                   │
          │                     │
          │ AST construction    │
          └─────────┬───────────┘
                    ↓
                   AST
                    │
             Semantic analysis
                    │
                    ↓
                    IR
```


# regex parser v1

## Grammar

```txt
Regex -> Concat {'|' Concat}
Concat -> Repeat {Repeat}
Repeat -> Atom Quantifier
Quantifier -> '*' | '+' | '?'
Atom -> '(' Regex ')'
        | '.'
        | NUM
        | ALPHA
```

## Parser

1. Recursive Descent
```python
def parse_regex(s):
    parse_concat(s)
    while next(s) == '|':
        consume(s, '|')
        parse_concat(s)

def parse_concat(s):
    parse_repeat(s)
    while starts_atom(peek(s)):
        parse_repeat(s)

def parse_repeat(s):
    parse_atom(s)
    if next(s) in '*+?':
        parse_quantifier(s)
```

## Write a Parser with System Mind

```txt
Regex       → Alternation
Alternation → Concatenation { "|" Concatenation }
Concatenation
            → Repetition { Repetition }
Repetition  → Atom [ Quantifier ]
Quantifier  → "*" | "+" | "?"
Atom        → CHAR
            | "(" Alternation ")"
```

1. Write a lexer to parse the input string to a list of tokens.
```c
typedef struct {
    TokenKind kind;
    char ch;

    int pos;
} Token;
```

```txt
ab*|c
```
Output of lexer:
```txt
TOK_CHAR('a')
TOK_CHAR('b')
TOK_STAR
TOK_PIPE
TOK_CHAR('c')
TOK_EOF
```
2. TokenStream
```c
typedef struct {
    Token *tokens;
    int count;
    int pos;
} TokenStream;
```
interface of TokenStream:
```c
Token *peek(TokenStream *ts);
Token *advance(TokenStream *ts);
int match(TokenStream *ts, TokenKind kind);
```


