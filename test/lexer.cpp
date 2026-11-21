#include <array>
#include <lexer.hpp>
#include <testframework/testing.hpp>

// Helper macro to assert token type on std::expected<Token, LexError>
#define REQUIRE_TOKEN_TYPE(expected_type, expected_res)                        \
  REQUIRE_SAME(1, (expected_res).has_value() ? 1 : 0);                         \
  REQUIRE_SAME((expected_type), (expected_res)->t)

TEST_CASE("Basic Lexing of Alpha") {
  std::string case1 = "hello";
  Lexer simple{case1};
  auto res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Identifier, res);

  std::string case2 = "let";
  Lexer letLexer{case2};
  auto res2 = letLexer.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::KeywordLet, res2);

  std::string case3 = "let1";
  Lexer fakeLetLexer{case3};
  auto res3 = fakeLetLexer.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Identifier, res3);
}

TEST_CASE("Multi Alpha Lexing") {
  std::string case1 = "hello let if else qwer1";
  Lexer simple{case1};

  auto res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Identifier, res);

  res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::KeywordLet, res);

  res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::If, res);

  res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Else, res);

  res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Identifier, res);

  res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Eof, res);
}

TEST_CASE("Type Alpha Lexing") {
  std::string case1 = "int float ten";
  Lexer simple{case1};

  auto res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Type, res);

  res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Type, res);

  res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Identifier, res);
}

TEST_CASE("Basic Symbol Lexing") {
  std::string case1 = "-";
  Lexer simple{case1};
  auto res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Minus, res);

  std::string case2 = "->";
  Lexer arrow_lex{case2};
  auto res2 = arrow_lex.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Arrow, res2);
}

TEST_CASE("Arrow and Minus Symbol Lexing") {
  std::string case1 = "- -> - - ->-->";
  Lexer simple{case1};

  auto res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Minus, res);

  res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Arrow, res);

  res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Minus, res);

  res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Minus, res);

  res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Arrow, res);

  res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Minus, res);

  res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Arrow, res);
}

TEST_CASE("Basic integer parsing") {
  std::string case1 = "123123";
  Lexer simple{case1};
  auto res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::IntLiteral, res);
}

TEST_CASE("Tokenize Assignment") {
  std::string case1 = "let hello : int = 12";
  Lexer simple{case1};
  auto res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::KeywordLet, res);
  res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Identifier, res);
  res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Colon, res);
  res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Type, res);
  res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Equal, res);
  res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::IntLiteral, res);
}

TEST_CASE("Comparison And Logical Operator Lexing") {
  std::string src = "== != <= >= < > && || ! =";
  Lexer lx{src};
  const std::array expected = {
      Lexer::TokenType::EqualComp, Lexer::TokenType::NotEq,
      Lexer::TokenType::LesserEq,  Lexer::TokenType::GreaterEq,
      Lexer::TokenType::Lesser,    Lexer::TokenType::Greater,
      Lexer::TokenType::And,       Lexer::TokenType::Or,
      Lexer::TokenType::Not,       Lexer::TokenType::Equal,
      Lexer::TokenType::Eof,
  };
  for (auto t : expected) {
    auto res = lx.getToken();
    REQUIRE_TOKEN_TYPE(t, res);
  }
}

TEST_CASE("Unknown Symbol Reports UnknownToken And Advances") {
  std::string src = "` x";
  Lexer lx{src};
  auto res = lx.getToken();
  REQUIRE_TRUE(!res.has_value());
  REQUIRE_SAME(LexErrorType::UnknownToken, res.error().t);
  auto next = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Identifier, next);
}

TEST_CASE("Digits Followed By Letters Is MalformedToken") {
  std::string src = "12ab";
  Lexer lx{src};
  auto res = lx.getToken();
  REQUIRE_TRUE(!res.has_value());
  REQUIRE_SAME(LexErrorType::MalformedToken, res.error().t);
}

TEST_CASE("Var with underscore") {
  std::string case1 = "hello_world";
  Lexer simple{case1};
  auto res = simple.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Identifier, res);

  std::string case2 = "_helloworld";
  Lexer simple2{case2};
  auto res2 = simple2.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Identifier, res2);
}

TEST_CASE("Single & And | Lex As BitAnd / BitOr") {
  std::string src = "a & b | c";
  Lexer lx{src};
  auto t1 = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Identifier, t1);
  auto amp = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::BitAnd, amp);
  REQUIRE_SAME("&", amp->lexeme);
  auto t2 = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Identifier, t2);
  auto bar = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::BitOr, bar);
  REQUIRE_SAME("|", bar->lexeme);
  auto t3 = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Identifier, t3);
  auto t4 = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Eof, t4);
}

