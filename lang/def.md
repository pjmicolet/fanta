# Syntax Definition

## 1. Core Grammar Rules

* **Variables:** `let <name>: <type> = <expression>;` (using `let` keeps it modern/clean)
* **Functions:** `fn <name>(<params>) -> <return_type> { <body> }`
* **Parameters:** `<name>: <type>` separated by commas
* **Expressions:** Standard binary operations (+, -, *, /) and integer literals

---

## 2. Code Example (Mental Model)

```rust
fn add(a: int, b: int) -> int {
    let result: int = a + b;
    return result;
}

fn main() -> int {
    let val: int = add(10, 20);
    return val;
}
```

---

## 3. Basic EBNF / BNF Grammar

```ebnf
Program    ::= Function*
Function   ::= "fn" Identifier "(" ParamList? ")" "->" Type "{" Statement* "}"
ParamList  ::= Param ("," Param)*
Param      ::= Identifier ":" Type
Statement  ::= VarDecl | ReturnStmt | ExprStmt
VarDecl    ::= "let" Identifier ":" Type "=" Expr ";"
ReturnStmt ::= "return" Expr ";"
ExprStmt   ::= Expr ";"
```
