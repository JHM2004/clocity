# clocity

**Blazing-fast code line counter written in pure C. Zero dependencies.**

clocity counts blank lines, comment lines, code lines, and mixed lines (code + comment) across **58+ programming languages**. Built as a single-binary, zero-dependency alternative to [tokei](https://github.com/XAMPPRocky/tokei) and [cloc](https://github.com/AlDanial/cloc), with Windows-native support and cross-platform compatibility.

## Benchmark

Tested on Windows 10, Intel Xeon, with Git source code (~1.3M lines, 3273 files):

| Tool | Time | Relative |
|------|------|----------|
| **clocity** | **0.39s** | **1x** |
| tokei v12.1.2 | 1.08s | 2.8x slower |
| cloc v2.08 (Perl) | 43s | 110x slower |

> clocity is **~3x faster than tokei** and **~100x faster than cloc** on large projects.

## Features

- **58+ languages** — C/C++, Python, JavaScript, TypeScript, Go, Rust, Java, C#, Ruby, Swift, Kotlin, and more
- **State machine parser** — Accurate classification of CODE / COMMENT / BLANK / MIXED lines
- **String-aware** — Comments inside strings (e.g. `"http://"`) are never misclassified
- **Nested comments** — Supports nested block comments (Rust, Swift, Haskell, D, Nim, etc.)
- **Multiple output formats** — Aligned table (with colors), JSON, CSV, YAML
- **Parallel processing** — Thread pool with producer-consumer model
- **gitignore support** — Respects `.gitignore` rules during scanning
- **Shebang detection** — Identifies languages from `#!/usr/bin/env python3` etc.
- **Zero dependencies** — Pure C11, compiles with GCC/Clang/MSVC
- **Cross-platform** — Windows (native), Linux, macOS

## Quick Start

### Build

```bash
# GCC / Clang
make

# MSVC
build.bat

# Manual
gcc -std=c11 -O2 -Iinclude -o clocc src/*.c src/os_win32.c -lkernel32   # Windows
gcc -std=c11 -O2 -Iinclude -o clocc src/*.c src/os_unix.c -lpthread      # Linux/macOS
```

### Usage

```bash
# Count a directory
clocc /path/to/project

# JSON output
clocc -f json /path/to/project

# Filter by language, sort by files
clocc -t C,Python -s files /path/to/project

# Use 8 threads
clocc -j 8 /path/to/project

# CSV output, exclude empty languages
clocc -f csv --exclude-empty /path/to/project
```

### Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Show help |
| `-v, --version` | Show version |
| `-f FORMAT` | Output format: `table` (default), `json`, `csv`, `yaml` |
| `-s FIELD` | Sort by: `code` (default), `files`, `lines`, `comment`, `blank`, `mixed` |
| `-r, --sort-reverse` | Sort in ascending order |
| `-t LANGS` | Filter by language (comma-separated) |
| `-j N` | Use N threads (0 = auto-detect) |
| `--no-color` | Disable colored output |
| `--exclude-empty` | Exclude languages with zero files |
| `--verbose` | Verbose output |

## Output Example

```
Language   | Files | Blank | Comment |  Code | Mixed |  Lines
-----------+-------+-------+---------+-------+-------+-------
C          |     9 |   391 |     238 |  2745 |    15 |  3389
Python     |     3 |   120 |      45 |   890 |     8 |  1063
JavaScript |     5 |    80 |      30 |   650 |     5 |   765
-----------+-------+-------+---------+-------+-------+-------
SUM        |    17 |   591 |     313 |  4285 |    28 |  5217
```

## Project Structure

```
src/
  main.c        CLI entry point, argument parsing
  scanner.c     File traversal with gitignore support
  counter.c     State machine line classification engine
  language.c    Language definitions (58+ languages)
  output.c      Output formatting (table/JSON/CSV/YAML)
  thread.c      Thread pool for parallel processing
  os_win32.c    Windows platform adapter
  os_unix.c     Linux/macOS platform adapter
  utils.c       Utility functions
include/
  clocc.h       Public API header
tests/
  test_all.c    Unit test runner
  fixtures/     Test files for each language
```

## Accuracy

Compared against [tokei](https://github.com/XAMPPRocky/tokei) v12.1.2:

- Total line count: **100% match**
- Code + Mixed lines: **matches tokei's code count** (tokei has no "mixed" category)
- String-aware comment detection: comments inside string literals are never misclassified

## Supported Languages

C, C++, C#, Java, JavaScript, TypeScript, JSX, TSX, Python, Ruby, Go, Rust, Swift,
Kotlin, Scala, Haskell, OCaml, F#, Erlang, Elixir, Clojure, R, MATLAB, Julia, Lua,
Perl, PHP, Shell/Bash, PowerShell, Dart, Groovy, Objective-C, D, Pascal, Ada,
Fortran, COBOL, Lisp, Scheme, Visual Basic, VB.NET, SQL, HTML, CSS, XML, YAML,
TOML, Markdown, Assembly, Makefile, CMake, Dockerfile, Terraform (HCL), Vim Script,
Emacs Lisp, Zig, V (Vlang), Nim, Crystal, Vue — and more.

## License

MIT

---

[中文](README.md)
