// Final Effect 3D - minimal, dependency-free test harness.
// SPDX-License-Identifier: GPL-3.0-only
//
// Rationale: the engine must build and test on a machine with no network access,
// so no external test framework is used. This harness is original code and knows
// about exactly two things: registering test functions and reporting failures.
//
// Note: this header intentionally contains the project's only singleton (the
// registry), because test registration happens before main() runs.
#pragma once

#include <cmath>
#include <cstddef>
#include <exception>
#include <functional>
#include <sstream>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <string>
#include <utility>
#include <vector>

namespace fe::test {

struct TestCase {
    std::string suite;
    std::string name;
    std::function<void()> body;
};

class Registry {
public:
    static Registry& instance();

    void add(std::string suite, std::string name, std::function<void()> body);
    const std::vector<TestCase>& cases() const noexcept { return cases_; }

private:
    std::vector<TestCase> cases_;
};

/// Thrown by FE3D_REQUIRE to abort the current test.
class Failure final : public std::exception {
public:
    explicit Failure(std::string message) : message_(std::move(message)) {}
    const char* what() const noexcept override { return message_.c_str(); }
    const std::string& message() const noexcept { return message_; }

private:
    std::string message_;
};

/// Records a non-fatal failure of the test that is currently running.
void reportFailure(const char* file, int line, const std::string& expression, const std::string& detail);

/// Runs the registered tests. suiteFilter selects one suite ("all" or "" = every
/// suite). Returns 0 when every test passed.
int runAll(const std::string& suiteFilter, bool verbose);

struct Registrar {
    Registrar(const char* suite, const char* name, void (*function)());
};

std::string formatNumber(double value);

namespace detail {

template <typename T, typename = void>
struct IsStreamable : std::false_type {};

template <typename T>
struct IsStreamable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>>
    : std::true_type {};

template <typename T, typename = void>
struct HasValueMember : std::false_type {};

template <typename T>
struct HasValueMember<T, std::void_t<decltype(std::declval<const T&>().value)>> : std::true_type {};

}  // namespace detail

/// Formats any comparable value for failure messages, so FE3D_CHECK_EQ works for
/// strings, enums, numbers and small wrapper types such as ObjectId alike.
template <typename T>
std::string formatValue(const T& value) {
    if constexpr (detail::IsStreamable<T>::value) {
        std::ostringstream stream;
        stream << value;
        return stream.str();
    } else if constexpr (std::is_enum_v<T>) {
        return std::to_string(static_cast<long long>(value));
    } else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
        return std::to_string(value);
    } else if constexpr (detail::HasValueMember<T>::value) {
        return formatValue(value.value);
    } else {
        return std::string("<") + typeid(T).name() + ">";
    }
}

inline std::string describe(const std::string& actual, const std::string& expected) {
    return "actual=" + actual + " expected=" + expected;
}

}  // namespace fe::test

#define FE3D_TEST(suite, name)                                                                   \
    static void fe3d_test_##suite##_##name();                                                    \
    static const ::fe::test::Registrar fe3d_registrar_##suite##_##name(#suite, #name,             \
                                                                      &fe3d_test_##suite##_##name); \
    static void fe3d_test_##suite##_##name()

#define FE3D_CHECK(expression)                                                                   \
    do {                                                                                         \
        if (!(expression)) {                                                                     \
            ::fe::test::reportFailure(__FILE__, __LINE__, #expression, "");                       \
        }                                                                                        \
    } while (false)

#define FE3D_CHECK_MSG(expression, detail)                                                        \
    do {                                                                                         \
        if (!(expression)) {                                                                     \
            ::fe::test::reportFailure(__FILE__, __LINE__, #expression, (detail));                 \
        }                                                                                        \
    } while (false)

#define FE3D_REQUIRE(expression)                                                                  \
    do {                                                                                         \
        if (!(expression)) {                                                                     \
            throw ::fe::test::Failure(std::string("required: ") + #expression + " at " +          \
                                      __FILE__ + ":" + std::to_string(__LINE__));                 \
        }                                                                                        \
    } while (false)

#define FE3D_CHECK_NEAR(actual, expected, tolerance)                                              \
    do {                                                                                         \
        const double fe3d_actual = static_cast<double>(actual);                                    \
        const double fe3d_expected = static_cast<double>(expected);                                \
        const double fe3d_tolerance = static_cast<double>(tolerance);                              \
        if (!(std::fabs(fe3d_actual - fe3d_expected) <= fe3d_tolerance)) {                        \
            ::fe::test::reportFailure(__FILE__, __LINE__, #actual " ~= " #expected,                \
                                      "actual=" + ::fe::test::formatNumber(fe3d_actual) +          \
                                          " expected=" + ::fe::test::formatNumber(fe3d_expected) +  \
                                          " tolerance=" + ::fe::test::formatNumber(fe3d_tolerance));\
        }                                                                                        \
    } while (false)

#define FE3D_CHECK_EQ(actual, expected)                                                           \
    do {                                                                                         \
        const auto& fe3d_actual_value = (actual);                                                 \
        const auto& fe3d_expected_value = (expected);                                             \
        if (!(fe3d_actual_value == fe3d_expected_value)) {                                        \
            ::fe::test::reportFailure(__FILE__, __LINE__, #actual " == " #expected,                \
                                      ::fe::test::describe(::fe::test::formatValue(fe3d_actual_value), \
                                                           ::fe::test::formatValue(fe3d_expected_value))); \
        }                                                                                        \
    } while (false)
