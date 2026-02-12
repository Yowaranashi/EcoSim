#include "integration/test_cases.h"

#include <chrono>
#include <iomanip>
#include <iostream>

int main() {
    auto tests = ecosim_integration::buildIntegrationTests();

    int passed = 0;
    int failed = 0;
    for (std::size_t i = 0; i < tests.size(); ++i) {
        auto &test = tests[i];
        auto start = std::chrono::steady_clock::now();
        auto result = test->run();
        auto finish = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = finish - start;

        const char *status = result.passed ? "success" : "error";
        if (result.passed) {
            ++passed;
        } else {
            ++failed;
        }

        std::cout << "№ " << (i + 1) << ' ' << result.name << " result: " << status << " time: "
                  << std::fixed << std::setprecision(6) << elapsed.count() << " sec";
        if (!result.details.empty()) {
            std::cout << " details: " << result.details;
        }
        std::cout << '\n';
    }

    std::cout << "\nИтоговый результат: " << passed << " success, " << failed << " error." << '\n';
    return failed == 0 ? 0 : 1;
}
