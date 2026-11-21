#pragma once
#include <cctype>
#include <cstdint>
#include <expected>
#include <string_view>

using namespace std::literals;

enum class LexErrorType : std::uint8_t {
  UnterminatedString,
  UnknownToken,
  MalformedToken
};

struct LexError {
  LexErrorType t;
  std::size_t line;
  std::size_t start;
  std::size_t end;
};

struct Lexer {

  Lexer(std::string_view body) : body_(body), end_{body.size()} {}

  enum class TokenType : std::uint8_t {
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
    BitOr,
    BitAnd,
    Intrinsic,
  };

  struct Token {
    TokenType t;
    std::string_view lexeme;
    uint64_t line;
    std::size_t char_start;
    std::size_t char_end;
  };

  auto getToken() -> std::expected<Token, LexError> {
    std::string_view tString = body_.substr(cursor_, 0);

    while (cursor_ != end_) {
      switch (getType(body_.at(cursor_))) {
      case CharType::Alpha:
        return parseAlpha(cursor_);
      case CharType::Whitespace:
        cursor_++;
        break;
      case CharType::Newline:
        cursor_++;
        line_++;
        break;
      case CharType::Symbol:
        return parseSymbol();
      case CharType::Num:
        return parseNumber(cursor_);
      }
    }

    return Token{TokenType::Eof, tString, line_, cursor_, end_};
  }

private:
  auto allowedCharsInAlpha(char c) -> bool {
    return (::isalnum(c) != 0) || c == '_';
  }

