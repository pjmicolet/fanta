#pragma once
// compiler_def.hpp names Allocator without including it.
#include "allocator.hpp"
#include "compiler_def.hpp"
#include <chrono>
#include <cstdint>
#include <exception>
#include <expected>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief Compiling Fanta source from inside the TUI.
 *
 * @details Kept apart from program.hpp so fanta-trace/fanta-diff don't pull
 * in the compiler.
 */
namespace Fanta::Build {

/**
 * @brief Compiles `source` with the default pass pipeline.
 *
 * @details The compiler reports errors by throwing; they come back as the
 * error string. It runs on a worker thread with a timeout because the parser
 * can still loop forever on some malformed input (review B6). A compile that
 * times out is reported, and its thread is detached: it keeps spinning until
 * the TUI exits, but the TUI stays responsive and the editor contents are
 * safe.
 */
inline auto compile(std::string source,
                    std::chrono::milliseconds timeout = std::chrono::seconds(2))
    -> std::expected<std::vector<uint32_t>, std::string> {
  using Result = std::expected<std::vector<uint32_t>, std::string>;

  auto promise = std::make_shared<std::promise<Result>>();
  auto future = promise->get_future();
  std::thread([promise, src = std::move(source)]() mutable {
    try {
      promise->set_value(DefaultCompiler{}.compile(std::move(src)));
    } catch (const std::exception &e) {
      promise->set_value(std::unexpected(std::string{e.what()}));
    } catch (...) {
      promise->set_value(std::unexpected(std::string{"unknown compiler error"}));
    }
  }).detach();

  if (future.wait_for(timeout) == std::future_status::timeout)
    return std::unexpected(std::string{
        "compiler did not finish (parser hang on malformed input, review B6)"});
  return future.get();
}

} // namespace Fanta::Build
