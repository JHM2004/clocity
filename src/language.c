#include "clocc.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ── Language definitions ──────────────────────────────────────────── */

static clocc_lang_t languages[] = {
    /* 0: C */
    {
        .name = "C",
        .extensions = (const char *[]){ "c", "h" },
        .ext_count = 2,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "//",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 1: C++ */
    {
        .name = "C++",
        .extensions = (const char *[]){ "cpp", "cxx", "cc", "hpp", "hxx", "hh" },
        .ext_count = 6,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "//",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 2: C# */
    {
        .name = "C#",
        .extensions = (const char *[]){ "cs" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "//",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 3: Java */
    {
        .name = "Java",
        .extensions = (const char *[]){ "java" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "//",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 4: JavaScript */
    {
        .name = "JavaScript",
        .extensions = (const char *[]){ "js", "mjs", "cjs" },
        .ext_count = 3,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "//",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 1,
        .shebang = "node"
    },
    /* 5: TypeScript */
    {
        .name = "TypeScript",
        .extensions = (const char *[]){ "ts", "tsx" },
        .ext_count = 2,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "//",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 1,
        .shebang = ""
    },
    /* 6: Python */
    {
        .name = "Python",
        .extensions = (const char *[]){ "py", "pyw", "pyi" },
        .ext_count = 3,
        .comment_style = CLOCC_COMMENT_HASH,
        .line_comment = "#",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 1,
        .shebang = "python"
    },
    /* 7: Ruby */
    {
        .name = "Ruby",
        .extensions = (const char *[]){ "rb", "rake" },
        .ext_count = 2,
        .comment_style = CLOCC_COMMENT_HASH,
        .line_comment = "#",
        .block_comment_start = "=begin",
        .block_comment_end = "=end",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 1,
        .shebang = "ruby"
    },
    /* 8: Go */
    {
        .name = "Go",
        .extensions = (const char *[]){ "go" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "//",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 1,
        .shebang = ""
    },
    /* 9: Rust */
    {
        .name = "Rust",
        .extensions = (const char *[]){ "rs" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "//",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 1,
        .string_literals = 1,
        .multiline_strings = 1,
        .shebang = ""
    },
    /* 10: Swift */
    {
        .name = "Swift",
        .extensions = (const char *[]){ "swift" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "//",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 1,
        .string_literals = 1,
        .multiline_strings = 1,
        .shebang = "swift"
    },
    /* 11: Kotlin */
    {
        .name = "Kotlin",
        .extensions = (const char *[]){ "kt", "kts" },
        .ext_count = 2,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "//",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 12: Scala */
    {
        .name = "Scala",
        .extensions = (const char *[]){ "scala", "sc" },
        .ext_count = 2,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "//",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 1,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = "scala"
    },
    /* 13: Haskell */
    {
        .name = "Haskell",
        .extensions = (const char *[]){ "hs", "lhs" },
        .ext_count = 2,
        .comment_style = CLOCC_COMMENT_DASH,
        .line_comment = "--",
        .block_comment_start = "{-",
        .block_comment_end = "-}",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 1,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 14: OCaml */
    {
        .name = "OCaml",
        .extensions = (const char *[]){ "ml", "mli" },
        .ext_count = 2,
        .comment_style = CLOCC_COMMENT_DASH,
        .line_comment = "--",
        .block_comment_start = "(*",
        .block_comment_end = "*)",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 1,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 15: F# */
    {
        .name = "F#",
        .extensions = (const char *[]){ "fs", "fsi", "fsx" },
        .ext_count = 3,
        .comment_style = CLOCC_COMMENT_DASH,
        .line_comment = "//",
        .block_comment_start = "(*",
        .block_comment_end = "*)",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 1,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 16: Erlang */
    {
        .name = "Erlang",
        .extensions = (const char *[]){ "erl", "hrl" },
        .ext_count = 2,
        .comment_style = CLOCC_COMMENT_PERCENT,
        .line_comment = "%",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = "escript"
    },
    /* 17: Elixir */
    {
        .name = "Elixir",
        .extensions = (const char *[]){ "ex", "exs" },
        .ext_count = 2,
        .comment_style = CLOCC_COMMENT_HASH,
        .line_comment = "#",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 1,
        .shebang = "elixir"
    },
    /* 18: Clojure */
    {
        .name = "Clojure",
        .extensions = (const char *[]){ "clj", "cljs", "cljc" },
        .ext_count = 3,
        .comment_style = CLOCC_COMMENT_SEMICOLON,
        .line_comment = ";",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 19: R */
    {
        .name = "R",
        .extensions = (const char *[]){ "r", "R" },
        .ext_count = 2,
        .comment_style = CLOCC_COMMENT_HASH,
        .line_comment = "#",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = "Rscript"
    },
    /* 20: MATLAB */
    {
        .name = "MATLAB",
        .extensions = (const char *[]){ "mat" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_PERCENT,
        .line_comment = "%",
        .block_comment_start = "%{",
        .block_comment_end = "}%",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 21: Julia */
    {
        .name = "Julia",
        .extensions = (const char *[]){ "jl" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_HASH,
        .line_comment = "#",
        .block_comment_start = "#=",
        .block_comment_end = "=#",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 1,
        .string_literals = 1,
        .multiline_strings = 1,
        .shebang = "julia"
    },
    /* 22: Lua */
    {
        .name = "Lua",
        .extensions = (const char *[]){ "lua" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_LUA,
        .line_comment = "--",
        .block_comment_start = "--[[",
        .block_comment_end = "]]",
        .block_comment_start_alt = "--[=[",
        .block_comment_end_alt = "]=]",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 1,
        .shebang = "lua"
    },
    /* 23: Perl */
    {
        .name = "Perl",
        .extensions = (const char *[]){ "pl", "pm", "t" },
        .ext_count = 3,
        .comment_style = CLOCC_COMMENT_HASH,
        .line_comment = "#",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 1,
        .shebang = "perl"
    },
    /* 24: PHP */
    {
        .name = "PHP",
        .extensions = (const char *[]){ "php", "phtml" },
        .ext_count = 2,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "//",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = "php"
    },
    /* 25: Shell/Bash */
    {
        .name = "Shell",
        .extensions = (const char *[]){ "sh", "bash", "zsh", "ksh" },
        .ext_count = 4,
        .comment_style = CLOCC_COMMENT_HASH,
        .line_comment = "#",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 1,
        .shebang = "bash"
    },
    /* 26: PowerShell */
    {
        .name = "PowerShell",
        .extensions = (const char *[]){ "ps1", "psm1", "psd1" },
        .ext_count = 3,
        .comment_style = CLOCC_COMMENT_HASH,
        .line_comment = "#",
        .block_comment_start = "<#",
        .block_comment_end = "#>",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 27: Dart */
    {
        .name = "Dart",
        .extensions = (const char *[]){ "dart" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "//",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 1,
        .shebang = ""
    },
    /* 28: Groovy */
    {
        .name = "Groovy",
        .extensions = (const char *[]){ "groovy", "gradle" },
        .ext_count = 2,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "//",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 1,
        .shebang = "groovy"
    },
    /* 29: Objective-C */
    {
        .name = "Objective-C",
        .extensions = (const char *[]){ "m", "mm" },
        .ext_count = 2,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "//",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 30: D */
    {
        .name = "D",
        .extensions = (const char *[]){ "d", "di" },
        .ext_count = 2,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "//",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "/+",
        .block_comment_end_alt = "+/",
        .nested_comments = 1,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 31: Pascal */
    {
        .name = "Pascal",
        .extensions = (const char *[]){ "pas", "pp", "dpr" },
        .ext_count = 3,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "//",
        .block_comment_start = "{",
        .block_comment_end = "}",
        .block_comment_start_alt = "(*",
        .block_comment_end_alt = "*)",
        .nested_comments = 1,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 32: Ada */
    {
        .name = "Ada",
        .extensions = (const char *[]){ "adb", "ads" },
        .ext_count = 2,
        .comment_style = CLOCC_COMMENT_DASH,
        .line_comment = "--",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 33: Fortran */
    {
        .name = "Fortran",
        .extensions = (const char *[]){ "f", "f90", "f95", "f03", "f08", "for" },
        .ext_count = 6,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "!",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 34: COBOL */
    {
        .name = "COBOL",
        .extensions = (const char *[]){ "cob", "cbl" },
        .ext_count = 2,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "*",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 35: Lisp */
    {
        .name = "Lisp",
        .extensions = (const char *[]){ "lisp", "lsp", "cl" },
        .ext_count = 3,
        .comment_style = CLOCC_COMMENT_SEMICOLON,
        .line_comment = ";",
        .block_comment_start = "#|",
        .block_comment_end = "|#",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 1,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 36: Visual Basic */
    {
        .name = "Visual Basic",
        .extensions = (const char *[]){ "bas", "frm", "cls" },
        .ext_count = 3,
        .comment_style = CLOCC_COMMENT_REM,
        .line_comment = "'",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 37: VB.NET */
    {
        .name = "VB.NET",
        .extensions = (const char *[]){ "vb" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_REM,
        .line_comment = "'",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 38: SQL */
    {
        .name = "SQL",
        .extensions = (const char *[]){ "sql" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_DASH,
        .line_comment = "--",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 39: HTML */
    {
        .name = "HTML",
        .extensions = (const char *[]){ "html", "htm" },
        .ext_count = 2,
        .comment_style = CLOCC_COMMENT_HTML,
        .line_comment = "",
        .block_comment_start = "<!--",
        .block_comment_end = "-->",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 40: CSS */
    {
        .name = "CSS",
        .extensions = (const char *[]){ "css" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 41: XML */
    {
        .name = "XML",
        .extensions = (const char *[]){ "xml", "xsl", "xsd", "svg", "xhtml" },
        .ext_count = 5,
        .comment_style = CLOCC_COMMENT_HTML,
        .line_comment = "",
        .block_comment_start = "<!--",
        .block_comment_end = "-->",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 42: YAML */
    {
        .name = "YAML",
        .extensions = (const char *[]){ "yaml", "yml" },
        .ext_count = 2,
        .comment_style = CLOCC_COMMENT_HASH,
        .line_comment = "#",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 1,
        .shebang = ""
    },
    /* 43: TOML */
    {
        .name = "TOML",
        .extensions = (const char *[]){ "toml" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_HASH,
        .line_comment = "#",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 1,
        .shebang = ""
    },
    /* 44: JSON */
    {
        .name = "JSON",
        .extensions = (const char *[]){ "json" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_HASH,
        .line_comment = "",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 45: Markdown */
    {
        .name = "Markdown",
        .extensions = (const char *[]){ "md", "markdown" },
        .ext_count = 2,
        .comment_style = CLOCC_COMMENT_HTML,
        .line_comment = "",
        .block_comment_start = "<!--",
        .block_comment_end = "-->",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 46: Assembly */
    {
        .name = "Assembly",
        .extensions = (const char *[]){ "asm", "s", "S" },
        .ext_count = 3,
        .comment_style = CLOCC_COMMENT_HASH,
        .line_comment = ";",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 0,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 47: Makefile */
    {
        .name = "Makefile",
        .extensions = (const char *[]){ "mk" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_HASH,
        .line_comment = "#",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 48: CMake */
    {
        .name = "CMake",
        .extensions = (const char *[]){ "cmake" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_HASH,
        .line_comment = "#",
        .block_comment_start = "#[[",
        .block_comment_end = "]]",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 49: Dockerfile */
    {
        .name = "Dockerfile",
        .extensions = (const char *[]){ "dockerfile" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_HASH,
        .line_comment = "#",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 50: Terraform (HCL) */
    {
        .name = "Terraform",
        .extensions = (const char *[]){ "tf", "tfvars" },
        .ext_count = 2,
        .comment_style = CLOCC_COMMENT_HASH,
        .line_comment = "#",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 51: Vim Script */
    {
        .name = "Vim Script",
        .extensions = (const char *[]){ "vim" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_HASH,
        .line_comment = "\"",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 52: Emacs Lisp */
    {
        .name = "Emacs Lisp",
        .extensions = (const char *[]){ "el" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_SEMICOLON,
        .line_comment = ";",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 53: Zig */
    {
        .name = "Zig",
        .extensions = (const char *[]){ "zig" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "//",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 1,
        .shebang = ""
    },
    /* 54: V (Vlang) */
    {
        .name = "V",
        .extensions = (const char *[]){ "v" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_C_STYLE,
        .line_comment = "//",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 0,
        .shebang = ""
    },
    /* 55: Nim */
    {
        .name = "Nim",
        .extensions = (const char *[]){ "nim" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_HASH,
        .line_comment = "#",
        .block_comment_start = "#[",
        .block_comment_end = "]#",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 1,
        .string_literals = 1,
        .multiline_strings = 1,
        .shebang = ""
    },
    /* 56: Crystal */
    {
        .name = "Crystal",
        .extensions = (const char *[]){ "cr" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_HASH,
        .line_comment = "#",
        .block_comment_start = "",
        .block_comment_end = "",
        .block_comment_start_alt = "",
        .block_comment_end_alt = "",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 1,
        .shebang = "crystal"
    },
    /* 57: Vue */
    {
        .name = "Vue",
        .extensions = (const char *[]){ "vue" },
        .ext_count = 1,
        .comment_style = CLOCC_COMMENT_HTML,
        .line_comment = "//",
        .block_comment_start = "/*",
        .block_comment_end = "*/",
        .block_comment_start_alt = "<!--",
        .block_comment_end_alt = "-->",
        .nested_comments = 0,
        .string_literals = 1,
        .multiline_strings = 1,
        .shebang = ""
    },
};

/* Total number of languages defined */
static const int lang_count = sizeof(languages) / sizeof(languages[0]);

/* ── Extension hash map ────────────────────────────────────────────── */

static int ext_hash_map[CLOCC_HASH_SIZE];

/* Sentinel: -1 means empty slot */
#define HASH_EMPTY (-1)

/* djb2 hash function */
static unsigned int ext_hash(const char *str)
{
    unsigned int hash = 5381;
    int c;
    while ((c = (unsigned char)*str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

/* ── Public functions ──────────────────────────────────────────────── */

void clocc_lang_init(void)
{
    int i, j;
    unsigned int h, idx;

    /* Initialize all slots to empty */
    for (int k = 0; k < CLOCC_HASH_SIZE; k++)
        ext_hash_map[k] = HASH_EMPTY;

    for (i = 0; i < lang_count; i++) {
        for (j = 0; j < languages[i].ext_count; j++) {
            const char *ext = languages[i].extensions[j];
            h = ext_hash(ext);
            idx = h % CLOCC_HASH_SIZE;

            /* Linear probing for collision resolution */
            while (ext_hash_map[idx] != HASH_EMPTY) {
                idx = (idx + 1) % CLOCC_HASH_SIZE;
            }
            ext_hash_map[idx] = i;
        }
    }
}

int clocc_lang_by_extension(const char *ext)
{
    unsigned int h, idx, start;

    if (!ext || !*ext)
        return -1;

    h = ext_hash(ext);
    idx = h % CLOCC_HASH_SIZE;
    start = idx;

    for (;;) {
        int lang_idx = ext_hash_map[idx];
        if (lang_idx == HASH_EMPTY)
            return -1;

        /* Check all extensions of this language for a match */
        int j;
        for (j = 0; j < languages[lang_idx].ext_count; j++) {
            if (strcasecmp(languages[lang_idx].extensions[j], ext) == 0)
                return lang_idx;
        }

        idx = (idx + 1) % CLOCC_HASH_SIZE;
        if (idx == start)
            return -1;
    }
}

int clocc_lang_by_shebang(const char *path)
{
    FILE *fp;
    char line[512];
    char interp[CLOCC_MAX_SHEBANG_LEN];
    const char *p;
    int i, len;

    fp = fopen(path, "r");
    if (!fp)
        return -1;

    if (!fgets(line, (int)sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    /* Strip trailing newline */
    len = (int)strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        line[--len] = '\0';

    /* Must start with #! */
    if (len < 3 || line[0] != '#' || line[1] != '!')
        return -1;

    p = line + 2;

    /* Skip whitespace after #! */
    while (*p && (*p == ' ' || *p == '\t'))
        p++;

    /* Handle "/usr/bin/env interpreter" pattern */
    if (strstr(p, "/usr/bin/env") == p ||
        strstr(p, "/bin/env") == p ||
        strstr(p, "/usr/local/bin/env") == p) {
        /* Skip to the env binary, then skip whitespace */
        p = strchr(p, ' ');
        if (!p)
            return -1;
        while (*p && (*p == ' ' || *p == '\t'))
            p++;
    } else {
        /* Skip path prefix: extract last component after '/' */
        const char *slash = strrchr(p, '/');
        if (slash)
            p = slash + 1;
    }

    if (!*p)
        return -1;

    /* Copy interpreter name, stripping version suffix and args */
    len = 0;
    while (*p && *p != ' ' && *p != '\t' && len < CLOCC_MAX_SHEBANG_LEN - 1) {
        /* Stop at version suffix like python3.9 -> python */
        if (isdigit((unsigned char)*p) && len > 0)
            break;
        interp[len++] = *p++;
    }
    interp[len] = '\0';

    if (len == 0)
        return -1;

    /* Match against language shebang fields */
    for (i = 0; i < lang_count; i++) {
        if (languages[i].shebang[0] == '\0')
            continue;
        if (strstr(interp, languages[i].shebang) == interp)
            return i;
    }

    return -1;
}

clocc_lang_t *clocc_lang_get(int index)
{
    if (index < 0 || index >= lang_count)
        return NULL;
    return &languages[index];
}

int clocc_lang_count(void)
{
    return lang_count;
}
