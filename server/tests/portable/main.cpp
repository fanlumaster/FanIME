// The main test executable's runner formats with fmt. This one deliberately depends on nothing
// outside the standard library, so that a sanitized build needs no package manager.
#include <cstdio>
#include <exception>

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
            std::printf("[PASS] %s\n", test_case.name.c_str());
        }
        catch (const std::exception &ex)
        {
            ++failures;
            std::fprintf(stderr, "[FAIL] %s: %s\n", test_case.name.c_str(), ex.what());
        }
        catch (...)
        {
            ++failures;
            std::fprintf(stderr, "[FAIL] %s: unknown exception\n", test_case.name.c_str());
        }
    }
    std::printf("%zu test(s), %d failure(s)\n", test::registry().size(), failures);
    return failures == 0 ? 0 : 1;
}
