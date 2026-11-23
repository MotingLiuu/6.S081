# TODO
1. `syntax_error()`
2. `new_node()`
3. `free_cst()`

# Notes
When designing a parser, there are several independent concerns.
1. Predictive decision
   Grammar + lookahead decide which parsing path to take.
   FIRST/FOLLOW are mainly used here.
   If no valid path exists, report a syntax error.
2. Required syntax validation
   Once a production has been selected, some terminals or
   nonterminals are mandatory.
   They must be consumed/validated, e.g. expect(TOK_RPAREN).
3. Function contract
   Decide who owns the precondition of parse_X():
   - caller validates it, or
   - callee validates it.
4. Error responsibility
   Distinguish:
   - invalid source input → syntax_error
   - parser invariant / contract violation → assert/internal error

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



