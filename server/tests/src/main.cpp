#include <exception>
#include <fmt/core.h>
#include "tests/includes/test_framework.h"

namespace test
{
std::vector<TestCase> &registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

Registrar::Registrar(const char *name, std::function<void()> fn)
{
    registry().push_back({name, std::move(fn)});
}
} // namespace test

int main()
{
    int failures = 0;
    for (const auto &test_case : test::registry())
    {
        try
        {
            test_case.fn();
            fmt::print("[PASS] {}\n", test_case.name);
        }
        catch (const std::exception &ex)
        {
            ++failures;
            fmt::print(stderr, "[FAIL] {}: {}\n", test_case.name, ex.what());
        }
        catch (...)
        {
            ++failures;
            fmt::print(stderr, "[FAIL] {}: unknown exception\n", test_case.name);
        }
    }

    if (failures > 0)
    {
        fmt::print(stderr, "{} test(s) failed\n", failures);
        return 1;
    }

    fmt::print("{} test(s) passed\n", test::registry().size());
    return 0;
}
