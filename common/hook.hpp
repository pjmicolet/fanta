#pragma once
#include <array>
#include <string>

namespace Fanta {

struct Hook {
  std::string name;
  uint8_t arity;
  uint32_t addr;
};

constexpr static uint32_t HOOK_BASE = 0x0100000;
constexpr static std::array<Hook, 1> hooks = {Hook{"@print", 1, HOOK_BASE}};
constexpr static size_t NUM_HOOKS = hooks.size();
constexpr static uint32_t HOOK_END = HOOK_BASE + ((NUM_HOOKS - 1) * 4);
} // namespace Fanta
