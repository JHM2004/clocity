#include "clocc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Assert macros ─────────────────────────────────────────────────── */

static int g_fail_count = 0;

#define ASSERT_EQ(a, b, msg)                                        \
    do {                                                            \
        int _a = (a), _b = (b);                                     \
        if (_a != _b) {                                             \
            printf("  FAIL: %s (got %d, expected %d)\n",            \
                   msg, _a, _b);                                    \
            g_fail_count++;                                         \
        } else {                                                    \
            printf("  PASS: %s\n", msg);                            \
        }                                                           \
    } while (0)

#define ASSERT_STREQ(a, b, msg)                                     \
    do {                                                            \
        const char *_a = (a), *_b = (b);                            \
        if (_a == NULL && _b == NULL) {                             \
            printf("  PASS: %s\n", msg);                            \
        } else if (_a == NULL || _b == NULL) {                      \
            printf("  FAIL: %s (got %s, expected %s)\n",            \
                   msg,                                              \
                   _a ? _a : "NULL",                                 \
                   _b ? _b : "NULL");                                \
            g_fail_count++;                                         \
        } else if (strcmp(_a, _b) != 0) {                           \
            printf("  FAIL: %s (got \"%s\", expected \"%s\")\n",    \
                   msg, _a, _b);                                    \
            g_fail_count++;                                         \
        } else {                                                    \
            printf("  PASS: %s\n", msg);                            \
        }                                                           \
    } while (0)

#define ASSERT_NULL(p, msg)                                         \
    do {                                                            \
        if ((p) != NULL) {                                          \
            printf("  FAIL: %s (expected NULL)\n", msg);            \
            g_fail_count++;                                         \
        } else {                                                    \
            printf("  PASS: %s\n", msg);                            \
        }                                                           \
    } while (0)

#define ASSERT_NOT_NULL(p, msg)                                     \
    do {                                                            \
        if ((p) == NULL) {                                          \
            printf("  FAIL: %s (got NULL)\n", msg);                 \
            g_fail_count++;                                         \
        } else {                                                    \
            printf("  PASS: %s\n", msg);                            \
        }                                                           \
    } while (0)

#define ASSERT_TRUE(cond, msg)                                      \
    do {                                                            \
        if (!(cond)) {                                              \
            printf("  FAIL: %s\n", msg);                            \
            g_fail_count++;                                         \
        } else {                                                    \
            printf("  PASS: %s\n", msg);                            \
        }                                                           \
    } while (0)

/* ── Test: clocc_lang_by_extension ─────────────────────────────────── */

static void test_lang_by_extension(void)
{
    printf("\n[test_lang_by_extension]\n");

    ASSERT_EQ(clocc_lang_by_extension("c"), 0,
              "extension 'c' -> C (index 0)");
    ASSERT_EQ(clocc_lang_by_extension("h"), 0,
              "extension 'h' -> C (index 0)");
    ASSERT_EQ(clocc_lang_by_extension("py"), 6,
              "extension 'py' -> Python (index 6)");
    ASSERT_EQ(clocc_lang_by_extension("js"), 4,
              "extension 'js' -> JavaScript (index 4)");
    ASSERT_EQ(clocc_lang_by_extension("rs"), 9,
              "extension 'rs' -> Rust (index 9)");
    ASSERT_EQ(clocc_lang_by_extension("html"), 39,
              "extension 'html' -> HTML (index 39)");
    ASSERT_EQ(clocc_lang_by_extension("lua"), 22,
              "extension 'lua' -> Lua (index 22)");
    ASSERT_EQ(clocc_lang_by_extension("go"), 8,
              "extension 'go' -> Go (index 8)");
    ASSERT_EQ(clocc_lang_by_extension("sh"), 25,
              "extension 'sh' -> Shell (index 25)");
    ASSERT_EQ(clocc_lang_by_extension("xyz"), -1,
              "unknown extension returns -1");
    ASSERT_EQ(clocc_lang_by_extension(""), -1,
              "empty extension returns -1");
    ASSERT_EQ(clocc_lang_by_extension(NULL), -1,
              "NULL extension returns -1");
}

/* ── Test: clocc_lang_by_shebang ───────────────────────────────────── */

