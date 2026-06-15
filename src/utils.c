#include "clocc.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ── djb2 hash ──────────────────────────────────────────────────────── */

size_t clocc_hash_string(const char *str)
{
    size_t hash = 5381;
    int c;

    while ((c = (unsigned char)*str++) != '\0')
        hash = hash * 33 + (size_t)c;

    return hash;
}

/* ── File extension extraction ──────────────────────────────────────── */

const char *clocc_get_extension(const char *path)
{
    if (path == NULL)
        return NULL;

    const char *dot = NULL;
    const char *p = path;

    while (*p != '\0') {
        if (*p == '/' || *p == '\\')
            dot = NULL;
        else if (*p == '.')
            dot = p;
        p++;
    }

    /* No dot found, or dot is at the start of a path segment */
    if (dot == NULL)
        return NULL;
    if (dot == path || dot[-1] == '/' || dot[-1] == '\\')
        return NULL;

    return dot + 1;
}

/* ── Case-insensitive string compare ────────────────────────────────── */

int clocc_str_icmp(const char *a, const char *b)
{
    if (a == b)
        return 0;
    if (a == NULL)
        return -1;
    if (b == NULL)
        return 1;

    while (*a != '\0' && *b != '\0') {
        int ca = (unsigned char)*a;
        int cb = (unsigned char)*b;

        if (ca >= 'A' && ca <= 'Z')
            ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z')
            cb += 'a' - 'A';

        if (ca != cb)
            return ca - cb;

        a++;
        b++;
    }

    return (unsigned char)*a - (unsigned char)*b;
}

/* ── Aggregate per-file results into per-language summary ───────────── */

int clocc_aggregate_results(const clocc_file_result_t *files,
                            int file_count, clocc_result_t *result)
{
    memset(result, 0, sizeof(*result));

    /* Save a copy of per-file results for detail view */
    if (file_count > 0 && files) {
        result->file_results = malloc((size_t)file_count *
                                      sizeof(clocc_file_result_t));
        if (result->file_results) {
            memcpy(result->file_results, files,
                   (size_t)file_count * sizeof(clocc_file_result_t));
            result->file_result_count = file_count;
        }
    }

    /* We need space for all known languages + up to 256 binary extensions */
    int max_langs = clocc_lang_count() + 256;
    if (max_langs <= 1) {
        result->lang_count = 0;
        return 0;
    }

    result->languages = calloc((size_t)max_langs,
                               sizeof(clocc_lang_result_t));
    if (result->languages == NULL)
        return -1;

    /* Static buffer for extension-based names like "JPG", "PNG", etc. */
    static char ext_names[256][16];

    for (int i = 0; i < 256; i++)
        ext_names[i][0] = '\0';

    for (int i = 0; i < file_count; i++) {
        const clocc_file_result_t *f = &files[i];

        const char *lang_name = NULL;
        if (f->is_binary || f->lang_index < 0) {
            /* Categorize by extension: "JPG", "PNG", "GZ", etc. */
            if (f->ext && f->ext[0]) {
                /* Find or create an uppercase extension name */
                int slot = -1;
                for (int j = 0; j < result->lang_count; j++) {
                    if (result->languages[j].name &&
                        result->languages[j].name[0] != '\0') {
                        const char *existing = result->languages[j].name;
                        const char *ext = f->ext;
                        int match = 1;
                        while (*existing && *ext) {
                            int ec = (unsigned char)*existing;
                            int xc = (unsigned char)*ext;
                            if (ec >= 'a' && ec <= 'z') ec -= 'a' - 'A';
                            if (xc >= 'a' && xc <= 'z') xc -= 'a' - 'A';
                            if (ec != xc) { match = 0; break; }
                            existing++;
                            ext++;
                        }
                        if (match && *existing == '\0' && *ext == '\0') {
                            slot = j;
                            break;
                        }
                    }
                }
                if (slot < 0) {
                    /* Create new extension name (uppercase) */
                    slot = result->lang_count;
                    int idx = slot;
                    if (idx < 256) {
                        const char *e = f->ext;
                        int k = 0;
                        while (*e && k < 15) {
                            char c = *e;
                            if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
                            ext_names[idx][k++] = c;
                            e++;
                        }
                        ext_names[idx][k] = '\0';
                        result->languages[slot].name = ext_names[idx];
                    }
                    result->lang_count++;
                }
                lang_name = result->languages[slot].name;
            } else {
                /* No extension — use "Other" */
                static const char *other_name = "Other";
                lang_name = other_name;
            }
        } else {
            const clocc_lang_t *lang = clocc_lang_get(f->lang_index);
            if (lang == NULL) {
                static const char *other_name = "Other";
                lang_name = other_name;
            } else {
                lang_name = lang->name;
            }
        }

        /* Find or create the language entry */
        int slot = -1;
        for (int j = 0; j < result->lang_count; j++) {
            if (result->languages[j].name == lang_name) {
                slot = j;
                break;
            }
        }

        if (slot < 0) {
            slot = result->lang_count;
            result->languages[slot].name = lang_name;
            result->lang_count++;
        }

        clocc_lang_result_t *lr = &result->languages[slot];
        lr->file_count++;
        lr->code_lines += f->code_lines;
        lr->comment_lines += f->comment_lines;
        lr->blank_lines += f->blank_lines;
        lr->mixed_lines += f->mixed_lines;
    }

    /* Calculate per-language totals and overall totals */
    for (int i = 0; i < result->lang_count; i++) {
        clocc_lang_result_t *lr = &result->languages[i];
        lr->total_lines = lr->code_lines + lr->comment_lines +
                          lr->blank_lines + lr->mixed_lines;

        result->total_files += lr->file_count;
        result->total_code += lr->code_lines;
        result->total_comment += lr->comment_lines;
        result->total_blank += lr->blank_lines;
        result->total_mixed += lr->mixed_lines;
    }

    result->total_lines = result->total_code + result->total_comment +
                          result->total_blank + result->total_mixed;

    return 0;
}
