#include "clocc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

/* ANSI color codes */
#define ANSI_BOLD   "\033[1m"
#define ANSI_GREEN  "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_BLUE   "\033[34m"
#define ANSI_RESET  "\033[0m"

/* Column indices for width tracking */
enum {
    COL_LANG,
    COL_FILES,
    COL_BLANK,
    COL_COMMENT,
    COL_CODE,
    COL_MIXED,
    COL_LINES,
    COL_COUNT
};

/* ---- Helper functions ---- */

static int count_digits(int n)
{
    if (n < 0) n = -n;
    if (n == 0) return 1;
    int count = 0;
    while (n > 0) {
        count++;
        n /= 10;
    }
    return count;
}

static void print_padded(const char *str, int width, int align_right)
{
    int slen = (int)strlen(str);
    if (align_right) {
        for (int i = slen; i < width; i++)
            putchar(' ');
        fputs(str, stdout);
    } else {
        fputs(str, stdout);
        for (int i = slen; i < width; i++)
            putchar(' ');
    }
}

static int is_terminal(void)
{
#ifdef _WIN32
    return _isatty(_fileno(stdout));
#else
    return isatty(fileno(stdout));
#endif
}

/* ---- Filtering ---- */

static int filter_results(const clocc_lang_result_t *lang,
                          const clocc_config_t *config)
{
    if (config->exclude_empty && lang->file_count == 0)
        return 0;

    if (config->filter_lang_count > 0) {
        for (int i = 0; i < config->filter_lang_count; i++) {
            if (strcasecmp(lang->name, config->filter_langs[i]) == 0)
                return 1;
        }
        return 0;
    }

    return 1;
}

/* ---- Sorting ---- */

static int get_sort_value(const clocc_lang_result_t *lang,
                          clocc_sort_field_t field)
{
    switch (field) {
    case CLOCC_SORT_FILES:   return lang->file_count;
    case CLOCC_SORT_LINES:   return lang->total_lines;
    case CLOCC_SORT_CODE:    return lang->code_lines;
    case CLOCC_SORT_COMMENT: return lang->comment_lines;
    case CLOCC_SORT_BLANK:   return lang->blank_lines;
    case CLOCC_SORT_MIXED:   return lang->mixed_lines;
    }
    return 0;
}

/* We need a way to pass config into the comparator. Since C's qsort doesn't
   support a context parameter in C11, we use file-scope globals that are
   set right before qsort is called. This is safe in single-threaded use. */
static clocc_sort_field_t g_sort_field;
static int g_sort_desc;

static int compare_lang_wrapper(const void *a, const void *b)
{
    const clocc_lang_result_t *la = (const clocc_lang_result_t *)a;
    const clocc_lang_result_t *lb = (const clocc_lang_result_t *)b;

    int va = get_sort_value(la, g_sort_field);
    int vb = get_sort_value(lb, g_sort_field);

    if (va != vb) {
        return g_sort_desc ? (vb - va) : (va - vb);
    }
    return strcasecmp(la->name, lb->name);
}

static clocc_lang_result_t *sort_results(const clocc_result_t *result,
                                         const clocc_config_t *config,
                                         int *out_count)
{
    /* First pass: count filtered entries */
    int count = 0;
    for (int i = 0; i < result->lang_count; i++) {
        if (filter_results(&result->languages[i], config))
            count++;
    }

    if (count == 0) {
        *out_count = 0;
        return NULL;
    }

    clocc_lang_result_t *sorted = malloc(
        (size_t)count * sizeof(clocc_lang_result_t));
    if (!sorted) {
        *out_count = 0;
        return NULL;
    }

    /* Second pass: copy filtered entries */
    int idx = 0;
    for (int i = 0; i < result->lang_count; i++) {
        if (filter_results(&result->languages[i], config))
            sorted[idx++] = result->languages[i];
    }

    /* Sort */
    g_sort_field = config->sort_by;
    g_sort_desc = config->sort_descending;
    qsort(sorted, (size_t)count, sizeof(clocc_lang_result_t),
          compare_lang_wrapper);

    *out_count = count;
    return sorted;
}

/* ---- Table output ---- */

