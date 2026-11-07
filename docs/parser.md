# Fanta Front End: Lexer and Parser

A reference for how source text becomes an AST, written so you can rebuild the mental model
quickly. The code lives in `compiler/lexer.hpp`, `compiler/parser.{hpp,cpp}` and
`compiler/ast.hpp`. For a slower, step-by-step trace of expression parsing see
`docs/pratt_parser.md`.

This describes what is implemented today, not including the in-progress `@` intrinsics. The planned
language is in `docs/language_design.md`.

---

## 1. The big picture

```
source text ──Lexer::getToken()──▶ tokens ──Parser──▶ asts[] (flat arena) + roots[]
                                                   └─▶ SimpleIRPass walks roots
```

The parser is two techniques glued together:

| Part of the language       | Technique                     | Entry point            |
|----------------------------|-------------------------------|------------------------|
| Statements and declarations (`let`, `fn`, `if`, `for`, `return`, `{ }`) | Recursive descent, LL(1) | `walk()`, `walkBody()` |
| Expressions (`a + b * f(c)`) | Pratt (operator precedence) | `walkExpression(p)`    |

Statements are where the grammar is "the keyword tells you what comes next". Expressions are
where operator precedence matters, and that is what Pratt parsing is good at.

---

## 2. LL(1) in one page

**LL(1)** = scan **L**eft to right, build the **L**eftmost derivation, decide using **1** token
of lookahead.

In practice: one function per grammar rule, and each function picks what to do by looking only
at the *current* token. If you never need to look further ahead than one token to know which rule
applies, the grammar is LL(1). Fanta's statement grammar is designed that way: every statement
form starts with a distinct keyword, and anything else falls through to "it's an expression".

```
walkBody():
  current token is `let`    → walkLet()
  current token is `return` → walkReturn()
  current token is `if`     → walkIf()
  current token is `for`    → walkFor()
  anything else             → walkExpression(LOWEST)
```

### The token window

`Parser` holds two tokens:

- `currentToken`: the token being decided on. **All decisions look at this one.**
- `peekToken`: the next token, pre-fetched from the lexer. Nothing in the parser currently
  reads it to make a decision, so the parser is genuinely LL(1). It's there if a rule ever needs
  two tokens of lookahead (e.g. telling `ident :` from `ident (`).

`nextToken()` shifts the window: `currentToken = peekToken; peekToken = lexer.getToken()`.

### The two primitives

| Function     | If `currentToken.t == t`        | Otherwise                         |
|--------------|---------------------------------|-----------------------------------|
| `accept(t)`  | consume it, return `true`       | return `false`, consume nothing   |
| `expect(t)`  | consume it, return the token    | return `std::unexpected`, consume nothing |

`accept` is for **optional / branching** syntax ("is this an `else`?"). `expect` is for
**mandatory** syntax ("there must be a `)` here"). The `EXTRACT_TOKEN*` macros in `parser.cpp`
wrap `expect` and early-return on failure.

Note the keyword is consumed by `accept` *before* the `walkX()` call, so `walkLet()` starts on
the identifier, not on `let`.

---

## 3. Grammar

EBNF, matching what the code does today. `expr@P` means `walkExpression(P)`.

```ebnf
program     = { top_level } EOF ;
top_level   = "let" let_rest ";"
            | "fn" fn_rest
            | expr@LOWEST ";" ;

let_rest    = IDENT ":" TYPE "=" expr@ASSIGN_OR_RET ;
fn_rest     = IDENT "(" { IDENT ":" TYPE [ "," ] } ")" "->" TYPE body ;
body        = "{" { stmt ";" } "}" ;

stmt        = "let" let_rest
            | "return" expr@ASSIGN_OR_RET
            | "if" "(" expr@LOWEST ")" body [ "else" body ]
            | "for" "(" [ "let" let_rest ] ";" expr@LOWEST ";" expr@LOWEST [ ";" ] ")" body
            | expr@LOWEST ;
```

`TYPE` is one of `int float string long void` (the lexer emits `TokenType::Type` for these).

Quirks worth knowing:

