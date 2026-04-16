# Task 1: Parallel integer sort (pthreads, C)

## Problem

Write a multithreaded C program (using `pthreads`) that sorts an integer array in parallel.

- **CLI**: `task1 <threads>`
- **stdin**: unsorted integers separated by whitespace
- **stdout**: sorted integers separated by a single space

Example:

```bash
echo "3 2 1" | ./task1 12
# 1 2 3
```

## Build

From the `task1/` directory:

```bash
make
```

You may pass extra compiler flags via `CFLAGS`:

```bash
make CFLAGS="-O2 -g -Wall -Wextra"
```

## Approach

1. Read all integers from `stdin` into a dynamic array.
2. Split the array into **T contiguous runs** (where \(T\) is the number of threads).
3. Sort each run in parallel using `qsort` (one thread per run).
4. Merge the sorted runs using a **k-way merge** implemented with a small **min-heap**:
   - the heap stores the current "head" element of each run
   - repeatedly pop the smallest element, output it, then push the next element from the same run

This keeps the heap size bounded by \(T\) and produces the globally sorted output stream.

