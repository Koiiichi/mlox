# MLOX

MLOX is a C implementation of the Lox scripting language from *Crafting Interpreters*, complete with a bytecode compiler, garbage-collected virtual machine, and interactive REPL. The project mirrors the behaviour of the book's `clox` interpreter while laying the groundwork for future language extensions.

## Features

- Bytecode-based virtual machine with dynamic typing and garbage collection
- Scanner, Pratt parser, and compiler for the full Lox language
- First-class functions, closures, classes, inheritance, and bound methods
- Native `clock()` function exposed to user code
- Interactive REPL and ability to run scripts from disk
- Comprehensive Doxygen documentation for all runtime modules

## Prerequisites

### Windows Users

If you're on Windows, you'll need to install development tools before building:

1. **Install Windows Package Manager tools**:
   ```powershell
   # Install make
   winget install GnuWin32.Make
   
   # Install MSYS2 for gcc and development tools
   winget install MSYS2.MSYS2
   ```

2. **Set up MSYS2**:
   - Open "MSYS2 MSYS" from your Start menu
   - Install gcc, make, and diff utilities:
     ```bash
     pacman -S gcc make diffutils
     ```

3. **Add tools to PATH** (for PowerShell/Git Bash usage):
   ```powershell
   # Add GnuWin32 make to PATH
   [Environment]::SetEnvironmentVariable("PATH", $env:PATH + ";C:\Program Files (x86)\GnuWin32\bin", [EnvironmentVariableTarget]::User)
   
   # Add MSYS2 tools to PATH
   [Environment]::SetEnvironmentVariable("PATH", $env:PATH + ";C:\msys64\usr\bin", [EnvironmentVariableTarget]::User)
   ```

   **Note**: Restart your terminal after setting PATH variables.

**Recommended**: Use the MSYS2 terminal for building and testing, as it provides a complete Unix-like development environment.

### Linux/macOS Users

Ensure you have `gcc` and `make` installed:
- **Ubuntu/Debian**: `sudo apt install build-essential`
- **macOS**: Install Xcode Command Line Tools: `xcode-select --install`

## Building

**Windows users**: Use MSYS2 terminal or ensure development tools are in your PATH (see Prerequisites above).

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

## Troubleshooting

### Windows Common Issues

**"make: command not found"**
- Ensure you've installed GnuWin32.Make: `winget install GnuWin32.Make`
- Add make to PATH or restart your terminal after installation

**"gcc: command not found"**
- Ensure you've installed MSYS2 and gcc: `pacman -S gcc` in MSYS2 terminal
- Add MSYS2 to PATH: `$env:PATH += ";C:\msys64\usr\bin"` (temporary) or use the permanent PATH setup above

**"diff: command not found" during tests**
- Install diffutils in MSYS2: `pacman -S diffutils`
- Ensure MSYS2 usr/bin is in your PATH

**Test failures with line ending differences**
- This is handled automatically by the Makefile's `--strip-trailing-cr` option
- If issues persist, use MSYS2 terminal instead of PowerShell/Git Bash

### General Issues

**Build fails with missing headers**
- Ensure you have a complete C development environment installed
- On Linux: `sudo apt install build-essential`
- On macOS: `xcode-select --install`

## License

This project is distributed under the MIT License. See `LICENSE` for details.
