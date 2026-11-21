#pragma once
// compiler_def.hpp names Allocator without including it.
#include "allocator.hpp"
#include "compiler_def.hpp"
#include <chrono>
#include <cstdint>
#include <exception>
#include <expected>
#include <format>
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

/// A failed build: the message to show, and where in the source it failed.
struct Error {
  std::string text;
  std::size_t line = 0; ///< 1-based source line; 0 when the pass doesn't know
};

/**
 * @brief Compiles `source` with the default pass pipeline.
 *
 * @details The error string is ready to show: "Parse error: …" when the
 * parser rejects the source (returned as a ParserError, with its line),
 * "Compile error: …" when a later pass throws (no line). It runs on a worker thread with a timeout because the parser
 * can still loop forever on some malformed input (review B6). A compile that
 * times out is reported, and its thread is detached: it keeps spinning until
 * the TUI exits, but the TUI stays responsive and the editor contents are
 * safe.
 */
inline auto compile(std::string source,
                    std::chrono::milliseconds timeout = std::chrono::seconds(2))
    -> std::expected<std::vector<uint32_t>, Error> {
  using Result = std::expected<std::vector<uint32_t>, Error>;

  auto promise = std::make_shared<std::promise<Result>>();
  auto future = promise->get_future();
  std::thread([promise, src = std::move(source)] mutable {
    try {
      auto built = DefaultCompiler{}.compile(std::move(src));
      if (built)
        promise->set_value(std::move(*built));
      else
        promise->set_value(std::unexpected(Error{
            std::format("Parse error (line {}): {}", built.error().line_,
                        built.error().what_),
            built.error().line_}));
    } catch (const std::exception &e) {
      promise->set_value(
          std::unexpected(Error{"Compile error: " + std::string{e.what()}}));
    } catch (...) {
      promise->set_value(
          std::unexpected(Error{"Compile error: unknown compiler error"}));
    }
  }).detach();

  if (future.wait_for(timeout) == std::future_status::timeout)
    return std::unexpected(Error{
        "Compile error: compiler did not finish (parser hang on malformed input, review B6)"});
  return future.get();
}

} // namespace Fanta::Build
