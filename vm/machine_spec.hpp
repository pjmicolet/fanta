#pragma once
#include <cstdint>

/**
 * @brief The machine's datasheet: the nominal numbers every timing is derived
 * from.
 *
 * @details Nothing here reads a real clock. The emulator counts cycles, and
 * these numbers only say how many cycles a device operation takes, so a run is
 * the same on every host. Store inputs as they would appear on a datasheet and
 * derive everything else in member functions, so retuning one number can't
 * leave a stale constant behind. Machine variants are different values of this
 * struct, not different types.
 */
struct MachineSpec {
  uint64_t cpu_hz = 200'000'000; // Pentium Pro era
  uint32_t serial_baud = 115'200;
  uint32_t serial_bits_per_char = 10; // start + 8 data + stop

  /// Cycles the serial port stays busy after accepting a byte.
  [[nodiscard]] constexpr auto cycles_per_serial_char() const -> uint64_t {
    return cpu_hz / (serial_baud / serial_bits_per_char);
  }
};

inline constexpr MachineSpec DefaultSpec{};

static_assert(DefaultSpec.cycles_per_serial_char() >= 1,
              "a serial character must take at least one cycle");
