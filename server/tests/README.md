# Tests

This project now includes a lightweight regression test target wired into the main CMake build.

## What It Covers

Current tests focus on high-value IME behaviors that are easy to accidentally break during refactors:

- `shuangpin` scheme request construction
- `quanpin` scheme request construction
- case-preserving shuangpin segmentation
- full-helpcode detection
- `engine + shuangpin` continuation behavior
- cloud suggestion trigger timing for shuangpin

The goal is not to fully simulate the whole IME UI pipeline yet. The goal is to protect the core input rules and
session behavior.

## Build The Tests

From the repository root:

```powershell
cmake --build build --config Debug --target MetasequoiaImeServerTests
```

## Run The Tests

Recommended:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

You can also run from inside the build directory:

```powershell
cd build
ctest -C Debug --output-on-failure
```

## Common Pitfall

If you run `ctest` from the source root directly, you may see:

```text
No tests were found!!!
```

That usually means `ctest` is looking at the source directory instead of the build directory.

Use one of these instead:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

or

```powershell
cd build
ctest -C Debug --output-on-failure
```

## Test Layout

- `tests/src/main.cpp`
  - tiny custom test runner
- `tests/includes/test_framework.h`
  - lightweight assertion helpers
- `tests/src/test_shuangpin_scheme.cpp`
  - shuangpin scheme unit tests
- `tests/src/test_quanpin_scheme.cpp`
  - quanpin scheme unit tests
- `tests/src/test_shuangpin_query.cpp`
  - query/helper function tests
- `tests/src/test_engine_shuangpin_session.cpp`
  - session-level behavior tests

## Recommended Practice

When you change input behavior, add a regression test if the change affects:

- input segmentation
- space/enter/backspace behavior
- helpcode handling
- continuation after candidate selection
- create-word flow
- cloud suggestion timing

The best tests in this project are the ones that capture a concrete input example that previously broke.