static void test_lang_by_shebang(void)
{
    FILE *fp;
    const char *tmp_path;
    int idx;

    printf("\n[test_lang_by_shebang]\n");

    /* Create a temp file with a bash shebang */
    tmp_path = "test_shebang_tmp.sh";
    fp = fopen(tmp_path, "w");
    if (fp == NULL) {
        printf("  FAIL: could not create temp shebang file\n");
        g_fail_count++;
        return;
    }
    fprintf(fp, "#!/bin/bash\necho hello\n");
    fclose(fp);

    idx = clocc_lang_by_shebang(tmp_path);
    ASSERT_EQ(idx, 25,
              "shebang '#!/bin/bash' -> Shell (index 25)");

    /* Create a temp file with a python shebang */
    tmp_path = "test_shebang_tmp.py";
    fp = fopen(tmp_path, "w");
    if (fp == NULL) {
        printf("  FAIL: could not create temp python shebang file\n");
        g_fail_count++;
        return;
    }
    fprintf(fp, "#!/usr/bin/env python\nprint('hi')\n");
    fclose(fp);

    idx = clocc_lang_by_shebang(tmp_path);
    ASSERT_EQ(idx, 6,
              "shebang '#!/usr/bin/env python' -> Python (index 6)");

    /* Test a file without shebang */
    tmp_path = "test_shebang_tmp_none.txt";
    fp = fopen(tmp_path, "w");
    if (fp == NULL) {
        printf("  FAIL: could not create temp no-shebang file\n");
        g_fail_count++;
        return;
    }
    fprintf(fp, "no shebang here\n");
    fclose(fp);

    idx = clocc_lang_by_shebang(tmp_path);
    ASSERT_EQ(idx, -1,
              "file without shebang returns -1");

    /* Test nonexistent file */
    idx = clocc_lang_by_shebang("nonexistent_file_xyz.txt");
    ASSERT_EQ(idx, -1,
              "nonexistent file returns -1");

    /* Clean up temp files */
    remove("test_shebang_tmp.sh");
    remove("test_shebang_tmp.py");
    remove("test_shebang_tmp_none.txt");
}

/* ── Test: clocc_count_buffer ──────────────────────────────────────── */

static void test_count_buffer(void)
{
    clocc_file_result_t result;
    int rc;

    printf("\n[test_count_buffer]\n");

    /* Known C code:
     *   #include <stdio.h>       <- code
     *   (blank)                  <- blank
     *   int main() {             <- code
     *       // comment           <- comment
     *       return 0;            <- code
     *   }                        <- code
     *
     * Expected: 1 blank, 1 comment, 4 code, 0 mixed
     */
    const char *c_code =
        "#include <stdio.h>\n"
        "\n"
        "int main() {\n"
        "    // comment\n"
        "    return 0;\n"
        "}\n";
    size_t c_len = strlen(c_code);

    rc = clocc_count_buffer(c_code, c_len, 0, &result);
    ASSERT_EQ(rc, 0, "count_buffer returns 0 on valid C code");
    ASSERT_EQ(result.blank_lines, 1,
              "C code: 1 blank line");
    ASSERT_EQ(result.comment_lines, 1,
              "C code: 1 comment line");
    ASSERT_EQ(result.code_lines, 4,
              "C code: 4 code lines");
    ASSERT_EQ(result.mixed_lines, 0,
              "C code: 0 mixed lines");

    /* Test with mixed line:
     *   int x = 1; // comment    <- mixed
     */
    const char *c_mixed =
        "int x = 1; // comment\n";
    size_t c_mixed_len = strlen(c_mixed);

    rc = clocc_count_buffer(c_mixed, c_mixed_len, 0, &result);
    ASSERT_EQ(rc, 0, "count_buffer returns 0 on mixed line");
    ASSERT_EQ(result.mixed_lines, 1,
              "C mixed: 1 mixed line");
    ASSERT_EQ(result.code_lines, 0,
              "C mixed: 0 pure code lines");
    ASSERT_EQ(result.comment_lines, 0,
              "C mixed: 0 pure comment lines");

    /* Test with block comment spanning lines:
     *   int a = 1;               <- code
     *   /* block                 <- comment (starts block)
     *    * continued             <- comment (inside block)
     *    * end  * /              <- comment (ends block)
     *   int b = 2;               <- code
     */
    const char *c_block =
        "int a = 1;\n"
        "/* block\n"
        " * continued\n"
        " * end */\n"
        "int b = 2;\n";
    size_t c_block_len = strlen(c_block);

    rc = clocc_count_buffer(c_block, c_block_len, 0, &result);
    ASSERT_EQ(rc, 0, "count_buffer returns 0 on block comment");
    ASSERT_EQ(result.code_lines, 2,
              "C block: 2 code lines");
    ASSERT_EQ(result.comment_lines, 3,
              "C block: 3 comment lines");

    /* Test with invalid lang index */
    rc = clocc_count_buffer("x\n", 2, -1, &result);
    ASSERT_EQ(rc, -1,
              "count_buffer returns -1 on invalid lang_index");

    /* Test string with comment-like content:
     *   char *s = "http://example.com";  <- code (not comment)
     */
    const char *c_str_comment =
        "char *s = \"http://example.com\";\n";
    size_t c_str_len = strlen(c_str_comment);

    rc = clocc_count_buffer(c_str_comment, c_str_len, 0, &result);
    ASSERT_EQ(rc, 0, "count_buffer handles string with //");
    ASSERT_EQ(result.code_lines, 1,
              "string with // is code, not comment");
    ASSERT_EQ(result.comment_lines, 0,
              "string with // has 0 comment lines");
}

