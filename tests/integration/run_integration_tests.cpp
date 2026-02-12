#include "integration/test_cases.h"

#include <iostream>

int main() {
    auto tests = ecosim_integration::buildIntegrationTests();

    int passed = 0;
    int failed = 0;
    for (auto &test : tests) {
        auto result = test->run();
        if (result.passed) {
            ++passed;
            std::cout << "[PASS] " << result.name << " :: " << result.details << '\n';
        } else {
            ++failed;
            std::cout << "[FAIL] " << result.name << " :: " << result.details << '\n';
        }
    }

    std::cout << "\nИтоговый результат: " << passed << " passed, " << failed << " failed." << '\n';
    return failed == 0 ? 0 : 1;
}