- `walkBody` expects a `;` after **every** statement, including `if`/`for` blocks. A missing one
  is silently tolerated because the `expect` result is discarded (see §7).
- Statements after a `return` in the same body are parsed and then dropped.
- Function parameters accept an optional trailing comma.

---

## 4. Expressions: Pratt parsing

### Precedence table (`Parser::Precedence`, higher binds tighter)

| Value | Name            | Tokens                              |
|-------|-----------------|-------------------------------------|
| 0     | `LOWEST`        | `;` `)` and anything not listed     |
| 1     | `IFELSE`        | `if` `else`                         |
| 2     | `ASSIGN_OR_RET` | `=` `let` `return`                  |
| 3     | `LOGIC`         | `&&` `\|\|`                         |
| 4     | `BITWISE`       | `&` `\|`                            |
| 5     | `COMP`          | `==` `!=` `<` `<=` `>` `>=`         |
| 6     | `SUM` / `MINUS` | `+` `-`                             |
| 7     | `MULT`/`DIVIDE` | `*` `/`                             |
| 8     | `CALL`          | `(` (when it follows an expression) |

### The loop

```cpp
auto Parser::walkExpression(Precedence p) -> NodeIndex {
  auto prefix = walkPrefix();                       // 1. the thing an expression starts with
  while (p < getPrecedence(currentToken.t)) {       // 2. next token binds tighter than my limit?
    auto current = currentToken;
    nextToken();                                    //    consume the operator
    prefix = walkInfix(prefix, current);            // 3. fold it in, result becomes new LHS
  }
  return prefix;
}
```

The mental model: **`p` is how tightly the caller is holding on to the left operand.** An operator
only gets to steal the expression parsed so far if it binds *strictly* tighter than `p`.

- `walkInfix` for a binary operator parses its right side with `walkExpression(prec(op))`. Equal
  precedence does not pass `<`, so `1 - 2 - 3` stops after `2` and folds left:
  `(1 - 2) - 3`. That's how left associativity falls out for free.
- A token with no operator meaning (`;`, `)`, `,`, `{`) maps to `LOWEST`, which never beats any
  `p`, so it ends the expression without being consumed. The statement rule then `expect`s it.

### Prefix vs infix: the key distinction

| Position                       | Handled by    | Today supports                               |
|--------------------------------|---------------|----------------------------------------------|
| **Start** of an expression     | `walkPrefix`  | identifier, int literal, `( expr )` grouping |
| **After** a complete operand   | `walkInfix`   | binary operators, `(` as a call              |

`getPrecedence` is **only** consulted for tokens in the infix position. A token that can only
start an expression (a literal, a unary `-`, a `!`) belongs in `walkPrefix` and does not need a
precedence entry. `(` is the classic dual-role token: in prefix position it's grouping, in infix
position it's a call.

### Calls

`foo(a, b)` parses as: prefix `Identifier(foo)`, then `(` at `CALL` precedence beats `LOWEST`,
so `walkInfix(foo, '(')` loops `walkExpression(LOWEST)` + `expect(',')` until `)`, producing
`FunctionCall{funcNameIdx = foo, params = [a, b]}`. Because the callee is just "the expression to
the left", the AST allows any expression as a callee; `SimpleIRPass::emitCall` assumes it's an
`Identifier` (`std::get<AST::Identifier>`).

### Assignment

There's no separate assignment statement. `x = 5;` is an expression: `Identifier(x)`, then `=`
at `ASSIGN_OR_RET` beats `LOWEST`, giving `BinaryOperator{x, 5, Equal}`. Because it uses the same
left-fold, `a = b = c` parses as `(a = b) = c`, not the C-style right-associative form.

---

## 5. The AST arena

`ast.hpp` defines each node as a plain struct in one `std::variant` (`AstNodeType`). Nodes are
stored flat in `Parser::asts` and refer to each other by `NodeIndex` (an index into that vector),
not by pointer.

- **Children are pushed before parents.** `walkInfix` builds both operands, then
  `asts.push_back(op)` and returns `asts.size() - 1`. The arena ends up in post-order.
