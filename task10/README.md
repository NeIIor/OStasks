# Task 10: Boolean expression parser + truth table (C)

## Problem

Read a single line with a boolean expression and print its truth table.

- **CLI args**: none
- **Variables**: single letters `A`..`Z`
- **Operators**: `NOT`, `AND`, `OR`
- **Parentheses**: `(`, `)`
- **On invalid input**: exit with a non-zero code

Output is a truth table with a header:

- variable names sorted in ascending order, then `Result`

Example:

```bash
echo "A AND B" | ./task10
```

Expected output:

```
A B Result
0 0 0
0 1 0
1 0 0
1 1 1
```

## Build

From the `task10/` directory:

```bash
make
```

Extra compile flags can be passed via `CFLAGS`:

```bash
make CFLAGS="-O2 -g -Wall -Wextra"
```

## Notes on parsing

The parser is a small recursive-descent parser with precedence:

1. `NOT` (unary)
2. `AND`
3. `OR`

Whitespace between tokens is optional.

