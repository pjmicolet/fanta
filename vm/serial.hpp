#pragma once

#include <concepts>
#include <cstdint>
#include <string>

/**
 * @brief A memory-mapped device. `now` is the CPU's cycle count, so a device
 * can model how long an operation takes without being called every cycle.
 */
template <typename D>
concept SerialDevice = requires(D device, uint32_t offset, uint32_t val,
                                uint64_t now) {
  { D::size } -> std::convertible_to<uint32_t>;
  { device.read(offset, now) } -> std::same_as<uint32_t>;
  { device.write(offset, val, now) } -> std::same_as<void>;
};

/**
 * @brief A UART: DATA at +0, STATUS at +4.
 *
 * @details Sending a byte keeps the port busy for CyclesPerChar cycles. A byte
 * written while it's busy is lost, as on a 16550 without its FIFO, and counted
 * in `dropped` so the loss is visible. Programs poll STATUS for TX_READY
 * before each write.
 */
template <uint32_t Size = 8, uint64_t CyclesPerChar = 1> struct BasicSerial {
  static constexpr uint32_t size = Size;
  static constexpr uint64_t cycles_per_char = CyclesPerChar;

  static constexpr uint32_t RX_READY = 0x1; // a received byte is waiting
  static constexpr uint32_t TX_READY = 0x2; // DATA accepts a byte

  auto write(uint32_t offset, uint32_t val, uint64_t now) -> void {
    if (offset != 0)
      return;
    if (now < ready_at_cycle) {
      ++dropped;
      return;
    }
    output.push_back(static_cast<char>(val & 0xFF));
    ready_at_cycle = now + cycles_per_char;
  }

  [[nodiscard]] auto read(uint32_t offset, uint64_t now) const -> uint32_t {
    if (offset == 0)
      return 0; // nothing is ever received yet
    return now >= ready_at_cycle ? TX_READY : 0;
  }

  /// Cycles until DATA accepts a byte; 0 if it already does.
  [[nodiscard]] auto cycles_until_ready(uint64_t now) const -> uint64_t {
    return now >= ready_at_cycle ? 0 : ready_at_cycle - now;
  }

  std::string output;
  uint64_t ready_at_cycle = 0;
  uint64_t dropped = 0;
};
