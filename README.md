# clocity

**纯C打造的极速代码行统计工具。零依赖，58+语言。**

clocity 统计空行、注释行、代码行和混合行（代码+注释），支持 **58+ 编程语言**。作为 [tokei](https://github.com/XAMPPRocky/tokei) 和 [cloc](https://github.com/AlDanial/cloc) 的零依赖单文件替代方案，Windows 原生支持，跨平台兼容。

## 性能基准

测试环境：Windows 10，Intel Xeon，Git 源码（~130万行，3273文件），3轮取中位数：

| 工具 | 耗时 | 相对速度 |
|------|------|----------|
| **clocity** | **0.43s** | **1x** |
| tokei v12.1.2 | 1.14s | 慢 2.7 倍 |
| cloc v2.08 (Perl) | 23.1s | 慢 54 倍 |

> clocity 比 tokei 快 **~3倍**，比 Perl 版 cloc 快 **~50倍**。

## 特性

- **58+ 语言** — C/C++、Python、JavaScript、TypeScript、Go、Rust、Java、C#、Ruby、Swift、Kotlin 等
- **状态机解析器** — 精确分类 CODE / COMMENT / BLANK / MIXED 行
- **字符串感知** — 字符串内的注释（如 `"http://"`）不会被误判
- **嵌套注释** — 支持 Rust、Swift、Haskell、D、Nim 等的嵌套块注释
- **多种输出格式** — 对齐表格（带颜色）、JSON、CSV、YAML
- **并行处理** — 线程池 + 生产者-消费者模型
- **gitignore 支持** — 扫描时遵循 `.gitignore` 规则
- **Shebang 检测** — 从 `#!/usr/bin/env python3` 推断语言
- **零依赖** — 纯 C11，GCC/Clang/MSVC 均可编译
- **跨平台** — Windows（原生）、Linux、macOS

## 快速开始

### 编译

```bash
# GCC / Clang
make

# MSVC
build.bat

# 手动编译
gcc -std=c11 -O2 -Iinclude -o clocc src/*.c src/os_win32.c -lkernel32   # Windows
gcc -std=c11 -O2 -Iinclude -o clocc src/*.c src/os_unix.c -lpthread      # Linux/macOS
```

### 使用

```bash
# 统计目录
clocc /path/to/project

# JSON 输出
clocc -f json /path/to/project

# 按语言过滤，按文件数排序
clocc -t C,Python -s files /path/to/project

# 使用 8 线程
clocc -j 8 /path/to/project

# CSV 输出，排除空语言
clocc -f csv --exclude-empty /path/to/project
```

### 命令行选项

| 选项 | 说明 |
|------|------|
| `-h, --help` | 显示帮助 |
| `-v, --version` | 显示版本 |
| `-f FORMAT` | 输出格式：`table`（默认）、`json`、`csv`、`yaml` |
| `-s FIELD` | 排序字段：`code`（默认）、`files`、`lines`、`comment`、`blank`、`mixed` |
| `-r, --sort-reverse` | 升序排列 |
| `-t LANGS` | 按语言过滤（逗号分隔） |
| `-j N` | 使用 N 个线程（0 = 自动检测） |
| `--no-color` | 禁用彩色输出 |
| `--exclude-empty` | 排除零文件语言 |
| `--verbose` | 详细输出 |

## 输出示例

```
Language   | Files | Blank | Comment |  Code | Mixed |  Lines
-----------+-------+-------+---------+-------+-------+-------
C          |     9 |   391 |     238 |  2745 |    15 |  3389
Python     |     3 |   120 |      45 |   890 |     8 |  1063
JavaScript |     5 |    80 |      30 |   650 |     5 |   765
-----------+-------+-------+---------+-------+-------+-------
SUM        |    17 |   591 |     313 |  4285 |    28 |  5217
```

## 项目结构

```
src/
  main.c        CLI 入口，参数解析
  scanner.c     文件遍历，gitignore 支持
  counter.c     状态机行分类引擎
  language.c    语言定义（58+ 语言）
  output.c      输出格式化（表格/JSON/CSV/YAML）
  thread.c      线程池并行处理
  os_win32.c    Windows 平台适配
  os_unix.c     Linux/macOS 平台适配
  utils.c       工具函数
include/
  clocc.h       公共 API 头文件
tests/
  test_all.c    单元测试
  fixtures/     各语言测试文件
```

## 准确性

与 [tokei](https://github.com/XAMPPRocky/tokei) v12.1.2 对比：

- 总行数：**100% 匹配**
- Code + Mixed 行数：**与 tokei 的 code 行数一致**（tokei 没有"混合行"分类）
- 字符串感知：字符串字面量内的注释不会被误判

## 支持的语言

C、C++、C#、Java、JavaScript、TypeScript、JSX、TSX、Python、Ruby、Go、Rust、Swift、
Kotlin、Scala、Haskell、OCaml、F#、Erlang、Elixir、Clojure、R、MATLAB、Julia、Lua、
Perl、PHP、Shell/Bash、PowerShell、Dart、Groovy、Objective-C、D、Pascal、Ada、
Fortran、COBOL、Lisp、Scheme、Visual Basic、VB.NET、SQL、HTML、CSS、XML、YAML、
TOML、Markdown、Assembly、Makefile、CMake、Dockerfile、Terraform (HCL)、Vim Script、
Emacs Lisp、Zig、V (Vlang)、Nim、Crystal、Vue — 以及更多。

## 许可证

MIT

---

[English](README_en.md)
