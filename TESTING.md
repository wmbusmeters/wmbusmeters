# Testing internals

## Running

```
make testinternals
./build/testinternals
```

## Two test systems in one binary

`testinternals` runs two kinds of tests back-to-back:

1. **Legacy X-macro tests** — the existing `test_*()` functions declared via `LIST_OF_TESTS` in `src/testinternals.cc`. These cover CRC, dvparser, formulas, address matching, and more.

2. **Describe/it tests** — the new framework in `src/test.h`, described below.

## Writing a new test suite

Create a file `src/path/to/module.test.h` and register it in `src/testinternals.cc` with a single `#include`.

**`src/utils/foo.test.h`**
```cpp
#ifndef FOO_TEST_H
#define FOO_TEST_H

#include "test.h"
#include "utils/foo.h"

static auto _foo_suite = describe("foo", []()
{
    it("does something", []()
    {
        assert(foo(1) == 2);
    });

    it("handles edge case", []()
    {
        std::vector<int> v;
        assert(v.empty());
        assert(fooList(v) == 0);
    });
});

#endif // FOO_TEST_H
```

**`src/testinternals.cc`** — add one line with the other includes:
```cpp
#include"foo.test.h"
```

The Makefile dependency on `$(shell find src -name '*.test.h' -type f)` ensures a rebuild whenever any `.test.h` file changes.

## How assert works here

`test.h` replaces the standard `assert` macro with a non-aborting version. Inside an `it()` block a failing assert records the message and lets the rest of the suite finish. Outside an `it()` block (i.e. in the legacy X-macro tests) a failing assert still prints to stderr and aborts, preserving the original behaviour.

```
src/utils/slip.test.h:65: assert(to.size() == 4) failed
```

## Output

```
slip
  ✓ slipAllEND returns true for empty vector
  ✓ addSlipFraming escapes 0xc0 in payload
  ✗ removeSlipFraming round-trips plain data
    src/utils/slip.test.h:65: assert(to.size() == 4) failed

2/3 passed — FAILURES!
```

Suite names are printed in bold. Passing tests show a green `✓`, failing tests a red `✗` with the assertion detail dimmed on the next line.
