// SPDX-License-Identifier: GPL-3.0-only
// Test harness implementation (kept out of the header to avoid a singleton in
// every translation unit that writes tests).
#include "TestFramework.hpp"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <mutex>

namespace fe::test {
namespace {

std::mutex g_reportMutex;
int g_failuresInCurrentTest = 0;
bool g_verbose = false;

}  // namespace

Registry& Registry::instance() {
    static Registry registry;
    return registry;
}

void Registry::add(std::string suite, std::string name, std::function<void()> body) {
    cases_.push_back(TestCase{std::move(suite), std::move(name), std::move(body)});
}

Registrar::Registrar(const char* suite, const char* name, void (*function)()) {
    Registry::instance().add(suite, name, function);
}

std::string formatNumber(double value) {
    std::ostringstream stream;
    stream.precision(9);
    stream << value;
    return stream.str();
}

void reportFailure(const char* file, int line, const std::string& expression, const std::string& detail) {
    std::lock_guard<std::mutex> lock(g_reportMutex);
    ++g_failuresInCurrentTest;
    std::cout << "    FAILED " << expression << "  (" << file << ':' << line << ')';
    if (!detail.empty()) {
        std::cout << "  " << detail;
    }
    std::cout << '\n';
}

int runAll(const std::string& suiteFilter, bool verbose) {
    g_verbose = verbose;
    const std::vector<TestCase>& cases = Registry::instance().cases();

    std::size_t executed = 0;
    std::size_t failed = 0;
    for (const TestCase& test : cases) {
        if (!suiteFilter.empty() && suiteFilter != "all" && test.suite != suiteFilter) {
            continue;
        }
        ++executed;
        g_failuresInCurrentTest = 0;
        if (verbose) {
            std::cout << "  running " << test.suite << '.' << test.name << '\n';
        }
        try {
            test.body();
        } catch (const Failure& failure) {
            ++g_failuresInCurrentTest;
            std::cout << "    ABORTED " << failure.message() << '\n';
        } catch (const std::exception& error) {
            ++g_failuresInCurrentTest;
            std::cout << "    EXCEPTION " << error.what() << '\n';
        } catch (...) {
            ++g_failuresInCurrentTest;
            std::cout << "    EXCEPTION unknown\n";
        }
        if (g_failuresInCurrentTest > 0) {
            ++failed;
            std::cout << "  [" << test.suite << "] " << test.name << ": " << g_failuresInCurrentTest
                      << " failure(s)\n";
        } else if (verbose) {
            std::cout << "  ok " << test.suite << '.' << test.name << '\n';
        }
    }

    std::cout << (failed == 0 ? "PASS" : "FAIL") << ": " << (executed - failed) << '/' << executed
              << " tests passed";
    if (!suiteFilter.empty()) {
        std::cout << " (suite filter: " << suiteFilter << ')';
    }
    std::cout << '\n';
    return failed == 0 ? 0 : 1;
}

}  // namespace fe::test
