#pragma once
#include <string>
#include <utility>

namespace Test {
namespace Format {
    struct Result {
        virtual auto to_string() -> std::string = 0;
        virtual ~Result() = default;
    };

    struct Passed : public Result {
        Passed(std::string testName) : testName_(std::move(testName)) {};
        auto to_string() -> std::string override {
            return "\033[1;32m" + testName_ + " PASSED \033[0m";
        }
        private:
            std::string testName_;
    };

    struct Failed : public Result {
        Failed(std::string testName, std::string reason) : testName_(std::move(testName)),
            reason_(std::move(reason)) {};
        auto to_string() -> std::string override {
            return "\033[1;31m" + testName_ + " FAILED due to " + reason_+ "\033[0m"   ;
        }
        private:
            std::string testName_;
            std::string reason_;
    };
};
};
