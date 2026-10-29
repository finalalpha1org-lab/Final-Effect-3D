// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors
// Zero-dependency C++20 unit test harness for deterministic, offline-capable testing.

#pragma once

#include <cmath>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace finaleffect::testing {

struct FailureRecord {
    std::string file;
    int line{0};
    std::string expression;
    std::string detail;
};

class TestContext {
public:
    void recordFailure(std::string_view file, int line, std::string_view expr, std::string detail = {}) {
        failures_.push_back(FailureRecord{
            .file = std::string(file),
            .line = line,
            .expression = std::string(expr),
            .detail = std::move(detail),
        });
    }

    [[nodiscard]] bool passed() const noexcept { return failures_.empty(); }
    [[nodiscard]] const std::vector<FailureRecord>& failures() const noexcept { return failures_; }

private:
    std::vector<FailureRecord> failures_;
};

struct TestCase {
    std::string suite;
    std::string name;
    std::function<void(TestContext&)> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> cases;
    return cases;
}

struct Registrar {
    Registrar(std::string suite, std::string name, std::function<void(TestContext&)> fn) {
        registry().push_back(TestCase{
            .suite = std::move(suite),
            .name = std::move(name),
            .fn = std::move(fn),
        });
    }
};

class AbortCaseException final : public std::exception {
public:
    [[nodiscard]] const char* what() const noexcept override {
        return "Required assertion failed";
    }
};

inline int runAllTests(int argc, char** argv) {
    std::string filter;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg.starts_with("--filter=")) {
            filter = std::string(arg.substr(9));
        }
    }

    std::size_t passed_count = 0;
    std::size_t failed_count = 0;

    for (const auto& tc : registry()) {
        const std::string full_name = tc.suite + "." + tc.name;
        if (!filter.empty() && full_name.find(filter) == std::string::npos) {
            continue;
        }

        TestContext ctx;
        try {
            tc.fn(ctx);
        } catch (const AbortCaseException&) {
            // Recorded in ctx before throwing.
        } catch (const std::exception& ex) {
            ctx.recordFailure(__FILE__, __LINE__, "uncaught std::exception", ex.what());
        } catch (...) {
            ctx.recordFailure(__FILE__, __LINE__, "uncaught unknown exception", "");
        }

        if (ctx.passed()) {
            ++passed_count;
            std::cout << "[ PASS ] " << full_name << '\n';
        } else {
            ++failed_count;
            std::cerr << "[ FAIL ] " << full_name << '\n';
            for (const auto& f : ctx.failures()) {
                std::cerr << "         " << f.file << ":" << f.line << " -> " << f.expression;
                if (!f.detail.empty()) {
                    std::cerr << " (" << f.detail << ")";
                }
                std::cerr << '\n';
            }
        }
    }

    std::cout << "Summary: " << passed_count << " passed, " << failed_count << " failed.\n";
    return failed_count == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

template <typename LhsT, typename RhsT>
inline void checkEqual(
    TestContext& ctx,
    std::string_view file,
    int line,
    std::string_view expr,
    const LhsT& lhs,
    const RhsT& rhs) {
    if (!(lhs == rhs)) {
        ctx.recordFailure(file, line, expr, "equality check failed");
    }
}

} // namespace finaleffect::testing

#define FE_TEST(Suite, Name)                                                                      \
    static void fe_test_##Suite##_##Name(::finaleffect::testing::TestContext& fe_ctx);            \
    static ::finaleffect::testing::Registrar fe_reg_##Suite##_##Name(                             \
        #Suite, #Name, &fe_test_##Suite##_##Name);                                                \
    static void fe_test_##Suite##_##Name(::finaleffect::testing::TestContext& fe_ctx)

#define FE_EXPECT_TRUE(Expr)                                                                      \
    do {                                                                                          \
        if (!(Expr)) {                                                                            \
            fe_ctx.recordFailure(__FILE__, __LINE__, #Expr, "expected true");                     \
        }                                                                                         \
    } while (false)

#define FE_EXPECT_FALSE(Expr)                                                                     \
    do {                                                                                          \
        if (Expr) {                                                                               \
            fe_ctx.recordFailure(__FILE__, __LINE__, #Expr, "expected false");                    \
        }                                                                                         \
    } while (false)

#define FE_REQUIRE_TRUE(Expr)                                                                     \
    do {                                                                                          \
        if (!(Expr)) {                                                                            \
            fe_ctx.recordFailure(__FILE__, __LINE__, #Expr, "required true");                     \
            throw ::finaleffect::testing::AbortCaseException{};                                   \
        }                                                                                         \
    } while (false)

#define FE_EXPECT_EQ(Lhs, Rhs)                                                                    \
    ::finaleffect::testing::checkEqual(fe_ctx, __FILE__, __LINE__, #Lhs " == " #Rhs, (Lhs), (Rhs))

#define FE_EXPECT_NEAR(ValA, ValB, Tol)                                                           \
    do {                                                                                          \
        const double fe_a = static_cast<double>(ValA);                                            \
        const double fe_b = static_cast<double>(ValB);                                            \
        const double fe_tol = static_cast<double>(Tol);                                           \
        if (!(std::fabs(fe_a - fe_b) <= fe_tol)) {                                                \
            std::ostringstream fe_oss;                                                            \
            fe_oss << "actual=" << fe_a << ", expected=" << fe_b << ", tol=" << fe_tol;           \
            fe_ctx.recordFailure(__FILE__, __LINE__, #ValA " ~= " #ValB, fe_oss.str());           \
        }                                                                                         \
    } while (false)
