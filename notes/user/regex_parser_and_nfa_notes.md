# 从上下文无关文法到 C 语言 Regex → NFA：理论、解析与工程重构

> 面向读者：已经会 C、栈、指针和 NFA，也能理解 `Frag`、`Ptrlist`、`patch` 等底层实现，但尚未系统学习编译原理。

## 目录

1. [先建立全局视图](#1-先建立全局视图)
2. [形式语言与上下文无关文法](#2-形式语言与上下文无关文法)
3. [推导、Parse Tree 与 AST](#3-推导parse-tree-与-ast)
4. [二义性、优先级与结合性](#4-二义性优先级与结合性)
5. [Parser 的两条主线：LL 与 LR](#5-parser-的两条主线ll-与-lr)
6. [递归下降 Parser](#6-递归下降-parser)
7. [Shift-Reduce 与显式栈](#7-shift-reduce-与显式栈)
8. [FIRST、FOLLOW 与 Parser 状态机](#8-firstfollow-与-parser-状态机)
9. [Operator Precedence 与 Shunting-yard](#9-operator-precedence-与-shunting-yard)
10. [Lexer 与 Token](#10-lexer-与-token)
11. [语义动作：从语法结构到计算结果](#11-语义动作从语法结构到计算结果)
12. [Thompson NFA、Frag、Ptrlist 与 patch](#12-thompson-nfafragptrlist-与-patch)
13. [把所有概念映射回现有双栈代码](#13-把所有概念映射回现有双栈代码)
14. [不变量、错误处理与内存所有权](#14-不变量错误处理与内存所有权)
15. [测试策略](#15-测试策略)
16. [推荐重构路线](#16-推荐重构路线)
17. [最终心智模型与速查表](#17-最终心智模型与速查表)

---

## 1. 先建立全局视图

一个 regex 系统里有两层不同的“语言”，必须先分清。

第一层是 **regex 源代码的语言**。例如：

```text
a
a|b
(a|b)*
```

这些字符串是否符合 regex 的书写规则，由 Parser 判断。`a||b`、`|a`、`()` 是否允许，也属于这一层。

第二层是 **regex 所描述的目标语言**。例如：

```text
a|bc  描述集合 { "a", "bc" }
a*    描述集合 { "", "a", "aa", ... }
```

这一层由生成出的 NFA 去识别。

因此完整系统应该理解为：

```text
regex 源字符串
    ↓ Lexer
Token 流
    ↓ Parser
AST（结构化 regex）
    ↓ Thompson NFA Builder
NFA
    ↓ Matcher
判断目标字符串是否属于 regex 描述的语言
```

你的现有 `re2nfa()` 同时做了字符读取、语法判断、优先级处理、NFA 构造和错误处理。它并非“算法不对”，而是把前端与后端压在了同一个控制流中，使局部正确很难组合成全局正确。

---

## 2. 形式语言与上下文无关文法

### 2.1 字母表、字符串与语言

字母表是有限符号集合，通常记作 `Σ`：

```text
Σ = { a, b }
```

由这些符号构成的有限序列叫字符串。空字符串记作 `ε`。`Σ*` 表示由 `Σ` 中符号组成的所有有限字符串。

语言就是字符串的集合：

```text
L = { aⁿ | n ≥ 0 } = { ε, a, aa, aaa, ... }
```

合法 C 程序是一个语言；合法 regex 源代码也是一个语言。

### 2.2 文法是什么

文法不是一段解析代码，而是一组描述“合法结构如何组成”的规则。上下文无关文法 CFG 通常写为：

```text
G = (N, Σ, P, S)
```

- `N`：非终结符集合，例如 `Alt`、`Concat`、`Repeat`、`Atom`。
- `Σ`：终结符集合，即真正出现在输入中的 token。
- `P`：产生式集合，例如 `Atom → CHAR`。
- `S`：开始符号，例如 `Regex`。

“上下文无关”是指每条产生式左边只有一个非终结符：

```text
A → α
```

只要出现 `A`，它就能按同一组规则展开，不需要查看 `A` 两侧是什么。

### 2.3 一个适合本项目的 regex 文法

用 EBNF 写：

```text
Regex      := Alt
Alt        := Concat ('|' Concat)*
Concat     := Repeat+
Repeat     := Atom Quantifier?
Quantifier := '*' | '+' | '?'
Atom       := CHAR | '(' Alt ')'
```

这里 EBNF 中的 `*`、`+`、`?` 是“重复零次以上、一次以上、可选”的元记号；引号内的 `'*'`、`'+'`、`'?'` 才是 regex token。

这份文法同时作出了几项语言设计决定：

- 空 regex 不合法；
- 空组 `()` 不合法；
- 一个 Atom 后最多跟一个量词，所以 `a**`、`a+?` 不合法；
- 连接不写成显式字符，而由相邻的 `Repeat` 表示；
- 括号里允许完整的 `Alt`，所以能嵌套。

文法不是天生唯一的。若你希望空 regex 表示 `ε`，或允许连续量词，就要修改文法及相应语义，而不是悄悄在代码中放行。

### 2.4 文法如何编码优先级

文法层次是：

```text
Alt
└── Concat
    └── Repeat
        └── Atom
```

越靠下的结构结合越紧，因此：

```text
括号/字符 > * + ? > 隐式连接 > |
```

`a|bc*` 必然形成：

```text
a | (b (c*))
```

你代码中的 `priority('|') = 1` 和 `priority('.') = 2`，本质上是在用数字压缩这份文法层次。

---

## 3. 推导、Parse Tree 与 AST

### 3.1 推导

推导是从开始符号出发，反复应用产生式，最终得到终结符串的过程。

以 `a|bc*` 为例，可概括为：

```text
Regex
⇒ Alt
⇒ Concat '|' Concat
⇒ Repeat '|' Repeat Repeat
⇒ Atom '|' Atom Atom '*'
⇒ a '|' b c '*'
```

每次优先展开最左边非终结符叫最左推导；优先展开最右边非终结符叫最右推导。LL Parser 与最左推导关系密切；LR Parser 可以理解为构造最右推导的逆过程。

### 3.2 Parse Tree（解析树）

解析树保留完整的文法结构：

```text
Regex
└── Alt
    ├── Concat
    │   └── Repeat
    │       └── Atom
    │           └── CHAR(a)
    ├── '|'
    └── Concat
        ├── Repeat
        │   └── Atom
        │       └── CHAR(b)
        └── Repeat
            ├── Atom
            │   └── CHAR(c)
            └── '*'
```

它适合解释“这个字符串如何由文法产生”，但对生成 NFA 来说太冗长。

### 3.3 AST（抽象语法树）

AST 去掉只为文法服务、对后续语义无用的节点和标点：

```text
        ALT
       /   \
LITERAL(a) CONCAT
           /   \
  LITERAL(b)  STAR
              |
         LITERAL(c)
```

Parse Tree 回答“用了哪些产生式”；AST 回答“这个程序是什么结构”。

一种简单的 C 表示是：

```c
typedef enum {
    NODE_LITERAL,
    NODE_ALT,
    NODE_CONCAT,
    NODE_STAR,
    NODE_PLUS,
    NODE_OPTIONAL
} NodeType;

typedef struct AST AST;

struct AST {
    NodeType type;
    unsigned char value;
    AST *left;
    AST *right;
};
```

二元节点 `ALT`、`CONCAT` 使用左右孩子；一元节点使用一个孩子；字面量保存值。

AST 的关键价值不是“多建一棵树”，而是建立清晰边界：Parser 负责证明语法并恢复结构，NFA Builder 负责解释结构。这样即使输入在末尾才报错，也不会留下一个半构造的、难清理的 NFA。

---

## 4. 二义性、优先级与结合性

如果同一个字符串能对应多棵解析树，文法就是二义的。

下面这份文法简短，但不适合直接指导可靠实现：

```text
E → E '|' E
E → E E
E → E '*'
E → '(' E ')'
E → CHAR
```

例如 `a|bc` 既可能被解释为 `a|(bc)`，也可能解释为 `(a|b)c`。`a|b|c` 也可能左结合或右结合。

解决方法有两类：

1. 改写文法，用 `Alt → Concat → Repeat → Atom` 的层级明确优先级和结合性。
2. 在 Operator Precedence/LR 工具中另行声明运算符的优先级与结合性。

本项目建议：

```text
量词：后缀一元，最高
连接：二元，次高，左结合
选择：二元，最低，左结合
括号：覆盖默认结合
```

即使选择与连接在集合语言意义上常有结合律，Parser 仍应生成确定结构；确定性会简化调试、打印、优化和测试。

---

## 5. Parser 的两条主线：LL 与 LR

### 5.1 自顶向下：LL / 递归下降

自顶向下从开始符号出发，预测输入应符合哪条产生式：

```text
Regex → Alt → Concat → Repeat → Atom
```

LL(1) 中：

- 第一个 `L`：从左到右读取输入；
- 第二个 `L`：构造最左推导；
- `1`：通常只看一个向前看 token。

递归下降是最直观的手写 LL 风格：每个非终结符对应一个函数，C 调用栈保存解析上下文。

### 5.2 自底向上：LR / Shift-Reduce

自底向上从 token 开始，把已经识别的片段逐步归约成更大的非终结符：

```text
CHAR(a) → Atom → Repeat → Concat → Alt → Regex
```

LR(1) 中：

- `L`：从左到右读取输入；
- `R`：构造最右推导的逆过程；
- `1`：使用一个向前看 token。

严格的 LR Parser 不只是“一个符号栈加若干 if”。它通常有 LR 状态、状态栈、ACTION/GOTO 表；状态表示“到目前为止可能处于哪些产生式位置”。

### 5.3 两者与当前项目的关系

- `parse_alt()`、`parse_concat()` 等属于递归下降路线。
- `opstack + fragstack` 更接近运算符优先级解析，是自底向上思想的专用子集。
- 它有 Shift/Reduce 的味道，但不是完整 LR Parser；不要把简单双栈等同于 LR 表驱动解析。

对这个小型 regex 语法，两条路线都可行。递归下降最适合作为容易验证的基准实现；Shunting-yard 最适合保留“显式栈、无递归”的学习目标。

---

## 6. 递归下降 Parser

### 6.1 消除左递归

直观文法可能写成：

```text
Alt → Alt '|' Concat | Concat
```

直接递归下降会调用 `parse_alt()` 自己而不消费输入，造成无限递归。改写为 EBNF：

```text
Alt := Concat ('|' Concat)*
```

连接同理：

```text
Concat := Repeat+
```

### 6.2 函数与文法一一对应

```c
AST *parse_expr(Parser *p);    /* Expr   := Alt */
AST *parse_alt(Parser *p);     /* Alt    := Concat ('|' Concat)* */
AST *parse_concat(Parser *p);  /* Concat := Repeat+ */
AST *parse_repeat(Parser *p);  /* Repeat := Atom Quantifier? */
AST *parse_atom(Parser *p);    /* Atom   := CHAR | '(' Expr ')' */
```

调用层次本身就编码了优先级：

```text
parse_alt
└── parse_concat
    └── parse_repeat
        └── parse_atom
            └── parse_expr  （括号内递归）
```

### 6.3 关键逻辑

`parse_atom()`：

```c
static AST *parse_atom(Parser *p)
{
    Token t = peek(p);

    if (t.type == TOK_CHAR) {
        consume(p);
        return ast_literal(p, t.value);
    }

    if (t.type == TOK_LPAREN) {
        consume(p);
        AST *node = parse_expr(p);
        if (node == NULL)
            return NULL;
        if (!match(p, TOK_RPAREN))
            return parser_fail(p, RE_ERR_MISSING_RPAREN,
                               "expected ')'");
        return node;
    }

    return parser_fail(p, RE_ERR_EXPECTED_ATOM,
                       "expected character or '('");
}
```

`parse_repeat()` 先得到 Atom，再按设计允许零个或一个量词。读完量词后若又见量词，应报告 duplicate quantifier。

`parse_concat()` 不需要真的读到 `.`：只要下一个 token 属于 `FIRST(Atom)`，就说明发生隐式连接。

```c
while (starts_atom(peek(p).type)) {
    AST *right = parse_repeat(p);
    left = ast_binary(p, NODE_CONCAT, left, right);
}
```

`parse_alt()` 在看到 `|` 后必须成功解析右侧 `Concat`：

```c
while (match(p, TOK_OR)) {
    AST *right = parse_concat(p);
    left = ast_binary(p, NODE_ALT, left, right);
}
```

顶层解析结束必须确认当前 token 是 `TOK_END`。否则像 `a)` 这样的输入会错误地被当作成功解析了前缀 `a`。

### 6.4 为什么它适合作为第一版

- 文法与代码几乎逐行对应；
- 报错位置自然；
- 不需要自己实现完整的 LR 状态机；
- AST 可以单独打印和测试；
- 能作为显式栈版本的差分测试基准。

递归下降使用 CPU 调用栈；显式栈版本只是把这些上下文改为自己管理。二者不是理论对立，而是状态保存方式不同。

---

## 7. Shift-Reduce 与显式栈

### 7.1 Shift 与 Reduce

Shift（移进）把一个新 token 纳入已处理部分：

```text
栈              输入
[]              a|b$
[CHAR(a)]       |b$       shift a
```

Reduce（归约）把栈顶符合某个产生式右部的部分替换为左部：

```text
CHAR(a)  → Atom
Atom '*' → Repeat
Expr '|' Term → Expr
```

被归约的那段右部称为 handle。真正困难不在弹栈，而在确定“此刻哪一段才是正确 handle”。LR 状态和 lookahead 正是用来做这个决定的。

### 7.2 栈元素需要语法身份和语义值

只有：

```c
Frag fragstack[MAXSTACK];
```

只能证明“栈里有一个 NFA 片段”，不能证明“这个片段在当前输入位置是合法的右操作数”。例如 `a|*b` 读到 `*` 时，栈里仍有 `Frag(a)`，所以单看 `popfrag()` 会误把星号作用于 `a`。

更一般的 Parser 栈元素应像：

```c
typedef struct {
    Symbol symbol;       /* CHAR, ATOM, REPEAT, CONCAT, ALT, OR... */
    SemanticValue value; /* AST* 或 Frag */
    size_t position;
} StackEntry;
```

若采用 Shunting-yard 专用双栈，可以不保存完整非终结符层次，但必须另外保存明确的 Parser 状态，如 `EXPECT_ATOM` / `EXPECT_OPERATOR`，不能让“栈是否下溢”代替语法判断。

### 7.3 双栈不是错误，但要有同步规则

经典运算符解析使用：

- 运算符栈：暂时不能执行的 `(`、`.`、`|`；
- 操作数/值栈：已经完成的子表达式，可为 AST 节点或 `Frag`。

风险是两个栈可能在错误路径或复制的 reduce 逻辑中不同步。必须把二元归约集中到一个函数中，并让每次归约原子化地完成：检查两个操作数、构造结果、再更新栈。

---

## 8. FIRST、FOLLOW 与 Parser 状态机

### 8.1 FIRST：一种结构能从什么开始

```text
Atom → CHAR | '(' Alt ')'
```

所以：

```text
FIRST(Atom) = { CHAR, LPAREN }
```

由于 `Repeat`、`Concat`、`Alt` 都从 Atom 开始，在当前不允许空表达式的文法中：

```text
FIRST(Repeat) = FIRST(Concat) = FIRST(Alt)
              = { CHAR, LPAREN }
```

这立刻解释了为什么 `|a`、`*a`、`)` 不能出现在表达式开头。

### 8.2 FOLLOW：一种结构之后允许出现什么

FOLLOW 集描述某个非终结符完成后，可能紧随其后的 token。

在本语法中：

- 完整 `Alt` 后可能是 `)` 或 `END`；
- 一个 `Concat` 后可能是 `|`、`)` 或 `END`；
- 一个 `Repeat` 后可能开始下一个 Atom，或出现 `|`、`)`、`END`；
- 一个 `Atom` 后可能先出现量词，也可能直接进入 `Repeat` 的后继位置。

注意：“下一个 Atom 的开头也可能跟在 Repeat 后”并不表示它属于同一个 Repeat；它意味着 Parser 应结束当前 Repeat，并在 Concat 层继续。

### 8.3 FIRST/FOLLOW 的工程压缩：两态 Parser

对这个小语法，可以把大量集合判断压缩成两个主要状态：

```c
typedef enum {
    EXPECT_ATOM,
    EXPECT_OPERATOR
} ParseMode;
```

`EXPECT_ATOM`：

```text
允许：CHAR, '('
拒绝：'|', '*', '+', '?', ')', END
```

`EXPECT_OPERATOR`：

```text
允许：'*', '+', '?', '|', ')', END
若见 CHAR 或 '('：先插入隐式连接，再把它作为新 Atom 处理
```

还应记录 `last_was_quantifier` 或等价信息，以按当前文法拒绝 `a**`、`a+?`。

例：`a|*b`：

```text
开始       EXPECT_ATOM
读 a       EXPECT_OPERATOR
读 |       EXPECT_ATOM
读 *       错误：此处必须开始一个 Atom
```

这比 `popfrag()` 失败才报错更早、更准确。Parser 状态不是拍脑袋设计的，而是 FIRST/FOLLOW 在这个小文法上的工程投影。

### 8.4 Parser 状态机与 LR 状态的区别

这里的两态机是专用语法检查器，足够处理当前运算符语法。LR 状态更强：它编码一组带“点”的产生式项目及当前识别位置，并通过 ACTION/GOTO 表选择 shift、reduce、accept 或 error。不要把 `EXPECT_ATOM` 两态机称作完整 LR Parser，但可以把它看作同一种“显式保存合法状态”思想的简化版。

---

## 9. Operator Precedence 与 Shunting-yard

### 9.1 核心思路

Shunting-yard 把中缀表达式转成后缀表达式，或在相同归约时机直接构造 AST/Frag。

推荐先显式分两阶段：

```text
中缀 regex
    ↓ 补隐式连接 + Shunting-yard
后缀 regex
    ↓ Thompson 栈机
NFA
```

例如：

```text
输入：      a|(b?c)|d
显式连接：  a|(b?.c)|d
后缀：      ab?c.|d|
```

后缀表达式已经消除了括号和二元运算符优先级，第二阶段只需要 `fragstack`。

### 9.2 隐式连接的可靠判定

若前一个 token 能结束表达式，当前 token 能开始表达式，则两者之间存在连接：

```c
static bool can_end_expr(TokenType t)
{
    return t == TOK_CHAR || t == TOK_RPAREN ||
           t == TOK_STAR || t == TOK_PLUS || t == TOK_QUESTION;
}

static bool can_start_expr(TokenType t)
{
    return t == TOK_CHAR || t == TOK_LPAREN;
}
```

```text
需要连接：ab、a(、)a、)(、a*b、a?(
不需连接：a|、|a、(a、a)、(*
```

也可以用 `EXPECT_OPERATOR && can_start_expr(curr)` 判断；关键是依据 token 关系或语法状态，而不是“看到一个新字符就猜测连接”。

### 9.3 运算符规则

```text
* + ?   后缀一元；读到即可输出/应用
.       二元；优先级 2；左结合
|       二元；优先级 1；左结合
(       屏障，不参与普通优先级比较
```

当读入新的左结合二元运算符 `incoming` 时：

```text
只要栈顶不是 '('，且 precedence(top) >= precedence(incoming)：
    弹出并输出/归约 top
最后压入 incoming
```

遇到 `)` 时一直弹到 `(`；若没找到 `(`，是多余右括号。输入结束时清空栈；若残留 `(`，是缺少右括号。

### 9.4 为什么建议先生成 postfix

“一边解析一边造 NFA”并非理论上错误，它属于语法制导翻译。但分成 postfix 与 NFA 两步后：

- 语法错误不会污染 NFA；
- 优先级问题可单独测试；
- Thompson 阶段只有固定栈效应；
- 能检查后缀输出是否正确，而不必读 NFA 图；
- 仍然满足显式栈、无递归的实现目标。

若项目后续要做优化、打印、源码位置诊断或多种后端，则应把 postfix 换成 AST。

---

## 10. Lexer 与 Token

直接对 `char` 做大 `switch` 在最小语法里能工作，但会让转义、字符类、Unicode 或更精确错误位置迅速污染 Parser。

Lexer 的唯一职责是：

```text
字符流 → Token 流
```

```c
typedef enum {
    TOK_CHAR,
    TOK_OR,
    TOK_STAR,
    TOK_PLUS,
    TOK_QUESTION,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_END,
    TOK_INVALID
} TokenType;

typedef struct {
    TokenType type;
    unsigned char value;
    size_t position;
} Token;
```

例如：

```text
a|(bc)*
```

变成：

```text
CHAR(a), OR, LPAREN, CHAR(b), CHAR(c), RPAREN, STAR, END
```

Token 的 `position` 让错误可以显示为：

```text
a|(b|)
     ^
error: expected expression after '|'
```

以后支持 `\.` 时，Lexer 可把两个输入字节转成一个 `TOK_CHAR('.')`，Parser 无须改变。

工程上可以选择“先生成 Token 数组”或“Lexer 按需返回下一个 Token”。初版数组更易调试；流式 Lexer 更省中间存储。无论哪种，都应保证存在明确的 `TOK_END`。

---

## 11. 语义动作：从语法结构到计算结果

产生式只描述结构；语义动作规定识别该结构后要计算什么。

例如连接规则：

```text
Concat → Concat Repeat
```

若语义值是 AST：

```c
result = ast_binary(NODE_CONCAT, left, right);
```

若语义值直接是 NFA `Frag`：

```c
patch(left.out, right.start);
result = frag(left.start, right.out);
```

选择规则对应：

```c
State *s = state(SPLIT, left.start, right.start);
result = frag(s, append(left.out, right.out));
```

这叫语法制导翻译。你当前代码中的 `state()`、`patch()`、`append()`、`frag()` 并不是 Parser 规则本身，而是归约某种语法结构时触发的语义动作。

分层以后，同一个 Parser 可以：

- 构造 AST；
- 输出 postfix；
- 直接构造 NFA；
- 或仅做语法检查。

这就是把“识别结构”和“解释结构”解耦的价值。

---

## 12. Thompson NFA、Frag、Ptrlist 与 patch

### 12.1 Thompson 构造的核心

Thompson 方法为每个 regex 结构生成一个 NFA 子图，并通过 `Split` 表示 ε 分支。最后把所有未决出口接到唯一的 `Match` 状态。

```c
typedef struct State State;

struct State {
    int c;          /* 字符、SPLIT 或 MATCH */
    State *out;
    State *out1;
};
```

### 12.2 Frag 不是“一个状态”

```c
typedef struct {
    State *start;
    Ptrlist *out;
} Frag;
```

`Frag` 表示一个部分完成的子图：

- `start`：子图入口；
- `out`：所有尚未知道目标的指针槽位集合。

“未决出口”不是目标状态，而是以后要被写入的槽位地址。例如 `s->out` 尚为 `NULL`，应记录的是：

```c
&s->out       /* 类型是 State ** */
```

之后：

```c
patch(list, target);
```

等价于对表中每个槽位执行：

```c
*slot = target;
```

### 12.3 清晰版 Ptrlist

初版推荐显式表示，不要先使用复用指针槽内存的技巧：

```c
typedef struct Ptrlist Ptrlist;

struct Ptrlist {
    State **slot;
    Ptrlist *next;
};
```

```c
static void patch(Ptrlist *list, State *target)
{
    while (list != NULL) {
        Ptrlist *next = list->next;
        *list->slot = target;
        free(list);
        list = next;
    }
}
```

某些经典实现把 `State **` 槽位本身临时解释成 `Ptrlist` 节点，以省掉分配。这是紧凑的低层技巧，但可读性差，也涉及对象表示、别名及生命周期推理。先使用清晰表示并建立正确性，再决定是否值得优化。

### 12.4 六种构造规则

以下用 `list1(&s->out)` 表示只含一个未决槽位的链表。

#### 字面量

```c
State *s = state(ch, NULL, NULL);
return frag(s, list1(&s->out));
```

#### 连接 `xy`

```c
patch(x.out, y.start);
return frag(x.start, y.out);
```

#### 选择 `x|y`

```c
State *s = state(SPLIT, x.start, y.start);
return frag(s, append(x.out, y.out));
```

#### 星号 `x*`

```c
State *s = state(SPLIT, x.start, NULL);
patch(x.out, s);
return frag(s, list1(&s->out1));
```

入口是 `Split`，所以可以执行零次；`x` 的出口回到 `Split`，所以可以重复。

#### 加号 `x+`

```c
State *s = state(SPLIT, x.start, NULL);
patch(x.out, s);
return frag(x.start, list1(&s->out1));
```

入口是 `x.start` 而不是 `Split`，所以至少执行一次。

#### 可选 `x?`

```c
State *s = state(SPLIT, x.start, NULL);
return frag(s, append(x.out, list1(&s->out1)));
```

一条分支进入 `x`，另一条未决分支跳过 `x`。

最终：

```c
Frag e = build(ast);
State *m = state(MATCH, NULL, NULL);
patch(e.out, m);
return e.start;
```

### 12.5 后缀栈机的栈效应

```text
CHAR：0 → 1
* + ?：1 → 1
. |：2 → 1
结束：必须恰好剩 1 个 Frag
```

这些栈效应很适合写断言和属性测试。若结束时不是一个 Frag，说明 postfix 非法或实现有 bug。

---

## 13. 把所有概念映射回现有双栈代码

现有结构大致为：

```c
char opstack[MAXSTACK];
Frag fragstack[MAXSTACK];

int re2nfa(char *re, State **start_node);
```

它可以用编译原理术语重新解释：

| 当前代码元素 | 理论角色 | 当前风险 | 推荐归宿 |
|---|---|---|---|
| `for` + `switch (*p)` | Lexer 与 Parser 混合 | 转义、位置、语法状态混杂 | 独立 Lexer 生成 Token |
| `opstack` | 待处理运算符/语法上下文 | 括号、优先级逻辑分散 | Shunting-yard 的运算符栈 |
| `fragstack` | 语义值栈 | 有 Frag 不代表语法合法 | 仅用于合法 postfix → NFA |
| `priority()` | 文法优先级的数值编码 | 若无结合性/状态规则仍会错 | 集中定义二元运算符规则 |
| 隐式 `.` | `Concat` 产生式 | 插入时机容易误判 | 用 token 相邻关系统一插入 |
| `state()` | NFA 后端构造 | 分配失败、所有权不清 | NFA Builder + arena |
| `patch()` | Concat/Repeat 的语义动作 | 与解析流程纠缠 | 独立 Frag 组合函数 |
| `append()` | 合并未决出口集合 | 链表表示过于技巧化 | 清晰 `slot + next` 初版 |
| 多处 pop/reduce | 归约 | 复制逻辑、可能重复弹栈 | 单一 `reduce_binary()` 或 postfix 阶段 |
| `return -1` | 所有错误 | 无法区分语法/OOM/内部错误 | 结构化错误对象 |
| 全局栈指针 | Parser 实例状态 | 失败后污染下一次调用 | 放进局部 Context |

### 13.1 当前代码常见问题

#### 语法合法性依赖栈下溢

`a|*b` 中，`*` 仍可能从 frag 栈弹到 `a`。这说明“有操作数”不等于“当前位置允许一元后缀运算符”。必须先用 Parser 状态验证 token。

#### 归约逻辑散落

在遇到 `|`、`)`、新 Atom 和输入结束时都可能需要归约。如果四处复制 `popfrag → patch/state → pushfrag`，某一处很容易弹栈顺序相反、弹两遍、漏检查返回值或忘记某个运算符。

#### 隐式连接依赖局部猜测

看到字符 `b` 时，只有前一个 token 能结束表达式才应插入 `.`。`a b` 要连接，`a|b` 中的 `| b` 不应连接。

#### 括号只被当作计数问题

括号不是简单的数量配对。`()`、`(a|)`、`(|a)` 数量都可能配对，但语法仍不合法。`(` 的真正含义是暂停外部归约并开始新的表达式上下文；`)` 要求内部表达式已经完整。

#### Parser 与 NFA 部分的失败路径纠缠

若扫描到末尾才发现缺少 `)`，此时可能已分配很多 State。必须明确清理策略，否则一次语法错误既泄漏内存，也可能留下全局栈残余。

#### 过早使用紧凑指针技巧

复用 `State **` 槽位保存 `Ptrlist` 链表很聪明，但会把算法理解、C 对象表示和优化绑在一起。它不应成为验证 Parser 正确性的前提。

### 13.2 两种合理目标架构

#### 推荐工程版

```text
source → Lexer → Token[] → Recursive-descent Parser → AST
       → Thompson Builder → NFA → Matcher
```

优点：边界清楚，最容易扩展和诊断。

#### 推荐显式栈学习版

```text
source → Lexer → Token[] → Syntax validator + Shunting-yard → Postfix
       → Thompson postfix stack machine → NFA → Matcher
```

优点：无递归，完整保留双栈思想，而且每一步可独立验证。

不建议继续维持的版本是：

```text
char → 同一个 switch 同时猜语法、比较优先级、弹 Frag、创建 State
```

---

## 14. 不变量、错误处理与内存所有权

### 14.1 Parser 不变量

扫描每个 token 前后都应成立：

- 当前下标始终指向一个有效 token，最终由 `TOK_END` 封口；
- `EXPECT_ATOM` 时只接受 `CHAR` 或 `(`；
- `EXPECT_OPERATOR` 时才允许量词、`|`、`)` 或结束；
- 一个 `|` 被接受后，状态立即变成 `EXPECT_ATOM`；
- 每个 `)` 必须对应尚未关闭的 `(`；
- 当前语言若不允许连续量词，量词后不得再读量词；
- 成功时输入必须完全消费，不能只解析合法前缀。

### 14.2 运算符栈不变量

- 只保存 `(`、`.`、`|`；量词直接输出或应用；
- `(` 是优先级屏障；
- 每个括号区间内，栈顺序满足 Shunting-yard 的维护规则；
- 弹出二元运算符时，值栈必须至少有两个值；
- 解析结束后运算符栈为空且值栈恰有一个结果。

### 14.3 Frag 不变量

对每个成功构造的 `Frag f`：

1. `f.start != NULL`；
2. `f.out` 精确包含所有尚未连接的出口槽位；
3. `f.out` 中每个槽位当前为 `NULL`；
4. 从 `f.start` 出发的每条未完成路径最终到达 `f.out` 中某个槽位；
5. 已经被 `patch()` 消费的 Ptrlist 不再使用。

每个组合函数都应被当作“不变量保持证明”：输入 Frag 满足条件，组合后的 Frag 仍满足条件。

### 14.4 State 不变量

可按类型规定：

- 字符状态：`out` 可指向后继，`out1 == NULL`；
- `SPLIT`：`out`、`out1` 表示两条 ε 边；构造中允许其中一个暂时未决；
- `MATCH`：`out == NULL && out1 == NULL`；
- 完成 NFA 后，除 `MATCH` 外不应残留未决出口。

调试构建中应尽量用 `assert()` 检查内部不变量。用户输入错误不应触发断言，而应返回正常错误；断言用于发现程序员错误。

### 14.5 结构化错误

不要让所有失败都变成 `-1`：

```c
typedef enum {
    RE_OK,
    RE_ERR_INVALID_CHAR,
    RE_ERR_EXPECTED_ATOM,
    RE_ERR_MISSING_OPERAND,
    RE_ERR_MISSING_RPAREN,
    RE_ERR_UNEXPECTED_RPAREN,
    RE_ERR_DUPLICATE_QUANTIFIER,
    RE_ERR_STACK_OVERFLOW,
    RE_ERR_OUT_OF_MEMORY,
    RE_ERR_INTERNAL
} ReErrorCode;

typedef struct {
    ReErrorCode code;
    size_t position;
    const char *message;
} ReError;
```

公共函数还应规定失败后的输出：

```c
ReError re_compile(const char *source, NFA **out_nfa);
```

进入函数先令 `*out_nfa = NULL`；只有完全成功才交付 NFA。

### 14.6 内存所有权

对每类对象回答三个问题：谁创建、谁拥有、谁释放。

推荐为 AST 和 State 分别使用 arena：每次分配都登记，成功时所有权转交给 AST/NFA 对象，失败时统一释放。图结构可能共享节点，从入口递归 `free()` 容易重复释放；arena 更简单可靠。

若仍用全局栈，一次失败可能污染下一次解析。更好的做法是：

```c
typedef struct {
    Lexer lexer;
    Token current;
    ParseMode mode;
    OperatorStack operators;
    ValueStack values;
    ReError error;
    Arena arena;
} CompileContext;
```

每次编译创建独立 context，成功或失败都由一个清理出口收尾。

---

## 15. 测试策略

### 15.1 按层测试，而不是只测最终图

#### Lexer

- 普通字符、每个运算符、END；
- 非法字符；
- 转义（加入该功能后）；
- 每个 token 的位置。

#### Parser / AST

输入 `a|bc*`，断言结构是：

```text
ALT(LITERAL(a), CONCAT(LITERAL(b), STAR(LITERAL(c))))
```

不要只断言“解析成功”。

#### Shunting-yard

对固定输入断言 postfix：

```text
ab        → ab.
a|bc*     → abc*.|
(a|b)c    → ab|c.
```

#### Thompson Builder

可以绕过 Parser，直接构造 AST 或输入已知 postfix，单测 literal、concat、alt、star、plus、optional 六种组合。

#### Matcher

通过可接受语言验证最终语义，比比较 State 分配顺序更稳健。

### 15.2 合法语法分类

```text
最小：      a
连接：      ab, abc
量词：      a*, a+, a?, ab*, a*b
选择：      a|b, a|b|c, ab|cd, a|bc
括号：      (a), (ab), (a|b), ((a))
组合：      a|(b?c)|d, (a|b)*(c|d)+, a?(bc)*d+
```

### 15.3 非法语法分类

```text
非法开头：  *a, +a, ?a, |a, )
非法结尾：  a|, (a, a(
非法中间：  a||b, (), (a|), (|a), a**, a+?, a(*b)
括号错误：  a), ((a), (a))
非法字符：  由 Lexer 规则决定
```

每个非法用例应同时断言错误码和位置，而不只是“返回失败”。

### 15.4 语义测试

```c
struct MatchCase {
    const char *pattern;
    const char *text;
    bool expected;
};
```

例如：

```text
a+    × ""     ✓ "a"    ✓ "aa"
a*    ✓ ""     ✓ "a"    ✓ "aa"
a?    ✓ ""     ✓ "a"    × "aa"
a|bc  ✓ "a"    ✓ "bc"   × "b"
```

### 15.5 属性与差分测试

- 对小字母表穷举短 regex 和短目标串；
- 与成熟 regex 实现的共同语法子集比较 full-match 结果；
- 对同一个 token 流比较“递归下降 → AST → NFA”和“Shunting-yard → postfix → NFA”的匹配结果；
- fuzz 非法输入，要求不崩溃、不泄漏、错误位置在输入范围内；
- 在 Debug/CI 中启用 AddressSanitizer、UndefinedBehaviorSanitizer 和严格编译警告。

调用 `<ctype.h>` 函数时应写：

```c
isalpha((unsigned char)c)
```

避免有符号 `char` 产生未定义行为。

---

## 16. 推荐重构路线

不要在现有大 `switch` 上一次性做巨型重写。按可验证的小阶段推进。

### 阶段 0：冻结语言规格

先写清楚：

- 支持哪些字面量；
- 是否允许空 regex、空组；
- 是否允许连续量词；
- 转义规则；
- 优先级与结合性；
- 匹配是 full match 还是 search。

没有规格就无法区分“实现 bug”和“语言设计选择”。

### 阶段 1：提取 NFA 原语

把散落的语义动作集中为：

```c
make_literal_frag()
concat_frag()
alternate_frag()
star_frag()
plus_frag()
optional_frag()
finish_frag()
```

单独测试这些函数和 Frag 不变量。此时暂不改变解析算法。

### 阶段 2：引入 Token 与结构化错误

实现 Lexer，让 Parser 不再直接处理裸字符；每个 token 保存位置。把 `-1` 改为可区分的错误码，规定失败后的资源清理和输出值。

### 阶段 3：写一个基准 Parser

优先写递归下降 Parser，只生成 AST，不生成 NFA。打印或序列化 AST，并对优先级、结合性和错误位置做表驱动测试。

这一版是“可读的规格实现”，以后可用来验证显式栈版本。

### 阶段 4：AST → Thompson NFA

写纯后端遍历：

```c
BuildResult build_nfa(NfaBuilder *b, const AST *node);
```

Parser 不再调用 `state()`；Builder 不再读取 token。成功后统一 patch 到 Match。

### 阶段 5：实现 Matcher

实现 ε-closure、当前状态集、下一状态集和 Match 检测。用行为测试 NFA，而不是依赖肉眼看图。

### 阶段 6：保留你的显式栈目标

在基准版通过后，再实现：

```text
Token[] → 验证状态机 + Shunting-yard → postfix → NFA
```

将它与 AST 版做差分测试。这样你不是放弃双栈，而是在拥有标准答案后重新实现它。

### 阶段 7：工程强化

- 用动态 vector 或明确报告固定栈容量错误；
- 用 arena 管理 AST/State 生命周期；
- 所有失败路径做统一 cleanup；
- fuzz、Sanitizer、严格警告；
- 最后再评估 Ptrlist 内存复用等优化是否必要。

### 一个实用的模块拆分

```text
regex_token.h/.c      Token 与 Lexer
regex_ast.h/.c        AST 类型、构造和销毁
regex_parser.h/.c     Token → AST
regex_postfix.h/.c    可选：Token → postfix
nfa.h/.c              State、Frag、Ptrlist、Builder
nfa_match.h/.c        NFA 模拟执行
regex_error.h/.c      错误码与诊断
tests/                 分层测试与端到端测试
```

初版文件数不必机械照搬；重要的是依赖方向：

```text
Lexer → Parser → AST → NFA Builder → Matcher
```

后面的模块不应反过来知道 Parser 的内部栈。

---

## 17. 最终心智模型与速查表

### 17.1 一句话定义

| 概念 | 一句话理解 |
|---|---|
| 形式语言 | 字符串的集合 |
| CFG | 用非终结符产生式描述合法结构 |
| 推导 | 从开始符号应用规则生成字符串 |
| Parse Tree | 完整记录文法推导结构的树 |
| AST | 去掉无意义语法层后的语义结构 |
| 二义性 | 同一字符串对应多棵解析树 |
| 优先级 | 决定不同运算符谁先结合 |
| 结合性 | 同级运算符连续出现时如何分组 |
| Lexer | 字符流变 Token 流 |
| Parser | 验证 token 序列并恢复结构 |
| LL | 自顶向下预测最左推导 |
| LR | 自底向上构造最右推导的逆过程 |
| Shift | 把新 token 纳入解析栈 |
| Reduce | 把 handle 替换成产生式左部 |
| FIRST | 某结构可能以哪些 token 开始 |
| FOLLOW | 某结构之后可能出现哪些 token |
| Shunting-yard | 用运算符栈处理优先级并生成 postfix |
| 语义动作 | 识别结构时计算 AST、Frag 等值 |
| Thompson | 按 regex 结构组合 ε-NFA |
| Frag | 入口加全部未决出口组成的部分 NFA |
| Ptrlist | 保存未来要被写入的 `State **` 槽位 |
| patch | 把未决出口统一指向已知目标 |
| 不变量 | 每次操作前后都必须成立的正确性条件 |

### 17.2 现有代码真正已经做到的事

你的代码并不只是“把 regex 变成 NFA”。它已经包含了：

```text
词法判断
+ 运算符优先级解析
+ Shift/Reduce 风格的显式栈
+ 语法制导翻译
+ Thompson 图构造
+ backpatching
```

这说明困难主要不在 C 语法或指针能力，而在如何为复杂系统建立层次、状态、不变量、错误模型和可验证边界。

### 17.3 最重要的五个结论

1. Parser 识别 regex 源语言，NFA 识别 regex 描述的目标语言，两者不是同一个问题。
2. 优先级不是几个孤立数字；它是文法结构的压缩表示。
3. `Frag` 是语义值，不携带足够的语法合法性信息；有 Frag 不等于当前位置语法正确。
4. `patch()` 是 Thompson 语义动作，不应该承担 Parser 状态判断。
5. 从“看到字符就处理”升级到“当前处于什么合法状态、这个 token 允许什么转移”，是这次重构最关键的思维变化。

推荐的最终学习顺序是：

```text
写清语言规格
→ Lexer/Token
→ 递归下降与 AST
→ AST 到 Thompson NFA
→ Matcher 与系统测试
→ 再用 Shunting-yard 重写显式栈版本
→ 两种实现做差分验证
```

这样既能获得清晰、可扩展的工程架构，也能真正理解你最初想掌握的显式栈 Parser。