TEST_CASE("Single And Double & / | Are Distinguished Without Spaces") {
  // "&&&" must lex as "&&" then "&"; "|||" as "||" then "|".
  std::string src = "a&&&b|||c&d";
  Lexer lx{src};
  const std::array expected = {
      Lexer::TokenType::Identifier, Lexer::TokenType::And,
      Lexer::TokenType::BitAnd,     Lexer::TokenType::Identifier,
      Lexer::TokenType::Or,         Lexer::TokenType::BitOr,
      Lexer::TokenType::Identifier, Lexer::TokenType::BitAnd,
      Lexer::TokenType::Identifier, Lexer::TokenType::Eof,
  };
  for (auto t : expected) {
    auto res = lx.getToken();
    REQUIRE_TOKEN_TYPE(t, res);
  }
}

TEST_CASE("Trailing & / | At End Of Input") {
  std::string src = "&";
  Lexer lx{src};
  auto t5 = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::BitAnd, t5);
  auto t6 = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Eof, t6);

  std::string src2 = "|";
  Lexer lx2{src2};
  auto t7 = lx2.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::BitOr, t7);
  auto t8 = lx2.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Eof, t8);
}

TEST_CASE("Intrinsic Lexing") {
  std::string src = "@print";
  Lexer lx{src};
  auto t = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Intrinsic, t);
  REQUIRE_SAME("@print", t->lexeme);
}

// Line numbers are 1-based and count '\n' only.
TEST_CASE("Line Numbers: Keywords And Literals") {
  std::string src = "let\nfn\n\n42";
  Lexer lx{src};
  auto t = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::KeywordLet, t);
  REQUIRE_SAME(uint64_t{1}, t->line);
  t = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::KeywordFn, t);
  REQUIRE_SAME(uint64_t{2}, t->line);
  t = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::IntLiteral, t);
  REQUIRE_SAME(uint64_t{4}, t->line);
}

TEST_CASE("Line Numbers: Identifiers And Types") {
  std::string src = "foo\nint";
  Lexer lx{src};
  auto t = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Identifier, t);
  REQUIRE_SAME(uint64_t{1}, t->line);
  t = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Type, t);
  REQUIRE_SAME(uint64_t{2}, t->line);
}

TEST_CASE("Line Numbers: Symbols And Intrinsics") {
  std::string src = ";\n->\n==\n@print";
  Lexer lx{src};
  auto t = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::SemiColon, t);
  REQUIRE_SAME(uint64_t{1}, t->line);
  t = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Arrow, t);
  REQUIRE_SAME(uint64_t{2}, t->line);
  t = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::EqualComp, t);
  REQUIRE_SAME(uint64_t{3}, t->line);
  t = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Intrinsic, t);
  REQUIRE_SAME(uint64_t{4}, t->line);
}

TEST_CASE("Line Numbers: Eof And CRLF") {
  // '\r' is plain whitespace, so CRLF counts as one line break.
  std::string src = "let\r\nfn\r\n";
  Lexer lx{src};
  auto t = lx.getToken();
  REQUIRE_SAME(uint64_t{1}, t->line);
  t = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::KeywordFn, t);
  REQUIRE_SAME(uint64_t{2}, t->line);
  t = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::Eof, t);
  REQUIRE_SAME(uint64_t{3}, t->line);
}

TEST_CASE("Line Numbers: Lex Errors") {
  std::string src = "let\n$";
  Lexer lx{src};
  auto t = lx.getToken();
  REQUIRE_TOKEN_TYPE(Lexer::TokenType::KeywordLet, t);
  auto bad = lx.getToken();
  REQUIRE_SAME(0, bad.has_value() ? 1 : 0);
  REQUIRE_SAME(std::size_t{2}, bad.error().line);
}

TEST_CASE("Line Numbers: Multi-line Program") {
  std::string src = "fn main() -> int {\n  let b : int = 10;\n  return b;\n}\n";
  Lexer lx{src};
  uint64_t expected[] = {1, 1, 1, 1, 1, 1, 1,      // fn main ( ) -> int {
                         2, 2, 2, 2, 2, 2, 2,      // let b : int = 10 ;
                         3, 3, 3,                  // return b ;
                         4};                       // }
  for (auto line : expected) {
    auto t = lx.getToken();
    REQUIRE_SAME(1, t.has_value() ? 1 : 0);
    REQUIRE_SAME(line, t->line);
  }
}
