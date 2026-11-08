#pragma once
#include "assertions.hpp"
#include "result_format.hpp"
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#define FUNC_NAME TestCase
#define GEN_CLASS_NAME3(name, line) name##line
#define GEN_CLASS_NAME2(name, line) GEN_CLASS_NAME3(name, line)
#define GEN_CLASS_NAME(name) GEN_CLASS_NAME2(name, __COUNTER__)

#define GEN_INSTANCE(name) name##Instance

struct TestCase;

struct TestCase {
  TestCase(std::string name) : name_(name) {};
  virtual auto run() -> void = 0;
  auto report() {
    std::cout << "Running " << name_ << "...\n";
    run();
  };
  const std::string name_;
};

static std::vector<TestCase *> cases;

#define GEN_TEST(FUNCNAME, strdescriptor, ...)                                 \
  struct FUNCNAME : public TestCase {                                          \
    FUNCNAME() : TestCase(strdescriptor) { cases.push_back(this); };           \
    auto run() -> void override;                                               \
  };                                                                           \
  static FUNCNAME GEN_INSTANCE(FUNCNAME){};                                    \
  void FUNCNAME::run()

#define TEST_CASE(name) GEN_TEST(GEN_CLASS_NAME(FUNC_NAME), name)

int main() try {
  std::vector<std::unique_ptr<Test::Format::Result>> results;
  bool failed = false;
  auto recordFailure = [&](const std::string &name, const std::string &why) {
    failed = true;
    results.emplace_back(std::make_unique<Test::Format::Failed>(name, why));
  };
  for (const auto &tCase : cases) {
    try {
      tCase->report();
    } catch (const TestException &e) {
      recordFailure(tCase->name_, e.what());
      continue;
    } catch (const std::exception &e) {
      // Anything else would terminate the run and lose every result so far.
      recordFailure(tCase->name_,
                    std::string("unexpected exception: ") + e.what());
      continue;
    }
    results.emplace_back(std::make_unique<Test::Format::Passed>(tCase->name_));
  }

  const std::string indent = "    ";
  for (const auto &result : results) {
    std::cout << indent << result->to_string() << "\n";
  }
  return failed ? EXIT_FAILURE : EXIT_SUCCESS;
} catch (...) {
  // Reporting itself failed (e.g. out of memory); still fail the run.
  std::cerr << "test runner: internal error while reporting results\n";
  return EXIT_FAILURE;
}
