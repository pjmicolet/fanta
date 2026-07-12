#pragma once

// Helper for building a std::visit callable from a set of lambdas, one per
// variant alternative: std::visit(overloaded{lambda1, lambda2, ...}, v).
template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