void clocc_output_table(const clocc_result_t *result,
                        const clocc_config_t *config)
{
    int count = 0;
    clocc_lang_result_t *sorted = sort_results(result, config, &count);
    if (!sorted && count == 0) {
        /* No results to display */
        printf("No languages found.\n");
        return;
    }

    int use_color = config->show_colors && is_terminal();

    /* Compute column widths */
    int widths[COL_COUNT];
    widths[COL_LANG]    = 8;  /* "Language" */
    widths[COL_FILES]   = 5;  /* "Files" */
    widths[COL_BLANK]   = 5;  /* "Blank" */
    widths[COL_COMMENT] = 7;  /* "Comment" */
    widths[COL_CODE]    = 4;  /* "Code" */
    widths[COL_MIXED]   = 5;  /* "Mixed" */
    widths[COL_LINES]   = 5;  /* "Lines" */

    for (int i = 0; i < count; i++) {
        int nl = (int)strlen(sorted[i].name);
        if (nl > widths[COL_LANG]) widths[COL_LANG] = nl;
        if (count_digits(sorted[i].file_count) > widths[COL_FILES])
            widths[COL_FILES] = count_digits(sorted[i].file_count);
        if (count_digits(sorted[i].blank_lines) > widths[COL_BLANK])
            widths[COL_BLANK] = count_digits(sorted[i].blank_lines);
        if (count_digits(sorted[i].comment_lines) > widths[COL_COMMENT])
            widths[COL_COMMENT] = count_digits(sorted[i].comment_lines);
        if (count_digits(sorted[i].code_lines) > widths[COL_CODE])
            widths[COL_CODE] = count_digits(sorted[i].code_lines);
        if (count_digits(sorted[i].mixed_lines) > widths[COL_MIXED])
            widths[COL_MIXED] = count_digits(sorted[i].mixed_lines);
        if (count_digits(sorted[i].total_lines) > widths[COL_LINES])
            widths[COL_LINES] = count_digits(sorted[i].total_lines);
    }

    /* Also account for totals */
    if (count_digits(result->total_files) > widths[COL_FILES])
        widths[COL_FILES] = count_digits(result->total_files);
    if (count_digits(result->total_blank) > widths[COL_BLANK])
        widths[COL_BLANK] = count_digits(result->total_blank);
    if (count_digits(result->total_comment) > widths[COL_COMMENT])
        widths[COL_COMMENT] = count_digits(result->total_comment);
    if (count_digits(result->total_code) > widths[COL_CODE])
        widths[COL_CODE] = count_digits(result->total_code);
    if (count_digits(result->total_mixed) > widths[COL_MIXED])
        widths[COL_MIXED] = count_digits(result->total_mixed);
    if (count_digits(result->total_lines) > widths[COL_LINES])
        widths[COL_LINES] = count_digits(result->total_lines);

    /* Print header */
    if (use_color) printf(ANSI_BOLD);
    print_padded("Language", widths[COL_LANG], 0);
    printf(" | ");
    print_padded("Files", widths[COL_FILES], 1);
    printf(" | ");
    print_padded("Blank", widths[COL_BLANK], 1);
    printf(" | ");
    print_padded("Comment", widths[COL_COMMENT], 1);
    printf(" | ");
    print_padded("Code", widths[COL_CODE], 1);
    printf(" | ");
    print_padded("Mixed", widths[COL_MIXED], 1);
    printf(" | ");
    print_padded("Lines", widths[COL_LINES], 1);
    if (use_color) printf(ANSI_RESET);
    printf("\n");

    /* Print separator */
    for (int i = 0; i < widths[COL_LANG]; i++) putchar('-');
    printf("-+-");
    for (int i = 0; i < widths[COL_FILES]; i++) putchar('-');
    printf("-+-");
    for (int i = 0; i < widths[COL_BLANK]; i++) putchar('-');
    printf("-+-");
    for (int i = 0; i < widths[COL_COMMENT]; i++) putchar('-');
    printf("-+-");
    for (int i = 0; i < widths[COL_CODE]; i++) putchar('-');
    printf("-+-");
    for (int i = 0; i < widths[COL_MIXED]; i++) putchar('-');
    printf("-+-");
    for (int i = 0; i < widths[COL_LINES]; i++) putchar('-');
    printf("\n");

    /* Print data rows */
    char buf[32];
    for (int i = 0; i < count; i++) {
        print_padded(sorted[i].name, widths[COL_LANG], 0);
        printf(" | ");

        sprintf(buf, "%d", sorted[i].file_count);
        print_padded(buf, widths[COL_FILES], 1);
        printf(" | ");

        if (use_color) printf(ANSI_BLUE);
        sprintf(buf, "%d", sorted[i].blank_lines);
        print_padded(buf, widths[COL_BLANK], 1);
        if (use_color) printf(ANSI_RESET);
        printf(" | ");

        if (use_color) printf(ANSI_YELLOW);
        sprintf(buf, "%d", sorted[i].comment_lines);
        print_padded(buf, widths[COL_COMMENT], 1);
        if (use_color) printf(ANSI_RESET);
        printf(" | ");

        if (use_color) printf(ANSI_GREEN);
        sprintf(buf, "%d", sorted[i].code_lines);
        print_padded(buf, widths[COL_CODE], 1);
        if (use_color) printf(ANSI_RESET);
        printf(" | ");

        sprintf(buf, "%d", sorted[i].mixed_lines);
        print_padded(buf, widths[COL_MIXED], 1);
        printf(" | ");

        sprintf(buf, "%d", sorted[i].total_lines);
        print_padded(buf, widths[COL_LINES], 1);
        printf("\n");
    }

    /* Print separator */
    for (int i = 0; i < widths[COL_LANG]; i++) putchar('-');
    printf("-+-");
    for (int i = 0; i < widths[COL_FILES]; i++) putchar('-');
    printf("-+-");
    for (int i = 0; i < widths[COL_BLANK]; i++) putchar('-');
    printf("-+-");
    for (int i = 0; i < widths[COL_COMMENT]; i++) putchar('-');
    printf("-+-");
    for (int i = 0; i < widths[COL_CODE]; i++) putchar('-');
    printf("-+-");
    for (int i = 0; i < widths[COL_MIXED]; i++) putchar('-');
    printf("-+-");
    for (int i = 0; i < widths[COL_LINES]; i++) putchar('-');
    printf("\n");

    /* Print summary row */
    if (use_color) printf(ANSI_BOLD);
    print_padded("SUM", widths[COL_LANG], 0);
    printf(" | ");

    sprintf(buf, "%d", result->total_files);
    print_padded(buf, widths[COL_FILES], 1);
    printf(" | ");

    if (use_color) printf(ANSI_BLUE);
    sprintf(buf, "%d", result->total_blank);
    print_padded(buf, widths[COL_BLANK], 1);
    if (use_color) printf(ANSI_RESET);
    if (use_color) printf(ANSI_BOLD);
    printf(" | ");

    if (use_color) printf(ANSI_YELLOW);
    sprintf(buf, "%d", result->total_comment);
    print_padded(buf, widths[COL_COMMENT], 1);
    if (use_color) printf(ANSI_RESET);
    if (use_color) printf(ANSI_BOLD);
    printf(" | ");

    if (use_color) printf(ANSI_GREEN);
    sprintf(buf, "%d", result->total_code);
    print_padded(buf, widths[COL_CODE], 1);
    if (use_color) printf(ANSI_RESET);
    if (use_color) printf(ANSI_BOLD);
    printf(" | ");

    sprintf(buf, "%d", result->total_mixed);
    print_padded(buf, widths[COL_MIXED], 1);
    printf(" | ");

    sprintf(buf, "%d", result->total_lines);
    print_padded(buf, widths[COL_LINES], 1);
    if (use_color) printf(ANSI_RESET);
    printf("\n");

    free(sorted);
}

