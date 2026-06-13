#ifndef CLOCC_H
#define CLOCC_H

#include <stddef.h>

/* Version */
#define CLOCC_VERSION           "1.0.0"

/* Constants */
#define CLOCC_MAX_PATH          4096
#define CLOCC_MAX_LANG_NAME     64
#define CLOCC_MAX_EXTENSIONS    32
#define CLOCC_MAX_EXT_LEN       16
#define CLOCC_MAX_COMMENT_LEN   8
#define CLOCC_MAX_SHEBANG_LEN   64
#define CLOCC_MAX_LANGUAGES     128
#define CLOCC_MAX_FILES         100000
#define CLOCC_HASH_SIZE         1024
#define CLOCC_MAX_THREADS       64

/* Output format options */
typedef enum {
    CLOCC_OUTPUT_TABLE,
    CLOCC_OUTPUT_JSON,
    CLOCC_OUTPUT_CSV,
    CLOCC_OUTPUT_YAML
} clocc_output_format_t;

/* Sort field options */
typedef enum {
    CLOCC_SORT_FILES,
    CLOCC_SORT_LINES,
    CLOCC_SORT_CODE,
    CLOCC_SORT_COMMENT,
    CLOCC_SORT_BLANK,
    CLOCC_SORT_MIXED
} clocc_sort_field_t;

/* Comment style */
typedef enum {
    CLOCC_COMMENT_C_STYLE,
    CLOCC_COMMENT_HASH,
    CLOCC_COMMENT_DASH,
    CLOCC_COMMENT_PERCENT,
    CLOCC_COMMENT_SEMICOLON,
    CLOCC_COMMENT_HTML,
    CLOCC_COMMENT_REM,
    CLOCC_COMMENT_LUA
} clocc_comment_style_t;

/* Parser state */
typedef enum {
    CLOCC_STATE_CODE,
    CLOCC_STATE_LINE_COMMENT,
    CLOCC_STATE_BLOCK_COMMENT,
    CLOCC_STATE_STRING,
    CLOCC_STATE_CHAR,
    CLOCC_STATE_MULTILINE_STRING
} clocc_parser_state_t;

/* Line type */
typedef enum {
    CLOCC_LINE_CODE,
    CLOCC_LINE_COMMENT,
    CLOCC_LINE_BLANK,
    CLOCC_LINE_MIXED
} clocc_line_type_t;

/* Language definition */
typedef struct {
    const char *name;
    const char **extensions;
    int ext_count;
    clocc_comment_style_t comment_style;
    const char *line_comment;
    const char *block_comment_start;
    const char *block_comment_end;
    const char *block_comment_start_alt;
    const char *block_comment_end_alt;
    int nested_comments;
    int string_literals;
    int multiline_strings;
    const char *shebang;
} clocc_lang_t;

/* Per-file result */
typedef struct {
    const char *path;
    int lang_index;
    int code_lines;
    int comment_lines;
    int blank_lines;
    int mixed_lines;
} clocc_file_result_t;

/* Per-language result */
typedef struct {
    const char *name;
    int file_count;
    int code_lines;
    int comment_lines;
    int blank_lines;
    int mixed_lines;
    int total_lines;
} clocc_lang_result_t;

/* Aggregate result */
typedef struct {
    clocc_lang_result_t *languages;
    int lang_count;
    int total_files;
    int total_code;
    int total_comment;
    int total_blank;
    int total_mixed;
    int total_lines;
} clocc_result_t;

/* Configuration */
typedef struct {
    const char **paths;
    int path_count;
    clocc_output_format_t format;
    clocc_sort_field_t sort_by;
    int sort_descending;
    const char **filter_langs;
    int filter_lang_count;
    int thread_count;
    int incremental;
    const char *prev_json_path;
    int show_colors;
    int exclude_empty;
    int verbose;
    /* Internal: file list for thread processing */
    const char **files;
    int file_count;
} clocc_config_t;

/* Language functions (language.c) */
void clocc_lang_init(void);
int clocc_lang_by_extension(const char *ext);
int clocc_lang_by_shebang(const char *path);
clocc_lang_t *clocc_lang_get(int index);
int clocc_lang_count(void);

/* Scanner functions (scanner.c) */
int clocc_scan_directory(const char *path, clocc_config_t *config,
                         char ***files, int *file_count);
void clocc_free_file_list(char **files, int file_count);
int clocc_match_gitignore(const char *path);
int clocc_is_binary_file(const char *path);

/* Counter functions (counter.c) */
int clocc_count_file(const char *path, int lang_index,
                     clocc_file_result_t *result);
int clocc_count_buffer(const char *buf, size_t len, int lang_index,
                       clocc_file_result_t *result);

/* Utility functions (utils.c) */
size_t clocc_hash_string(const char *str);
const char *clocc_get_extension(const char *path);
int clocc_str_icmp(const char *a, const char *b);
int clocc_aggregate_results(const clocc_file_result_t *files,
                            int file_count, clocc_result_t *result);

/* Thread pool (thread.c) */
int clocc_thread_init(int thread_count);
int clocc_thread_process(clocc_config_t *config, clocc_result_t *result);
void clocc_thread_cleanup(void);

/* OS functions (os_win32.c / os_unix.c) */
void clocc_os_init(void);
double clocc_os_time(void);
int clocc_os_cpu_count(void);

/* Output functions (output.c) */
void clocc_output_table(const clocc_result_t *result,
                        const clocc_config_t *config);
void clocc_output_json(const clocc_result_t *result,
                        const clocc_config_t *config);
void clocc_output_csv(const clocc_result_t *result,
                       const clocc_config_t *config);
void clocc_output_yaml(const clocc_result_t *result,
                        const clocc_config_t *config);

#endif /* CLOCC_H */
