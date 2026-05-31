#include <lexer.hpp>
#include <testframework/testing.hpp>

// Helper macro to assert token type on std::expected<Token, LexError>
#define REQUIRE_TOKEN_TYPE(expected_type, expected_res) \
  REQUIRE_SAME(1, expected_res.has_value() ? 1 : 0); \
  REQUIRE_SAME(expected_type, expected_res->t)

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
