# MLOX

MLOX is a C implementation of the Lox scripting language from *Crafting Interpreters*, complete with a bytecode compiler, garbage-collected virtual machine, and interactive REPL. The project mirrors the behaviour of the book's `clox` interpreter while laying the groundwork for future language extensions.

## Features

- Bytecode-based virtual machine with dynamic typing and garbage collection
- Scanner, Pratt parser, and compiler for the full Lox language
- First-class functions, closures, classes, inheritance, and bound methods
- Native `clock()` function exposed to user code
- Interactive REPL and ability to run scripts from disk
- Comprehensive Doxygen documentation for all runtime modules

## Building

```bash
make
```

The interpreter binary (`mlox`) is produced in the repository root. Compilation uses `gcc` with `-std=c99`, `-Wall`, and `-Wextra` by default.

## Running

To enter the interactive REPL:

```bash
./mlox
```

To run a script file:

```bash
./mlox path/to/script.mlox
```

## Tests

A lightweight regression test suite lives in `tests/`. Execute all scripts and verify their expected output using:

```bash
make test
```

Each test script has a matching `.expected` file whose contents are diffed against the interpreter's output.

## Documentation

The codebase is documented with Doxygen. Generate the HTML documentation into `docs/html` via:

```bash
make docs
```

## Project Layout

```
mlox/
├── Makefile          # Build, test, and documentation targets
├── README.md         # Project overview and usage
├── docs/             # Doxygen output directory (generated)
├── src/              # Interpreter source files
├── tests/            # Regression test scripts and expected outputs
└── LICENSE
```

## License

This project is distributed under the MIT License. See `LICENSE` for details.
