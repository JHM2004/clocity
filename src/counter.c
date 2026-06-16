#include "clocc.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int starts_with(const char *s, const char *prefix)
{
    if (prefix == NULL || *prefix == '\0')
        return 0;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static int is_blank_line(const char *line, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (line[i] != ' ' && line[i] != '\t' &&
            line[i] != '\r' && line[i] != '\n')
            return 0;
    }
    return 1;
}



int clocc_count_file(const char *path, int lang_index,
                     clocc_file_result_t *result)
{
    FILE *fp = clocc_fopen(path, "rb");
    if (fp == NULL)
        return -1;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }

    long fsize = ftell(fp);
    if (fsize < 0) {
        fclose(fp);
        return -1;
    }

    rewind(fp);

    char *buf = malloc((size_t)fsize + 1);
    if (buf == NULL) {
        fclose(fp);
        return -1;
    }

    size_t nread = fread(buf, 1, (size_t)fsize, fp);
    fclose(fp);

    if ((long)nread != fsize) {
        free(buf);
        return -1;
    }

    buf[nread] = '\0';

    int rc = clocc_count_buffer(buf, nread, lang_index, result);
    free(buf);

    strncpy(result->path, path, CLOCC_MAX_PATH - 1);
    result->lang_index = lang_index;

    return rc;
}