/* ---- JSON output ---- */

void clocc_output_json(const clocc_result_t *result,
                        const clocc_config_t *config)
{
    int count = 0;
    clocc_lang_result_t *sorted = sort_results(result, config, &count);

    /* Get current timestamp */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", tm_info);

    printf("{\n");
    printf("  \"cloc-c\": {\n");
    printf("    \"version\": \"1.0.0\",\n");
    printf("    \"timestamp\": \"%s\"\n", timestamp);
    printf("  },\n");
    printf("  \"languages\": [\n");

    for (int i = 0; i < count; i++) {
        printf("    {\n");
        printf("      \"name\": \"%s\",\n", sorted[i].name);
        printf("      \"files\": %d,\n", sorted[i].file_count);
        printf("      \"code\": %d,\n", sorted[i].code_lines);
        printf("      \"comment\": %d,\n", sorted[i].comment_lines);
        printf("      \"blank\": %d,\n", sorted[i].blank_lines);
        printf("      \"mixed\": %d,\n", sorted[i].mixed_lines);
        printf("      \"lines\": %d\n", sorted[i].total_lines);
        if (i < count - 1)
            printf("    },\n");
        else
            printf("    }\n");
    }

    printf("  ],\n");
    printf("  \"total\": {\n");
    printf("    \"files\": %d,\n", result->total_files);
    printf("    \"code\": %d,\n", result->total_code);
    printf("    \"comment\": %d,\n", result->total_comment);
    printf("    \"blank\": %d,\n", result->total_blank);
    printf("    \"mixed\": %d,\n", result->total_mixed);
    printf("    \"lines\": %d\n", result->total_lines);
    printf("  }\n");
    printf("}\n");

    free(sorted);
}

/* ---- CSV output ---- */

