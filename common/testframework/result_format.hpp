#pragma once
#include <string>

namespace Test {
namespace Format {
    struct Result {
        virtual auto to_string() -> std::string = 0;
        virtual ~Result() {};
    };

    struct Passed : public Result {
        Passed(const std::string& testName) : testName_(testName) {};
        auto to_string() -> std::string override {
            return "\033[1;32m" + testName_ + " PASSED \033[0m";
        }
        private:
            std::string testName_;
    };

    struct Failed : public Result {
        Failed(const std::string& testName, const std::string& reason) : testName_(testName),
            reason_(reason) {};
        auto to_string() -> std::string override {
            return "\033[1;31m" + testName_ + " FAILED due to " + reason_+ "\033[0m"   ;
        }
        private:
            std::string testName_;
            std::string reason_;
    };
};
};
