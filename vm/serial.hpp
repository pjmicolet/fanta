#pragma once

#include <concepts>
#include <cstdint>
#include <string>

template <typename D>
concept SerialDevice = requires(D device, uint32_t offset, uint32_t val) {
  { D::size } -> std::convertible_to<uint32_t>;
  { device.read(offset) } -> std::same_as<uint32_t>;
  { device.write(offset, val) } -> std::same_as<void>;
};

template <uint32_t s = 8> struct BasicSerial {
  auto write(uint32_t offset, uint32_t val) -> void {
    output.push_back(static_cast<char>(val & 0xFF));
  }
  auto read(uint32_t offset) -> uint32_t { /**TODO: Add state**/ return 0; }

  std::string output;
  static constexpr uint32_t size = s;
};