void clocc_output_csv(const clocc_result_t *result,
                       const clocc_config_t *config)
{
    int count = 0;
    clocc_lang_result_t *sorted = sort_results(result, config, &count);

    printf("Language,Files,Blank,Comment,Code,Mixed,Lines\n");

    for (int i = 0; i < count; i++) {
        printf("%s,%d,%d,%d,%d,%d,%d\n",
               sorted[i].name,
               sorted[i].file_count,
               sorted[i].blank_lines,
               sorted[i].comment_lines,
               sorted[i].code_lines,
               sorted[i].mixed_lines,
               sorted[i].total_lines);
    }

    printf("SUM,%d,%d,%d,%d,%d,%d\n",
           result->total_files,
           result->total_blank,
           result->total_comment,
           result->total_code,
           result->total_mixed,
           result->total_lines);

    free(sorted);
}

/* ---- JSON output to FILE* ---- */

void clocc_output_json_fp(const clocc_result_t *result,
                           const clocc_config_t *config, FILE *fp)
{
    int count = 0;
    clocc_lang_result_t *sorted = sort_results(result, config, &count);

    fprintf(fp, "{\n");
    fprintf(fp, "  \"clocity\": {\n");
    fprintf(fp, "    \"total_files\": %d,\n", result->total_files);
    fprintf(fp, "    \"total_lines\": %d,\n", result->total_lines);
    fprintf(fp, "    \"total_code\": %d,\n", result->total_code);
    fprintf(fp, "    \"total_comment\": %d,\n", result->total_comment);
    fprintf(fp, "    \"total_blank\": %d,\n", result->total_blank);
    fprintf(fp, "    \"total_mixed\": %d,\n", result->total_mixed);
    fprintf(fp, "    \"languages\": [\n");

    for (int i = 0; i < count; i++) {
        fprintf(fp, "      {\n");
        fprintf(fp, "        \"name\": \"%s\",\n", sorted[i].name);
        fprintf(fp, "        \"files\": %d,\n", sorted[i].file_count);
        fprintf(fp, "        \"code\": %d,\n", sorted[i].code_lines);
        fprintf(fp, "        \"comment\": %d,\n", sorted[i].comment_lines);
        fprintf(fp, "        \"blank\": %d,\n", sorted[i].blank_lines);
        fprintf(fp, "        \"mixed\": %d,\n", sorted[i].mixed_lines);
        fprintf(fp, "        \"lines\": %d\n", sorted[i].total_lines);
        if (i < count - 1)
            fprintf(fp, "      },\n");
        else
            fprintf(fp, "      }\n");
    }

    fprintf(fp, "    ]\n");
    fprintf(fp, "  }\n");
    fprintf(fp, "}\n");

    free(sorted);
}

/* ---- CSV output to FILE* ---- */

void clocc_output_csv_fp(const clocc_result_t *result,
                          const clocc_config_t *config, FILE *fp)
{
    int count = 0;
    clocc_lang_result_t *sorted = sort_results(result, config, &count);

    fprintf(fp, "Language,Files,Blank,Comment,Code,Mixed,Lines\n");

    for (int i = 0; i < count; i++) {
        fprintf(fp, "%s,%d,%d,%d,%d,%d,%d\n",
               sorted[i].name,
               sorted[i].file_count,
               sorted[i].blank_lines,
               sorted[i].comment_lines,
               sorted[i].code_lines,
               sorted[i].mixed_lines,
               sorted[i].total_lines);
    }

    fprintf(fp, "SUM,%d,%d,%d,%d,%d,%d\n",
           result->total_files,
           result->total_blank,
           result->total_comment,
           result->total_code,
           result->total_mixed,
           result->total_lines);

    free(sorted);
}

/* ---- YAML output ---- */

void clocc_output_yaml(const clocc_result_t *result,
                        const clocc_config_t *config)
{
    int count = 0;
    clocc_lang_result_t *sorted = sort_results(result, config, &count);

    /* Get current timestamp */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", tm_info);

    printf("cloc-c:\n");
    printf("  version: \"1.0.0\"\n");
    printf("  timestamp: \"%s\"\n", timestamp);
    printf("languages:\n");

    for (int i = 0; i < count; i++) {
        printf("  - name: \"%s\"\n", sorted[i].name);
        printf("    files: %d\n", sorted[i].file_count);
        printf("    code: %d\n", sorted[i].code_lines);
        printf("    comment: %d\n", sorted[i].comment_lines);
        printf("    blank: %d\n", sorted[i].blank_lines);
        printf("    mixed: %d\n", sorted[i].mixed_lines);
        printf("    lines: %d\n", sorted[i].total_lines);
    }

    printf("total:\n");
    printf("  files: %d\n", result->total_files);
    printf("  code: %d\n", result->total_code);
    printf("  comment: %d\n", result->total_comment);
    printf("  blank: %d\n", result->total_blank);
    printf("  mixed: %d\n", result->total_mixed);
    printf("  lines: %d\n", result->total_lines);

    free(sorted);
}
