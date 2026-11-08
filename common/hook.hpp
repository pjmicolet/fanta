#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Fanta {

struct Hook {
  std::string_view name;
  uint8_t arity;
  uint32_t addr;
};

// Top of the address space, like a PC's boot ROM: nothing in RAM or the device
// area can sit here, so a CALL landing in this range is always a hook. CALL's
// signed 26-bit offset wraps around 32 bits to reach it, from anywhere except
// the top 256 bytes of RAM. Just above the device base wouldn't work: it's out
// of range from low RAM. Keep this close to the top, as it sets that limit.
constexpr static uint32_t HOOK_BASE = 0xFFFF'FF00;
constexpr static std::array<Hook, 2> hooks = {
    Hook{"@print", 1, HOOK_BASE},
    Hook{"@printu32", 1, HOOK_BASE + 4},
};
constexpr static size_t NUM_HOOKS = hooks.size();
constexpr static uint32_t HOOK_END = HOOK_BASE + ((NUM_HOOKS - 1) * 4);
} // namespace Fanta
