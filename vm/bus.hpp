#pragma once

#include "serial.hpp"
#include <algorithm>
#include <cstdint>
#include <optional>
#include <tuple>
#include <type_traits>

/**
 * @brief A device placed at address B on the bus.
 *
 * @details The device only knows its size; where it sits is the board's
 * decision, so the same device type can be mapped at different bases.
 */
template <SerialDevice SType, uint32_t B> struct Mapped {
  static constexpr uint32_t base = B;
  static constexpr uint32_t size = SType::size;
  SType s;
};

/**
 * @brief Compile-time device map. Each access is one unrolled range check per
 * device; the first device whose range contains the address handles it, with
 * an offset relative to its base.
 */
template <typename... Dv> struct Bus {
  std::tuple<Dv...> devices;
  static constexpr uint32_t lowest = std::min({Dv::base...});

  /// @return true if a device decoded the address.
  auto write(uint32_t addr, uint32_t data) -> bool {
    return std::apply(
        [&](auto &...dev) {
          // addr - base < size: unsigned wrap makes this a two-sided check.
          return ((addr - dev.base < dev.size
                       ? (dev.s.write(addr - dev.base, data), true)
                       : false) ||
                  ...);
        },
        devices);
  }

  /// @return the device's value, or nullopt if no device decoded the address.
  auto read(uint32_t addr) -> std::optional<uint32_t> {
    std::optional<uint32_t> out;
    std::apply(
        [&](auto &...dev) {
          // Only the short-circuit matters here; the value is in `out`.
          (void)((addr - dev.base < dev.size
                      ? (out = dev.s.read(addr - dev.base), true)
                      : false) ||
                 ...);
        },
        devices);
    return out;
  }

  /// The first mapped device of type D (e.g. for the TUI to drain output).
  template <typename D> auto get() -> D & {
    return get_impl<D, 0>();
  }

private:
  template <typename D, std::size_t I> auto get_impl() -> D & {
    static_assert(I < sizeof...(Dv), "No device of this type on the bus");
    auto &slot = std::get<I>(devices);
    if constexpr (std::is_same_v<std::remove_cvref_t<decltype(slot.s)>, D>)
      return slot.s;
    else
      return get_impl<D, I + 1>();
  }
};
