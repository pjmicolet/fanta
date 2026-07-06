#pragma once
#include <expected>
#include <string_view>

using namespace std::literals;

enum class LexErrorType { UnterminatedString, UnknownToken, MalformedToken };

struct LexError {
  LexErrorType t;
  std::size_t line;
  std::size_t start;
  std::size_t end;
};

struct Lexer {

  Lexer(std::string_view body) : body_(body), end_{body.size()} {}

  enum class TokenType {
    IntLiteral,
    Eof,
    KeywordLet,
    KeywordFn,
    OpenParam,
    CloseParam,
    OpenBrace,
    CloseBrace,
    SemiColon,
    Comma,
    Plus,
    Minus,
    Slash,
    Mult,
    Arrow,
    Identifier,
    Equal,
    Colon,
    If,
    Else,
    Type,
    Return,
    EqualComp,
    Greater,
    Lesser,
    GreaterEq,
    LesserEq,
    NotEq,
    For,
    Or,
    And,
    Not,
  };

  struct Token {
    TokenType t;
    std::string_view lexeme;
    int line;
    std::size_t char_start;
    std::size_t char_end;
  };

  auto getToken() -> std::expected<Token, LexError> {
    Token t{};
    t.char_start = cursor_;
    std::string_view tString = body_.substr(cursor_, 0);

    while (cursor_ != end_) {
      switch (getType(body_.at(cursor_))) {
      case CharType::Alpha:
        return parseAlpha(cursor_);
      case CharType::Whitespace:
        cursor_++;
        break;
      case CharType::Symbol:
        return parseSymbol();
      case CharType::Num:
        return parseNumber(cursor_);
      }
    }

    return Token{TokenType::Eof, tString, 0, cursor_, end_};
  }

private:
  auto parseAlpha(std::size_t start) -> std::expected<Token, LexError> {
    while (cursor_ < end_ && ::isalnum(body_[cursor_]) &&
           (body_[cursor_] != ' ' || body_[cursor_] != '\0')) {
      cursor_++;
    }
    std::string_view lexeme = body_.substr(start, cursor_ - start);
    if (lexeme == "let")
      return Token{TokenType::KeywordLet, lexeme, 0 /*TODO: Add line*/, start,
                   cursor_};
    if (lexeme == "fn")
      return Token{TokenType::KeywordFn, lexeme, 0 /*TODO: Add line*/, start,
                   cursor_};
    if (lexeme == "if")
      return Token{TokenType::If, lexeme, 0 /*TODO: Add line*/, start, cursor_};
    if (lexeme == "else")
      return Token{TokenType::Else, lexeme, 0 /*TODO: Add line*/, start,
                   cursor_};
    if (lexeme == "return")
      return Token{TokenType::Return, lexeme, 0 /*TODO: Add line*/, start,
                   cursor_};
    if (lexeme == "for")
      return Token{TokenType::For, lexeme, 0 /*TODO: Add line*/, start,
                   cursor_};
    if (isType(lexeme))
      return Token{TokenType::Type, lexeme, 0, start, cursor_};
    return Token{TokenType::Identifier, lexeme, 0, start, cursor_};
  }

  auto isType(std::string_view lexeme) -> bool {
    return lexeme == "int" || lexeme == "float" || lexeme == "string" ||
           lexeme == "long" || lexeme == "void";
  }

  auto parseNumber(std::size_t start) -> std::expected<Token, LexError> {
    while (cursor_ < end_ && ::isalnum(body_[cursor_]) &&
           (body_[cursor_] != ' ' || body_[cursor_] != '\0')) {
      cursor_++;
    }

    std::string_view lexeme = body_.substr(start, cursor_ - start);
    for (const auto &c : lexeme) {
      if (::isalpha(c))
        return std::unexpected(
            LexError{LexErrorType::MalformedToken, 0, start, cursor_});
    }
    return Token{TokenType::IntLiteral, lexeme, 0 /*TODO: Add line*/, start,
                 cursor_};
  }

