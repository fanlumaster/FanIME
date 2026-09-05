#pragma once

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace test
{
struct TestCase
{
    std::string name;
    std::function<void()> fn;
};

std::vector<TestCase> &registry();

struct Registrar
{
    Registrar(const char *name, std::function<void()> fn);
};
} // namespace test

#define TEST_CASE(name)                                                                                                \
    static void name();                                                                                                \
    static test::Registrar name##_registrar(#name, name);                                                              \
    static void name()

#define REQUIRE(expr)                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(expr))                                                                                                   \
        {                                                                                                              \
            throw std::runtime_error("Requirement failed: " #expr);                                                    \
        }                                                                                                              \
    } while (false)

// Layout maths runs in floats, so exact comparison would make the tests fragile
// for reasons that have nothing to do with the behaviour under test.
#define REQUIRE_NEAR(actual, expected)                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        const float _a = static_cast<float>(actual);                                                                   \
        const float _e = static_cast<float>(expected);                                                                 \
        if (!(std::fabs(_a - _e) <= 0.01f))                                                                            \
        {                                                                                                              \
            throw std::runtime_error("Requirement failed: " #actual " ~= " #expected);                                 \
        }                                                                                                              \
    } while (false)