/* ── Test: clocc_str_icmp ──────────────────────────────────────────── */

static void test_str_icmp(void)
{
    printf("\n[test_str_icmp]\n");

    ASSERT_EQ(clocc_str_icmp("hello", "hello"), 0,
              "identical strings compare equal");
    ASSERT_EQ(clocc_str_icmp("Hello", "hello"), 0,
              "case-insensitive equal strings");
    ASSERT_EQ(clocc_str_icmp("HELLO", "hello"), 0,
              "all-upper vs all-lower");
    ASSERT_TRUE(clocc_str_icmp("abc", "abd") != 0,
                "different strings not equal");
    ASSERT_TRUE(clocc_str_icmp("ab", "abc") != 0,
                "prefix vs longer string not equal");
    ASSERT_EQ(clocc_str_icmp("", ""), 0,
              "empty strings equal");
    ASSERT_TRUE(clocc_str_icmp(NULL, "hello") != 0,
                "NULL vs non-NULL not equal");
    ASSERT_TRUE(clocc_str_icmp("hello", NULL) != 0,
                "non-NULL vs NULL not equal");
    ASSERT_EQ(clocc_str_icmp(NULL, NULL), 0,
              "both NULL equal");
}

/* ── Test: clocc_get_extension ─────────────────────────────────────── */

static void test_get_extension(void)
{
    printf("\n[test_get_extension]\n");

    ASSERT_STREQ(clocc_get_extension("foo.c"), "c",
                 "foo.c -> 'c'");
    ASSERT_STREQ(clocc_get_extension("path/to/file.py"), "py",
                 "path/to/file.py -> 'py'");
    ASSERT_STREQ(clocc_get_extension("test.js"), "js",
                 "test.js -> 'js'");
    ASSERT_STREQ(clocc_get_extension("archive.tar.gz"), "gz",
                 "archive.tar.gz -> 'gz'");
    ASSERT_NULL(clocc_get_extension("noext"),
                "noext -> NULL");
    ASSERT_NULL(clocc_get_extension(".hidden"),
                ".hidden -> NULL (dot at segment start)");
    ASSERT_STREQ(clocc_get_extension("path/file.test.js"), "js",
                 "path/file.test.js -> 'js'");
    ASSERT_NULL(clocc_get_extension(NULL),
                "NULL path -> NULL");
    ASSERT_STREQ(clocc_get_extension("dir\\file.rs"), "rs",
                 "Windows path dir\\file.rs -> 'rs'");
}

/* ── Test: clocc_hash_string ───────────────────────────────────────── */

static void test_hash_string(void)
{
    size_t h1, h2;

    printf("\n[test_hash_string]\n");

    h1 = clocc_hash_string("hello");
    ASSERT_TRUE(h1 != 0,
                "hash of 'hello' is non-zero");

    h2 = clocc_hash_string("hello");
    ASSERT_EQ((int)(h1 == h2), 1,
              "same string produces same hash");

    h2 = clocc_hash_string("world");
    ASSERT_TRUE(h1 != h2,
                "different strings produce different hashes");

    h1 = clocc_hash_string("");
    /* Empty string hash: 5381 (djb2 initial value), which is non-zero */
    ASSERT_TRUE(h1 != 0,
                "hash of empty string is non-zero");
}

/* ── Main ──────────────────────────────────────────────────────────── */

int main(void)
{
    printf("=== clocc unit tests ===\n");

    /* Initialize language table (required before lang lookups) */
    clocc_lang_init();

    test_lang_by_extension();
    test_lang_by_shebang();
    test_count_buffer();
    test_str_icmp();
    test_get_extension();
    test_hash_string();

    printf("\n=== results: %d failure(s) ===\n", g_fail_count);

    return g_fail_count > 0 ? 1 : 0;
}
