#pragma once
#include <string>
#include "lexer.hpp"

struct Parser {
  Parser(std::string program) : lexer(program) {}

private:
  Lexer lexer;
};