- **`roots`** holds the indices of top-level items (each `fn`, global `let`, or top-level
  expression). `SimpleIRPass` iterates `getRootIndices()`.
- Each `AstNode` has a `line` field, currently always `0` (the lexer doesn't track lines yet).

| Node               | Fields                                   | Produced by                  |
|--------------------|------------------------------------------|------------------------------|
| `IntLiteral`       | `literal`                                | `walkPrefix`                 |
| `Identifier`       | `name`                                   | `walkPrefix`                 |
| `BinaryOperator`   | `lhsOp`, `rhsOp`, `type` (the token type)| `walkInfix`                  |
| `FunctionCall`     | `funcNameIdx`, `params`                  | `walkInfix` on `(`           |
| `VariableDecl`     | `name`, `type`, `defineNode`             | `walkLet`                    |
| `FunctionDef`      | `name`, `params`, `body`, `retType`      | `walkFn`                     |
| `FunctionParamDef` | `name`, `type`                           | `walkFn`                     |
| `FunctionBody`     | `expressions`                            | `walkBody`                   |
| `ReturnVal`        | `type` (always `"unknown"`), `returnVal` | `walkReturn`                 |
| `IfStm`            | `cond`, `body`, `elbody` (optional)      | `walkIf`                     |
| `For`              | `init`, `comp`, `incr`, `body`           | `walkFor`                    |

`std::string_view` fields point into `Parser::program_`, so the AST must not outlive the
`Parser`.

---

## 6. The lexer

`Lexer` is a hand-written scanner over a `string_view`. `getToken()` skips whitespace, then
classifies the first character:

| Char class        | Routine        | Produces                                                |
|-------------------|----------------|---------------------------------------------------------|
| letter or `_`     | `parseAlpha`   | keyword (`let fn if else return for`), `Type`, or `Identifier` |
| digit             | `parseNumber`  | `IntLiteral` (decimal; a letter in it is `MalformedToken`) |
| anything else     | `parseSymbol`  | operator or punctuation; unknown → `UnknownToken`       |

Two-character operators (`->` `==` `!=` `<=` `>=` `&&` `||`) use `peek()` to check the next
character: maximal munch, so `==` wins over `=`.

Keywords are matched *after* scanning a whole word, so `letter` is an identifier, not `let` +
`ter`.

---

## 7. Error handling (current state)

Error handling is mostly a placeholder. Things that will bite you while extending the parser:

- **`expect` failures are usually ignored** (`auto c = expect(...)` with `c` never checked). A
  missing token is skipped over rather than reported.
- **`-1` as a `NodeIndex`.** `NodeIndex` is `std::size_t`, so `return -1` becomes `SIZE_MAX`. Any
  consumer that indexes `asts` with it reads out of bounds.
- **`walkPrefix` on an unexpected token returns `-1` without consuming anything.** Loops of the
  form `while (currentToken.t != CloseX)` (call arguments, `walkFn` params, `walkBody`) never
  advance on a stray token or EOF, so malformed input can hang the parser.
- **Lex errors are dereferenced unchecked.** `Parser` does `*lexer.getToken()`; an
  `UnknownToken` there is undefined behaviour.
- `Token::line` is always `0`.

---

## 8. Checklist: adding new syntax

1. **Lexer**: add a `TokenType`, produce it in `parseAlpha` (keyword) or `parseSymbol` (symbol).
2. **`tokenTypeToString`**: add a case so `printAST` dumps are readable.
3. **Decide the position.** Can it start an expression? → `walkPrefix`. Does it follow an operand
   and combine with it? → `walkInfix` **and** a `getPrecedence` entry. Is it a statement keyword?
   → a branch in `walk()` / `walkBody()` plus a `walkX()` function.
4. **AST**: add a node struct to `ast.hpp`, add it to `AstNodeType`, and add a `printAST` case.
5. **IR**: handle the new node in `SimpleIRPass` (both visitor sites, around the
   `emitStatement` and `emitExpression` visitors). The catch-all `[](const auto &) {}` lambdas
   mean a missing case compiles and silently emits nothing.
