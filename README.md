# Parallel Simulation of Unary Arithmetic State Machines

This folder implements the supplied Fall 2026 proposal. Each arithmetic job is a sequential, explicit tape/head simulation. OpenMP distributes independent jobs. The reference ZIP was used for context only; its implementation and reported timings were not copied.

## Build

Requires a C++17 compiler with OpenMP and CMake. On Linux: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j`. On Windows, use a compiler with OpenMP support, such as MSVC or MinGW GCC.

Run the C++ test suite with `ctest --test-dir build --output-on-failure`. Run the analysis test with `python -m unittest discover -s tests -v`.

## Run

* `build/unary test` checks boundary operands and 1,000 seeded random jobs against arithmetic expectations, then compares serial with 1, 2, 4, and 8 threads under static and dynamic scheduling.
* `build/unary show mul 3 4` prints the final tape, answer, and transition count for a small job.
* `build/unary bench 10000 30 10 12345 > raw.csv` performs a warm-up and ten measured repetitions per configuration. Increase job count to 50,000, 100,000, and 250,000 after calibrating memory and run time on the target computer.
* `python analysis/plot.py raw.csv` writes a median/variation CSV and execution-time, speedup, and efficiency SVG graphs using only Python's standard library.

The tape begins as `A#B|`, where A and B are unary runs of `1`. The area after `|` holds output. `x` marks consumed input, `y` marks a temporarily consumed B symbol, `t` marks a trial division subtraction, and `_` is blank. Subtraction is truncated at zero. Division/modulo reject a zero divisor. Expected arithmetic is used only by validation, outside the simulation kernel.

Benchmarks produce raw CSV on stdout. Correctness checks and warm-up occur outside each timed region. Tape allocation and simulation occur inside the region for both serial and parallel modes. Plotting and console output occur outside it.