  auto parseSymbol() -> std::expected<Token, LexError> {
    switch (body_[cursor_]) {
    case '-': {
      auto next = peek();
      if (next == '>') {
        auto start = cursor_;
        cursor_++;
        return Token{TokenType::Arrow, body_.substr(start, 2), 0, start,
                     cursor_++};
      }
      return Token{TokenType::Minus, body_.substr(cursor_, 1), 0, cursor_,
                   cursor_++};
    }
    case '+':
      return Token{TokenType::Plus, body_.substr(cursor_, 1), 0, cursor_,
                   cursor_++};
    case '=': {
      auto next = peek();
      if (next == '=') {
        auto start = cursor_;
        cursor_++;
        return Token{TokenType::EqualComp, body_.substr(start, 2), 0, start,
                     cursor_++};
      }
      return Token{
          TokenType::Equal, body_.substr(cursor_, 1), 0, cursor_,
          cursor_++}; // Ugly hack but I need to increment cursor at some point
    }
    case '>': {
      auto next = peek();
      if (next == '=') {
        auto start = cursor_;
        cursor_++;
        return Token{TokenType::GreaterEq, body_.substr(start, 2), 0, start,
                     cursor_++};
      }
      return Token{
          TokenType::Greater, body_.substr(cursor_, 1), 0, cursor_,
          cursor_++}; // Ugly hack but I need to increment cursor at some point
    }
    case '<': {
      auto next = peek();
      if (next == '=') {
        auto start = cursor_;
        cursor_++;
        return Token{TokenType::LesserEq, body_.substr(start, 2), 0, start,
                     cursor_++};
      }
      return Token{
          TokenType::Lesser, body_.substr(cursor_, 1), 0, cursor_,
          cursor_++}; // Ugly hack but I need to increment cursor at some point
    }
    case '!': {
      auto next = peek();
      if (next == '=') {
        auto start = cursor_;
        cursor_++;
        return Token{TokenType::NotEq, body_.substr(start, 2), 0, start,
                     cursor_++};
      }
      return Token{TokenType::Not, body_.substr(cursor_, 1), 0, cursor_,
                   cursor_++};
    }
    case '&': {
      auto next = peek();
      if (next == '&') {
        auto start = cursor_;
        cursor_++;
        return Token{TokenType::And, body_.substr(start, 2), 0, start,
                     cursor_++};
      }
    }
    case '|': {
      auto next = peek();
      if (next == '|') {
        auto start = cursor_;
        cursor_++;
        return Token{TokenType::Or, body_.substr(start, 2), 0, start,
                     cursor_++};
      }
    }
    case '*':
      return Token{TokenType::Mult, body_.substr(cursor_, 1), 0, cursor_,
                   cursor_++};
    case '/':
      return Token{TokenType::Slash, body_.substr(cursor_, 1), 0, cursor_,
                   cursor_++};
    case ';':
      return Token{TokenType::SemiColon, body_.substr(cursor_, 1), 0, cursor_,
                   cursor_++};
    case '(':
      return Token{TokenType::OpenParam, body_.substr(cursor_, 1), 0, cursor_,
                   cursor_++};
    case '{':
      return Token{TokenType::OpenBrace, body_.substr(cursor_, 1), 0, cursor_,
                   cursor_++};
    case '}':
      return Token{TokenType::CloseBrace, body_.substr(cursor_, 1), 0, cursor_,
                   cursor_++};
    case ')':
      return Token{TokenType::CloseParam, body_.substr(cursor_, 1), 0, cursor_,
                   cursor_++};
    case ',':
      return Token{TokenType::Comma, body_.substr(cursor_, 1), 0, cursor_,
                   cursor_++};
    case ':':
      return Token{TokenType::Colon, body_.substr(cursor_, 1), 0, cursor_,
                   cursor_++};
    }
    return std::unexpected(
        LexError{LexErrorType::UnknownToken, 0, cursor_, cursor_++});
  }

  enum class CharType {
    Whitespace,
    Alpha,
    Num,
    Symbol,
  };

  auto getType(char c) const -> CharType {
    if (::isalpha(c))
      return CharType::Alpha;
    if (::isdigit(c))
      return CharType::Num;
    if (::isspace(c))
      return CharType::Whitespace;
    return CharType::Symbol;
  }

  auto peek() const -> char {
    if (cursor_ + 1 < end_)
      return body_.at(cursor_ + 1);
    return '\0';
  }

  std::string_view body_;
  std::size_t cursor_ = 0;
  std::size_t end_ = 0;
};
