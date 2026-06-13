#include "clocc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

/* ------------------------------------------------------------------ */
/*  Static data                                                       */
/* ------------------------------------------------------------------ */

/* Directories to always skip */
static const char *excluded_dirs[] = {
    ".git", ".svn", ".hg", ".bzr",
    "node_modules", "vendor", "__pycache__",
    ".tox", ".mypy_cache", ".pytest_cache",
    ".venv", "venv", "env", ".env",
    ".idea", ".vscode",
    "target", "build", "dist", "out",
    ".gradle", ".mvn", ".cargo", ".cache",
    NULL
};

/* Binary file extensions */
static const char *binary_exts[] = {
    "exe", "dll", "so", "dylib", "o", "obj", "a", "lib",
    "png", "jpg", "jpeg", "gif", "bmp", "ico", "svg",
    "tiff", "webp",
    "mp3", "mp4", "avi", "mkv", "mov", "wmv",
    "flac", "ogg", "wav",
    "zip", "tar", "gz", "bz2", "xz", "7z", "rar",
    "jar", "war", "ear",
    "pdf", "doc", "docx", "xls", "xlsx", "ppt", "pptx",
    "class", "pyc", "pycache", "pkl",
    "db", "sqlite",
    "iso", "dmg", "msi", "deb", "rpm", "apk",
    "woff", "woff2", "ttf", "otf", "eot",
    NULL
};

/* ------------------------------------------------------------------ */
/*  Gitignore structures                                              */
/* ------------------------------------------------------------------ */

#define GITIGNORE_MAX_RULES  512
#define GITIGNORE_MAX_PAT    1024
#define GITIGNORE_CACHE_SIZE 64

typedef struct gitignore_rule_t {
    char pattern[GITIGNORE_MAX_PAT];
    int  is_negated;
    int  is_dir_only;
    int  is_anchored;
} gitignore_rule_t;

typedef struct gitignore_cache_t {
    char dir[CLOCC_MAX_PATH];
    gitignore_rule_t rules[GITIGNORE_MAX_RULES];
    int rule_count;
} gitignore_cache_t;

static gitignore_cache_t gitignore_cache[GITIGNORE_CACHE_SIZE];
static int gitignore_cache_count = 0;

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                  */
/* ------------------------------------------------------------------ */

