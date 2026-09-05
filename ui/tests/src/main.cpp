#include "tests/includes/test_framework.h"

#include <cstdio>
#include <exception>

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
            std::printf("[PASS] %s\n", test_case.name.c_str());
        }
        catch (const std::exception &ex)
        {
            ++failures;
            std::printf("[FAIL] %s: %s\n", test_case.name.c_str(), ex.what());
        }
    }

    std::printf("%d test(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}
