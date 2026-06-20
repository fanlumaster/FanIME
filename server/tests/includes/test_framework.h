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
}

#define TEST_CASE(name)                                                                                                 \
    static void name();                                                                                                 \
    static test::Registrar name##_registrar(#name, name);                                                              \
    static void name()

#define REQUIRE(expr)                                                                                                   \
    do                                                                                                                  \
    {                                                                                                                   \
        if (!(expr))                                                                                                    \
        {                                                                                                               \
            throw std::runtime_error("Requirement failed: " #expr);                                                    \
        }                                                                                                               \
    } while (false)

#define REQUIRE_EQ(lhs, rhs)                                                                                            \
    do                                                                                                                  \
    {                                                                                                                   \
        const auto &_lhs = (lhs);                                                                                       \
        const auto &_rhs = (rhs);                                                                                       \
        if (!(_lhs == _rhs))                                                                                            \
        {                                                                                                               \
            throw std::runtime_error("Requirement failed: " #lhs " == " #rhs);                                        \
        }                                                                                                               \
    } while (false)