/* Check if a directory name is in the excluded list */
static int is_excluded_dir(const char *name)
{
    for (int i = 0; excluded_dirs[i] != NULL; i++) {
        if (clocc_str_icmp(name, excluded_dirs[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Check if an extension is in the binary list */
static int is_binary_ext(const char *ext)
{
    if (!ext || !*ext) return 0;
    for (int i = 0; binary_exts[i] != NULL; i++) {
        if (clocc_str_icmp(ext, binary_exts[i]) == 0) {
            return 1;
        }
    }
    return 0;
}



/* ------------------------------------------------------------------ */
/*  Gitignore parsing and matching                                    */
/* ------------------------------------------------------------------ */

/* Parse a single .gitignore file into a cache entry */
static int parse_gitignore(const char *dir, gitignore_cache_t *cache)
{
    char filepath[CLOCC_MAX_PATH];
    snprintf(filepath, sizeof(filepath), "%s/.gitignore", dir);

    FILE *fp = fopen(filepath, "r");
    if (!fp) return -1;

    strncpy(cache->dir, dir, CLOCC_MAX_PATH - 1);
    cache->dir[CLOCC_MAX_PATH - 1] = '\0';
    cache->rule_count = 0;

    char line[GITIGNORE_MAX_PAT];
    while (fgets(line, sizeof(line), fp) &&
           cache->rule_count < GITIGNORE_MAX_RULES) {
        /* Strip trailing newline / carriage return */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' ||
                           line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        /* Skip blank lines and comments */
        if (len == 0 || line[0] == '#') continue;

        gitignore_rule_t *rule = &cache->rules[cache->rule_count];
        memset(rule, 0, sizeof(*rule));

        const char *p = line;

        /* Negation */
        if (*p == '!') {
            rule->is_negated = 1;
            p++;
        }

        /* Directory-only flag */
        size_t plen = strlen(p);
        if (plen > 0 && p[plen - 1] == '/') {
            rule->is_dir_only = 1;
            plen--;
        }

        /* Anchored */
        if (*p == '/') {
            rule->is_anchored = 1;
            p++;
            plen--;
        }

        if (plen >= GITIGNORE_MAX_PAT) plen = GITIGNORE_MAX_PAT - 1;
        memcpy(rule->pattern, p, plen);
        rule->pattern[plen] = '\0';

        cache->rule_count++;
    }

    fclose(fp);
    return 0;
}

/* Find or create a cache entry for a directory */
static gitignore_cache_t *get_gitignore_cache(const char *dir)
{
    /* Look for existing cache */
    for (int i = 0; i < gitignore_cache_count; i++) {
        if (strcmp(gitignore_cache[i].dir, dir) == 0) {
            return &gitignore_cache[i];
        }
    }

    /* Try to parse a new one */
    if (gitignore_cache_count >= GITIGNORE_CACHE_SIZE) {
        return NULL;
    }

    gitignore_cache_t *cache = &gitignore_cache[gitignore_cache_count];
    if (parse_gitignore(dir, cache) == 0) {
        gitignore_cache_count++;
        return cache;
    }

    /* No .gitignore in this dir – cache an empty entry */
    strncpy(cache->dir, dir, CLOCC_MAX_PATH - 1);
    cache->dir[CLOCC_MAX_PATH - 1] = '\0';
    cache->rule_count = 0;
    gitignore_cache_count++;
    return cache;
}

/* Simple glob match: * matches anything except /, ** matches everything */
static int glob_match(const char *pat, const char *str)
{
    while (*pat && *str) {
        if (pat[0] == '*' && pat[1] == '*') {
            pat += 2;
            /* ** matches zero or more path segments */
            do {
                if (glob_match(pat, str)) return 1;
                str = strchr(str, '/');
                if (!str) break;
                str++;
            } while (1);
            return glob_match(pat, str);
        } else if (*pat == '*') {
            pat++;
            /* * matches anything except / */
            do {
                if (glob_match(pat, str)) return 1;
                if (*str == '/' || *str == '\0') break;
                str++;
            } while (1);
            return 0;
        } else if (*pat == *str) {
            pat++;
            str++;
        } else {
            return 0;
        }
    }

    /* Skip trailing ** or * in pattern */
    while (*pat == '*') pat++;

    return *pat == '\0' && *str == '\0';
}

/* Check a single gitignore rule against a relative path */
static int match_rule(const gitignore_rule_t *rule,
                      const char *rel_path, int is_dir)
{
    if (rule->is_dir_only && !is_dir) return 0;

    const char *str = rel_path;

    if (rule->is_anchored) {
        return glob_match(rule->pattern, str);
    }

    /* Unanchored: match against the full path or any trailing segment */
    if (glob_match(rule->pattern, str)) return 1;

    /* Try matching from each path segment */
    const char *slash = strchr(str, '/');
    while (slash) {
        if (glob_match(rule->pattern, slash + 1)) return 1;
        slash = strchr(slash + 1, '/');
    }

    return 0;
}

/* Public: check if a path should be ignored by .gitignore rules */
int clocc_match_gitignore(const char *path)
{
    /* Determine directory portion */
    char dir[CLOCC_MAX_PATH];
    strncpy(dir, path, CLOCC_MAX_PATH - 1);
    dir[CLOCC_MAX_PATH - 1] = '\0';

    char *sep = strrchr(dir, '/');
    char *sep2 = strrchr(dir, '\\');
    char *last_sep;
    if (!sep) last_sep = sep2;
    else if (!sep2) last_sep = sep;
    else last_sep = sep > sep2 ? sep : sep2;

    const char *rel;
    if (last_sep) {
        *last_sep = '\0';
        rel = path + (last_sep - dir) + 1;
    } else {
        dir[0] = '.';
        dir[1] = '\0';
        rel = path;
    }

    gitignore_cache_t *cache = get_gitignore_cache(dir);
    if (!cache) return 0;

    int ignored = 0;
    for (int i = 0; i < cache->rule_count; i++) {
        const gitignore_rule_t *rule = &cache->rules[i];
        if (match_rule(rule, rel, 0)) {
            ignored = rule->is_negated ? 0 : 1;
        }
    }

    return ignored;
}

/* Check gitignore for a directory entry (with is_dir flag) */
static int match_gitignore_dir(const char *dir_path, const char *name,
                               int is_dir)
{
    char full[CLOCC_MAX_PATH];
    snprintf(full, sizeof(full), "%s/%s", dir_path, name);

    gitignore_cache_t *cache = get_gitignore_cache(dir_path);
    if (!cache) return 0;

    int ignored = 0;
    for (int i = 0; i < cache->rule_count; i++) {
        const gitignore_rule_t *rule = &cache->rules[i];
        if (match_rule(rule, name, is_dir)) {
            ignored = rule->is_negated ? 0 : 1;
        }
    }

    return ignored;
}

/* ------------------------------------------------------------------ */
/*  Binary file detection                                             */
/* ------------------------------------------------------------------ */

#define BINARY_CHECK_SIZE 8192

int clocc_is_binary_file(const char *path)
{
    /* Check extension first */
    const char *ext = clocc_get_extension(path);
    if (ext && is_binary_ext(ext)) return 1;

    /* Check first 8KB for NUL bytes */
    FILE *fp = fopen(path, "rb");
    if (!fp) return 1;  /* can't open → treat as binary */

    unsigned char buf[BINARY_CHECK_SIZE];
    size_t n = fread(buf, 1, sizeof(buf), fp);
    fclose(fp);

    for (size_t i = 0; i < n; i++) {
        if (buf[i] == 0) return 1;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  File list management                                              */
/* ------------------------------------------------------------------ */

void clocc_free_file_list(char **files, int file_count)
{
    if (!files) return;
    for (int i = 0; i < file_count; i++) {
        free(files[i]);
    }
    free(files);
}

/* Add a file path to the dynamic list; returns 0 on success */
static int add_file(char ***files, int *file_count, int *capacity,
                    const char *path)
{
    if (*file_count >= CLOCC_MAX_FILES) {
        return -1;
    }

    if (*file_count >= *capacity) {
        int new_cap = *capacity == 0 ? 256 : *capacity * 2;
        if (new_cap > CLOCC_MAX_FILES) new_cap = CLOCC_MAX_FILES;
        char **new_arr = realloc(*files, (size_t)new_cap * sizeof(char *));
        if (!new_arr) return -1;
        *files = new_arr;
        *capacity = new_cap;
    }

    (*files)[*file_count] = strdup(path);
    if (!(*files)[*file_count]) return -1;

    (*file_count)++;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Directory traversal – Unix                                        */
/* ------------------------------------------------------------------ */

#ifndef _WIN32

static int scan_dir_impl(const char *path, clocc_config_t *config,
                         char ***files, int *file_count, int *capacity)
{
    DIR *dp = opendir(path);
    if (!dp) {
        if (config->verbose) {
            fprintf(stderr, "clocc: cannot open directory: %s\n", path);
        }
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full[CLOCC_MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);

        /* Determine if it's a directory */
        struct stat st;
        if (stat(full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            /* Skip excluded directories */
            if (is_excluded_dir(entry->d_name)) continue;

            /* Check gitignore */
            if (match_gitignore_dir(path, entry->d_name, 1)) continue;

            /* Recurse */
            scan_dir_impl(full, config, files, file_count, capacity);
        } else if (S_ISREG(st.st_mode)) {
            /* Check gitignore */
            if (match_gitignore_dir(path, entry->d_name, 0)) continue;

            /* Only process recognized source extensions;
               skip binary content check for known extensions */
            const char *ext = clocc_get_extension(full);
            if (!ext || clocc_lang_by_extension(ext) < 0) continue;

            /* Add to list */
            if (add_file(files, file_count, capacity, full) != 0) {
                closedir(dp);
                return -1;
            }
        }
    }

    closedir(dp);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Directory traversal – Windows                                     */
/* ------------------------------------------------------------------ */

#else /* _WIN32 */

/* Convert UTF-8 to wide string; caller must free */
static wchar_t *utf8_to_wide(const char *utf8)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (len <= 0) return NULL;
    wchar_t *w = malloc((size_t)len * sizeof(wchar_t));
    if (!w) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, len);
    return w;
}

/* Convert wide string to UTF-8; caller must free */
static char *wide_to_utf8(const wchar_t *w)
{
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0,
                                  NULL, NULL);
    if (len <= 0) return NULL;
    char *s = malloc((size_t)len);
    if (!s) return NULL;
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s, len, NULL, NULL);
    return s;
}

static int scan_dir_impl(const char *path, clocc_config_t *config,
                         char ***files, int *file_count, int *capacity)
{
    char pattern[CLOCC_MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", path);

    wchar_t *wpattern = utf8_to_wide(pattern);
    if (!wpattern) return -1;

    WIN32_FIND_DATAW fdata;
    HANDLE hfind = FindFirstFileW(wpattern, &fdata);
    free(wpattern);

    if (hfind == INVALID_HANDLE_VALUE) {
        if (config->verbose) {
            fprintf(stderr, "clocc: cannot open directory: %s\n", path);
        }
        return -1;
    }

    do {
        /* Skip . and .. */
        if (fdata.cFileName[0] == L'.' &&
            (fdata.cFileName[1] == L'\0' ||
             (fdata.cFileName[1] == L'.' &&
              fdata.cFileName[2] == L'\0'))) {
            continue;
        }

        char *name_utf8 = wide_to_utf8(fdata.cFileName);
        if (!name_utf8) continue;

        char full[CLOCC_MAX_PATH];
        snprintf(full, sizeof(full), "%s\\%s", path, name_utf8);
        free(name_utf8);

        if (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            /* Get the short name for exclusion check */
            char *short_name = wide_to_utf8(fdata.cFileName);
            if (!short_name) continue;

            if (is_excluded_dir(short_name)) {
                free(short_name);
                continue;
            }

            if (match_gitignore_dir(path, short_name, 1)) {
                free(short_name);
                continue;
            }

            free(short_name);
            scan_dir_impl(full, config, files, file_count, capacity);
        } else {
            /* Get the short name for gitignore check */
            char *short_name = wide_to_utf8(fdata.cFileName);
            if (!short_name) continue;

            if (match_gitignore_dir(path, short_name, 0)) {
                free(short_name);
                continue;
            }

            free(short_name);

            /* Only process recognized source extensions;
               skip binary content check for known extensions */
            const char *ext = clocc_get_extension(full);
            if (!ext || clocc_lang_by_extension(ext) < 0) continue;

            if (add_file(files, file_count, capacity, full) != 0) {
                FindClose(hfind);
                return -1;
            }
        }
    } while (FindNextFileW(hfind, &fdata));

    FindClose(hfind);
    return 0;
}

#endif /* _WIN32 */

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

int clocc_scan_directory(const char *path, clocc_config_t *config,
                         char ***files, int *file_count)
{
    if (!path || !config || !files || !file_count) return -1;

    *files = NULL;
    *file_count = 0;
    int capacity = 0;

    /* Reset gitignore cache for a fresh scan */
    gitignore_cache_count = 0;

    /* Check if path is a single file */
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    if (attrs != INVALID_FILE_ATTRIBUTES &&
        !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        /* It's a file — add it directly */
        return add_file(files, file_count, &capacity, path);
    }
#else
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
        return add_file(files, file_count, &capacity, path);
    }
#endif

    int rc = scan_dir_impl(path, config, files, file_count, &capacity);

    if (rc != 0 && *files) {
        clocc_free_file_list(*files, *file_count);
        *files = NULL;
        *file_count = 0;
    }

    return rc;
}
