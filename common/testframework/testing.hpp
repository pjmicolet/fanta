#pragma once
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "assertions.hpp"
#include "result_format.hpp"

#define FUNC_NAME TestCase
#define GEN_CLASS_NAME3(name, line) name ## line
#define GEN_CLASS_NAME2(name, line) GEN_CLASS_NAME3(name, line)
#define GEN_CLASS_NAME(name) GEN_CLASS_NAME2(name, __COUNTER__)

#define GEN_INSTANCE(name) name ## Instance

struct TestCase;

struct TestCase {
    TestCase(std::string name) : name_(name) {
    };
    virtual auto run() -> void = 0;
    auto report() {
        std::cout << "Running " << name_ << "...\n";
        run();
    };
    const std::string name_;
};

static std::vector<TestCase*> cases;


#define GEN_TEST(FUNCNAME, strdescriptor,...)\
struct FUNCNAME : public TestCase {\
    FUNCNAME() : TestCase(strdescriptor) {\
        cases.push_back(this);\
    };\
    auto run() -> void override;\
};\
static FUNCNAME GEN_INSTANCE(FUNCNAME){};\
void FUNCNAME::run()\

#define TEST_CASE(name) GEN_TEST(GEN_CLASS_NAME(FUNC_NAME),name)

int main() {
    std::vector<std::unique_ptr<Test::Format::Result>> results;
    for(const auto& tCase : cases) {
        try {
            tCase->report();
        } catch(const TestException& e ) {
            results.emplace_back(static_cast<std::unique_ptr<Test::Format::Result>>(std::make_unique<Test::Format::Failed>(tCase->name_, e.what())));
            continue;
        }
        results.emplace_back(static_cast<std::unique_ptr<Test::Format::Result>>(std::make_unique<Test::Format::Passed>(tCase->name_)));
    }

    const std::string indent = "    ";
    for(const auto& result : results) {
        std::cout << indent << result->to_string() << "\n";
    }
}
