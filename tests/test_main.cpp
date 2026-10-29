// SPDX-License-Identifier: GPL-3.0-only
#include <cstring>
#include <iostream>
#include <string>

#include "TestFramework.hpp"

int main(int argc, char** argv) {
    std::string suiteFilter;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--suite" && i + 1 < argc) {
            suiteFilter = argv[++i];
        } else if (argument == "--verbose" || argument == "-v") {
            verbose = true;
        } else if (argument == "--list") {
            for (const fe::test::TestCase& test : fe::test::Registry::instance().cases()) {
                std::cout << test.suite << '.' << test.name << '\n';
            }
            return 0;
        } else {
            std::cerr << "unknown argument: " << argument << '\n';
            std::cerr << "usage: fe3d_tests [--suite <name>] [--verbose] [--list]\n";
            return 2;
        }
    }

    std::cout << "Final Effect 3D test suite\n";
    return fe::test::runAll(suiteFilter, verbose);
}
