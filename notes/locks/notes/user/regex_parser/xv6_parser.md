# format

```c
0 = success
1 = failure
```

return `int` type

```c
int lex();
int parse();
int parse_alt();
int parse_cat();
```

predicate function

```c
0 = false
1 = true
```

# Deal with Parser bug and input error seperately

```c
return -1;
```
this represents this operation is failed. e.g. illegal regex, or insufficient source.

```c
assert(...);
panic(...);
```

This means parser violates the contract.
e.g.
The contract of `parse_atom()` says that:
```txt
when calling parse_atom()

peek(p) is in FIRST(Atom)
```

```c
static int
parse_atom(Parser *p, CstNode **out)
{
    switch(peek(p)->kind){
    case TOK_CHAR:
        ...
        break;

    case TOK_LPAREN:
        ...
        break;

    default:
        panic("parse_atom");
    }
}
```

This `default` is not user input error. Because contract already says that Caller must guarantee that input is in `First(Atom)`. This default is a defensive validation.

```txt
Expected Failure during parsing: 
    return -1;
violating the contract:
    assert / panic
```

# Function contract decides the way to deal with failure

```txt
1. who is responsible for Predictive decision?
2. who should guarantee the entrance token?
3. who should check the token which must show up after the process?
```

```txt
Contract of parse_atom

Precondition:
    current token ∈ FIRST(Atom)

Postcondition on success:
    consume one complete Atom
    set *out

Failure:
    malformed Atom
    allocation failure

Internal bug:
    precondition violated
```

1. Predictive decision: this decides the decision flow of Grammar.
2. Required validation: this is things that must be checked based on the Grammar
e.g.
```txt
Atom -> "(" Alt ")"
```
When `(` is consumed and `Alt` is parsed successfully, then there must be a `)`. This is guaranteed by the Grammar.
```c 
if (peek(p)->kind != TOK_RPAREN)
    return -1;
advance(p);
```
This is Required validation.
3. Defensive check: this should check whether programmer violates the contract.

# The responsibility of Caller and Callee must be clearified.
A: Caller guarantees that the input is in `FIRST(Atom)`.
B: Callee guarantees that the input is in `FIRST(Atom)`.

# Grammar

```txt
# Grammar
```txt
Regex         → Alternation
Alternation   → Concatenation { "|" Concatenation }
Concatenation → Repetition { Repetition }
Repetition    → Atom [ Quantifier ]
Quantifier    → "*" | "+" | "?"
Atom          → CHAR
              | "(" Alternation ")"
```

# OwnerShip of RegexParser

Principle of OwnerShip:
1. The memo malloced by `malloc` in callee should be transfered to caller. then free it in caller.
    ownership is moved through `*start`
2. The final object is to construct an AstTree. Every Function would construct an AstTree, then the owner of AstTree would be moved to `start(NfaNode *)`

















