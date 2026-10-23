---
name: tests-cpp
description: Write tests with GoogleTest. Use it when the user asks for unit tests.
---

# Tests: GoogleTest

- First read the code under test and the existing tests to reuse their style.
- One test per behaviour: `TEST(SuiteName, expectedBehaviour)`; `TEST_F` fixtures when state is shared.
- Use `EXPECT_*` (keeps going) unless the rest of the test depends on the result (`ASSERT_*`).
- Every new test file must be declared in the build system.
- Cover the nominal case, edge cases (empty, zero, limits) and errors.

Run with `xmake test` and fix until green.
