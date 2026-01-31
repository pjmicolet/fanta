#pragma once
#include <stdexcept>
#include <string>
#include <utility>
#include <functional>

class TestException : public std::runtime_error {
  std::string exception_;

public:
  TestException(std::string reason) : std::runtime_error(reason) {};
};

namespace Test {
namespace Exceptions {
inline auto require_true(const std::string statement, const int line,
                         const bool result) -> void {
  if (!result) {
    std::string issue = ": Failed assertion \"" + statement + "\" at line " +
                        std::to_string(line);
    throw TestException(issue);
  }
};

template <typename T, typename R>
inline auto compare(const T& rhs, const R& lhs) -> bool {
  if constexpr(std::is_same<T, R>::value) {
    return rhs == lhs;
  } 
  // There's some signedness we need to check
  if constexpr(std::is_integral<T>::value && std::is_integral<R>::value) {
    return std::cmp_equal(rhs,lhs);
  }
  else  {
    return rhs == lhs;
  }
}

template <typename T, typename R>
inline auto require_same(const int line, const T &expected, const R &got, std::function<std::string()> onDump = [](){ return "";})
    -> void {
  if (!compare(expected,got)) {
    std::string expectedS;
    std::string gotS;

    if constexpr(std::is_integral<T>::value) {
      expectedS = std::to_string(expected);
    } else {
      expectedS = std::string(expected);
    }

    if constexpr(std::is_integral<R>::value) {
      gotS = std::to_string(got);
    } else {
      gotS = std::string(got);
    }

    std::string extraDump = onDump();
    std::string issue = "Line:" + std::to_string(line) + ": Items do not match, expected=  \"" +
                        expectedS + "\" got \"" +
                        gotS +"\"";
    if(!extraDump.empty()) {
      issue += " Extra Info: " + extraDump;
    }
    throw TestException(issue);
  }
};
}; // namespace Exceptions
}; // namespace Test

#define REQUIRE_TRUE(EXPR)                                                     \
  Test::Exceptions::require_true(#EXPR, __LINE__, (EXPR) == true);

#define REQUIRE_SAME(EXPECTED, GOT)                                            \
  Test::Exceptions::require_same(__LINE__, EXPECTED, GOT);

#define REQUIRE_SAME_VERBOSE(EXPECTED, GOT, DUMP)                               \
  Test::Exceptions::require_same(__LINE__, EXPECTED, GOT, DUMP);
