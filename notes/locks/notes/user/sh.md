# sh.h

## parseexec

```mermaid
graph TD 
    A[parseexec] --> B("the first char of next token is (")
    B --> |yes| C[return parseblock]
    B --> |no| D[parseredirs]
    E("the first char of next token is not in '|)&;'")
    D --> E
    E --> |yes| F["tok = gettoken(ps,es,&q, eq)"]
    F --> G("if token is \0")
    G --> |yes| H["break"]
    G --> |no| I("if token is not a word")
    I --> |yes| J["panic"]
    I --> |no| K["cmd->argv[argc] = q, cmd->eargv[argc] = eq, argc++"]
    K --> L("if argc >= MAXARGS")
    L --> |yes| M["panic too many args"]
    L --> |no| N["ret = parseredirs(ret, ps, es)"]
    N --> E
    E --> |no| O["cmd->argv[argc] = 0, cmd->eargv[argc]=0, return ret"]
    H --> O
```

What is the priority of `|)&;` ?

`()` alway has the highest priority. When parse, first deal with `()`

## parseblock
```mermaid
graph TD
    A("the first char of next token is not '('")
    A --> |yes| B["panic"]
    A --> |no| C["gettoken(ps, es, 0, 0)"]
    C --> K["parseline(ps, es)"]
    K --> D
    D("the first char of next token is not ')'")
    D --> |yes| E["panic"]
    D --> |no| F["gettoken(ps, es, 0, 0), cmd == parserdirs(cmd, ps, es)"]
    F --> G["return cmd"]

```

## parseline

```mermaid
graph TD
    A["parsepipe(ps, es)"]
    A --> B("the first char of next token is '&'")
    B --> |yes| C["gettoken(ps, es, 0, 0)"]
    C --> D["cmd = backcmd(cmd)"]
    D --> B
    B --> |no| E("the first char of next token is ';'")
    E --> |yes| F["gettoken(ps, es, 0, 0)"]
    F --> G["cmd = listcmd(cmd, parseline(ps, es))"]
    G --> H["retur cmd"]
```

The entrance is pareline. It would call `parsepipe()` first, this would generate a `struct pipcmd` then call peek to see wether the next token is `&`. If the next token is `&` it would call `backcmd()` to wrap the original cmd to `backcmd`.

The structure of a command is:
`pipecmd &`

The `parsepipe()` would calls `parseexec()` first to generate an `execcmd`. Then if the next token is `|` if would call pipecmd again.

The structure of a pipe command is:
`pipecmd : execcmd | pipecmd`

The `parseexec()` would call `peek()` to see if the next token is `(` if yes, it would treat `(...)` as an `execcmd` and just call `parseblock(ps, es)` to parse it, then return. If not, it would treate ret as an `redircmd`. Then if next token is `\0` just break, if not a `word`, `panic`. If is a `word` just let `argv` and `eargv` point to `q` and `eq`

The structure of a exec command is:
`execcmd: (...)` or `execcmd: redircmd args...`

The `parseredirs()`, while next token is in `<>`, it would call `gettoken(ps, es, 0, 0)` to store that token into `tok`. after that, if next token is not a `word` do `panic()`. else `switch(tok)` if `<, >, >>` call `redircmd()` to construct a `redircmd`.  

The structure of a redirect command is:
`redircmd: < word` or `> word` or `>> word`

```mermaid
graph TD
    A["parseline(ps, es)"]
    A --> B["parsepipe(ps, es)"]
    B --> C("next token is '&'")
    C --> |yes| D["cmd = backcmd(cmd)"]
    D --> C --> |no| E("next token is ';'")
    E --> |yes| F["gettoken(ps, es, 0, 0), cmd = listcmd(cmd, parseline)"]
    E --> |no| G["return cmd"]
    F --> G

    B --> A1["parseexec(ps, es)"]
    A1 --> B1("next token is '|'")
    B1 --> |yes| C1["gettoken(ps, es, 0, 0)"]
    C1 --> D1["cmd = pipecmd(cmd, parsepipe(ps, es))"]
    B1 --> |no| E1["return cmd"]
    D1 --> E1["return cmd"]

    A1 --> A2("the next token is '('")
    A2 --> |yes| B2["return parseblock(ps, es)"]
    A2 --> |no| C2["parseredirs(ret, ps, es), ret is an empty execcmd"]
    C2 --> D2("next token is not in '|)&;'")
    D2 --> |yes| E2("next token is '\0'")
    E2 --> |yes| F2["cmd->argv[argc]=0, cmd->eargv[argc]=0, return ret"]
    E2 --> |no| G2("next token is not a word")
    G2 --> |yes| H2["panic"]
    G2 --> |no| I2["cmd->argv[argc] = q, cmd->eargv[argc] = eq, argc++"]
    I2 --> J2("if argc >= MAXARGS")
    J2 --> |yes| K2["panic too many args"]
    J2 --> |no| L2["ret = parseredirs(ret, ps, es)"]
    L2 --> D2
    D2 --> |no| M2["cmd->argv[argc] = 0, cmd->eargv[argc]=0, return ret"]
```

# Summary

`cmd`: `backcmd`, `listcmd`, `pipecmd`, `execcmd`, `redircmd`

`cmd`: `pipecmd [&]* [; cmd]?`, This the `cmd` structure.

listcmd: `pipecmd [&]* (; cmd)+`

backcmd: `cmd [&]*`

block: `(cmd) redirs*`

pipecmd: `(execcmd or block) | pipecmd`

execcmd: `args`

redircmd: `redirs* (execcmd redirs*)*`

redircmd: `(cmd) redirs*`