  auto parseAlpha(std::size_t start) -> std::expected<Token, LexError> {
    while (cursor_ < end_ && allowedCharsInAlpha(body_[cursor_]) &&
           (body_[cursor_] != ' ' || body_[cursor_] != '\0')) {
      cursor_++;
    }
    std::string_view lexeme = body_.substr(start, cursor_ - start);
    if (lexeme == "let")
      return Token{TokenType::KeywordLet, lexeme, line_, start, cursor_};
    if (lexeme == "fn")
      return Token{TokenType::KeywordFn, lexeme, line_, start, cursor_};
    if (lexeme == "if")
      return Token{TokenType::If, lexeme, line_, start, cursor_};
    if (lexeme == "else")
      return Token{TokenType::Else, lexeme, line_, start, cursor_};
    if (lexeme == "return")
      return Token{TokenType::Return, lexeme, line_, start, cursor_};
    if (lexeme == "for")
      return Token{TokenType::For, lexeme, line_, start, cursor_};
    if (isType(lexeme))
      return Token{TokenType::Type, lexeme, line_, start, cursor_};
    return Token{TokenType::Identifier, lexeme, line_, start, cursor_};
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
            LexError{LexErrorType::MalformedToken, line_, start, cursor_});
    }
    return Token{TokenType::IntLiteral, lexeme, line_, start, cursor_};
  }

  auto parseSymbol() -> std::expected<Token, LexError> {
    switch (body_[cursor_]) {
    case '-': {
      auto next = peek();
      if (next == '>') {
        auto start = cursor_;
        cursor_++;
        return Token{TokenType::Arrow, body_.substr(start, 2), line_, start,
                     cursor_++};
      }
      return Token{TokenType::Minus, body_.substr(cursor_, 1), line_, cursor_,
                   cursor_++};
    }
    case '+':
      return Token{TokenType::Plus, body_.substr(cursor_, 1), line_, cursor_,
                   cursor_++};
    case '=': {
      auto next = peek();
      if (next == '=') {
        auto start = cursor_;
        cursor_++;
        return Token{TokenType::EqualComp, body_.substr(start, 2), line_, start,
                     cursor_++};
      }
      return Token{
          TokenType::Equal, body_.substr(cursor_, 1), line_, cursor_,
          cursor_++}; // Ugly hack but I need to increment cursor at some point
    }
    case '>': {
      auto next = peek();
      if (next == '=') {
        auto start = cursor_;
        cursor_++;
        return Token{TokenType::GreaterEq, body_.substr(start, 2), line_, start,
                     cursor_++};
      }
      return Token{
          TokenType::Greater, body_.substr(cursor_, 1), line_, cursor_,
          cursor_++}; // Ugly hack but I need to increment cursor at some point
    }
    case '<': {
      auto next = peek();
      if (next == '=') {
        auto start = cursor_;
        cursor_++;
        return Token{TokenType::LesserEq, body_.substr(start, 2), line_, start,
                     cursor_++};
      }
      return Token{
          TokenType::Lesser, body_.substr(cursor_, 1), line_, cursor_,
          cursor_++}; // Ugly hack but I need to increment cursor at some point
    }
    case '!': {
      auto next = peek();
      if (next == '=') {
        auto start = cursor_;
        cursor_++;
        return Token{TokenType::NotEq, body_.substr(start, 2), line_, start,
                     cursor_++};
      }
      return Token{TokenType::Not, body_.substr(cursor_, 1), line_, cursor_,
                   cursor_++};
    }
    case '&': {
      auto next = peek();
      if (next == '&') {
        auto start = cursor_;
        cursor_++;
        return Token{TokenType::And, body_.substr(start, 2), line_, start,
                     cursor_++};
      }
      return Token{TokenType::BitAnd, body_.substr(cursor_, 1), line_, cursor_,
                   cursor_++};
    }
    case '|': {
      auto next = peek();
      if (next == '|') {
        auto start = cursor_;
        cursor_++;
        return Token{TokenType::Or, body_.substr(start, 2), line_, start,
                     cursor_++};
      }
      return Token{TokenType::BitOr, body_.substr(cursor_, 1), line_, cursor_,
                   cursor_++};
    }
    case '*':
      return Token{TokenType::Mult, body_.substr(cursor_, 1), line_, cursor_,
                   cursor_++};
    case '/':
      return Token{TokenType::Slash, body_.substr(cursor_, 1), line_, cursor_,
                   cursor_++};
    case ';':
      return Token{TokenType::SemiColon, body_.substr(cursor_, 1), line_,
                   cursor_, cursor_++};
    case '(':
      return Token{TokenType::OpenParam, body_.substr(cursor_, 1), line_,
                   cursor_, cursor_++};
    case '{':
      return Token{TokenType::OpenBrace, body_.substr(cursor_, 1), line_,
                   cursor_, cursor_++};
    case '}':
      return Token{TokenType::CloseBrace, body_.substr(cursor_, 1), line_,
                   cursor_, cursor_++};
    case ')':
      return Token{TokenType::CloseParam, body_.substr(cursor_, 1), line_,
                   cursor_, cursor_++};
    case ',':
      return Token{TokenType::Comma, body_.substr(cursor_, 1), line_, cursor_,
                   cursor_++};
    case ':':
      return Token{TokenType::Colon, body_.substr(cursor_, 1), line_, cursor_,
                   cursor_++};
    case '@': {
      auto start = cursor_++;
      while (cursor_ < end_ && allowedCharsInAlpha(body_[cursor_])) {
        cursor_++;
      }
      return Token{TokenType::Intrinsic, body_.substr(start, cursor_ - start),
                   line_, start, cursor_};
    }
    default:
      return std::unexpected(
          LexError{LexErrorType::UnknownToken, line_, cursor_, cursor_++});
    }
  }

  enum class CharType : std::uint8_t {
    Whitespace,
    Newline,
    Alpha,
    Num,
    Symbol,
  };

  [[nodiscard]] auto getType(char c) const -> CharType {
    if (::isalpha(c) || c == '_')
      return CharType::Alpha;
    if (::isdigit(c))
      return CharType::Num;
    if (c == '\n')
      return CharType::Newline;
    if (::isspace(c))
      return CharType::Whitespace;
    return CharType::Symbol;
  }

  [[nodiscard]] auto peek() const -> char {
    if (cursor_ + 1 < end_)
      return body_.at(cursor_ + 1);
    return '\0';
  }

  std::string_view body_;
  std::size_t cursor_ = 0;
  std::size_t end_ = 0;
  uint64_t line_ = 1;
};