int clocc_count_buffer(const char *buf, size_t len, int lang_index,
                       clocc_file_result_t *result)
{
    const clocc_lang_t *lang = clocc_lang_get(lang_index);
    if (lang == NULL)
        return -1;

    result->code_lines = 0;
    result->comment_lines = 0;
    result->blank_lines = 0;
    result->mixed_lines = 0;

    clocc_parser_state_t state = CLOCC_STATE_CODE;
    int block_depth = 0;
    size_t pos = 0;

    size_t block_start_len =
        (lang->block_comment_start && lang->block_comment_start[0])
            ? strlen(lang->block_comment_start) : 0;
    size_t block_end_len =
        (lang->block_comment_end && lang->block_comment_end[0])
            ? strlen(lang->block_comment_end) : 0;
    size_t block_start_alt_len =
        (lang->block_comment_start_alt && lang->block_comment_start_alt[0])
            ? strlen(lang->block_comment_start_alt) : 0;
    size_t block_end_alt_len =
        (lang->block_comment_end_alt && lang->block_comment_end_alt[0])
            ? strlen(lang->block_comment_end_alt) : 0;

    while (pos < len) {
        /* Find end of current line */
        size_t line_start = pos;
        size_t line_end = pos;
        while (line_end < len && buf[line_end] != '\n')
            line_end++;

        size_t line_len = line_end - line_start;

        /* Determine line type */
        int has_code = 0;
        int has_comment = 0;
        size_t i = line_start;

        while (i < line_end) {
            char ch = buf[i];

            switch (state) {
            case CLOCC_STATE_CODE: {
                const char *rest = buf + i;

                /* Check for line comment */
                if (lang->line_comment != NULL &&
                    starts_with(rest, lang->line_comment)) {
                    state = CLOCC_STATE_LINE_COMMENT;
                    has_comment = 1;
                    /* Rest of line is comment */
                    i = line_end;
                    continue;
                }

                /* Check for block comment start */
                if (lang->block_comment_start != NULL &&
                    starts_with(rest, lang->block_comment_start)) {
                    state = CLOCC_STATE_BLOCK_COMMENT;
                    block_depth = 1;
                    has_comment = 1;
                    i += block_start_len;
                    continue;
                }

                /* Check for alt block comment start */
                if (lang->block_comment_start_alt != NULL &&
                    starts_with(rest, lang->block_comment_start_alt)) {
                    state = CLOCC_STATE_BLOCK_COMMENT;
                    block_depth = 1;
                    has_comment = 1;
                    i += block_start_alt_len;
                    continue;
                }

                /* Check for multiline string (triple-quote) */
                if (lang->multiline_strings &&
                    lang->string_literals &&
                    i + 2 < line_end &&
                    ch == '"' && buf[i + 1] == '"' &&
                    buf[i + 2] == '"') {
                    state = CLOCC_STATE_MULTILINE_STRING;
                    i += 3;
                    continue;
                }

                /* Check for string literal */
                if (lang->string_literals && ch == '"') {
                    state = CLOCC_STATE_STRING;
                    i++;
                    continue;
                }

                /* Check for char literal */
                if (lang->string_literals && ch == '\'') {
                    state = CLOCC_STATE_CHAR;
                    i++;
                    continue;
                }

                /* Regular code character */
                if (ch != ' ' && ch != '\t' && ch != '\r')
                    has_code = 1;
                i++;
                break;
            }

            case CLOCC_STATE_LINE_COMMENT:
                /* Rest of line is comment */
                has_comment = 1;
                i = line_end;
                break;

            case CLOCC_STATE_BLOCK_COMMENT: {
                has_comment = 1;
                const char *rest = buf + i;

                /* Check for nested block comment start */
                if (lang->nested_comments &&
                    lang->block_comment_start != NULL &&
                    starts_with(rest, lang->block_comment_start)) {
                    block_depth++;
                    i += block_start_len;
                    continue;
                }

                /* Check for nested alt block comment start */
                if (lang->nested_comments &&
                    lang->block_comment_start_alt != NULL &&
                    starts_with(rest, lang->block_comment_start_alt)) {
                    block_depth++;
                    i += block_start_alt_len;
                    continue;
                }

                /* Check for block comment end */
                if (lang->block_comment_end != NULL &&
                    starts_with(rest, lang->block_comment_end)) {
                    block_depth--;
                    i += block_end_len;
                    if (block_depth == 0) {
                        state = CLOCC_STATE_CODE;
                    }
                    continue;
                }

                /* Check for alt block comment end */
                if (lang->block_comment_end_alt != NULL &&
                    starts_with(rest, lang->block_comment_end_alt)) {
                    block_depth--;
                    i += block_end_alt_len;
                    if (block_depth == 0) {
                        state = CLOCC_STATE_CODE;
                    }
                    continue;
                }

                i++;
                break;
            }

            case CLOCC_STATE_STRING:
                if (ch == '\\') {
                    /* Escape sequence: skip next char */
                    i += 2;
                    continue;
                }
                if (ch == '"') {
                    state = CLOCC_STATE_CODE;
                    i++;
                    continue;
                }
                i++;
                break;

            case CLOCC_STATE_CHAR:
                if (ch == '\\') {
                    /* Escape sequence: skip next char */
                    i += 2;
                    continue;
                }
                if (ch == '\'') {
                    state = CLOCC_STATE_CODE;
                    i++;
                    continue;
                }
                i++;
                break;

            case CLOCC_STATE_MULTILINE_STRING:
                if (i + 2 < line_end &&
                    ch == '"' && buf[i + 1] == '"' &&
                    buf[i + 2] == '"') {
                    state = CLOCC_STATE_CODE;
                    i += 3;
                    continue;
                }
                i++;
                break;
            }
        }

        /* End-of-line state reset for line comments */
        if (state == CLOCC_STATE_LINE_COMMENT)
            state = CLOCC_STATE_CODE;

        /* Classify the line */
        if (state == CLOCC_STATE_MULTILINE_STRING) {
            /* Inside a multiline string — content is code (string literal) */
            result->code_lines++;
        } else if (state == CLOCC_STATE_BLOCK_COMMENT) {
            /* Inside a block comment */
            if (is_blank_line(buf + line_start, line_len)) {
                result->comment_lines++;
            } else if (has_code && has_comment) {
                result->mixed_lines++;
            } else if (has_code) {
                result->code_lines++;
            } else {
                result->comment_lines++;
            }
        } else {
            /* Not inside a multi-line construct */
            if (is_blank_line(buf + line_start, line_len)) {
                result->blank_lines++;
            } else if (has_code && has_comment) {
                result->mixed_lines++;
            } else if (has_comment) {
                result->comment_lines++;
            } else {
                result->code_lines++;
            }
        }

        /* Advance past the newline */
        pos = line_end + 1;
    }

    return 0;
}
