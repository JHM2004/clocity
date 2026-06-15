#include "clocc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

/* ------------------------------------------------------------------ */
/*  Progress bar for CLI                                              */
/* ------------------------------------------------------------------ */

#ifdef _WIN32
static CRITICAL_SECTION g_progress_lock;
static int g_progress_lock_init = 0;
#endif

static void cli_progress(int phase, int done, int total, void *user_data)
{
    (void)user_data;
    const int bar_width = 30;

#ifdef _WIN32
    if (!g_progress_lock_init) {
        InitializeCriticalSection(&g_progress_lock);
        g_progress_lock_init = 1;
    }
    EnterCriticalSection(&g_progress_lock);
#endif

    if (phase == 0) {
        /* Scanning phase — total unknown */
        fprintf(stderr, "\r\x1b[K  Scanning... %d files found", done);
    } else {
        /* Counting phase — total known */
        int pct = (total > 0) ? (done * 100 / total) : 0;
        int filled = (total > 0) ? (done * bar_width / total) : 0;

        fprintf(stderr, "\r\x1b[K  Counting [");
        for (int i = 0; i < bar_width; i++) {
            if (i < filled) fputc('=', stderr);
            else if (i == filled) fputc('>', stderr);
            else fputc(' ', stderr);
        }
        fprintf(stderr, "] %3d%% (%d/%d)", pct, done, total);

        if (done >= total)
            fputc('\n', stderr);
    }
    fflush(stderr);

#ifdef _WIN32
    LeaveCriticalSection(&g_progress_lock);
#endif
}

/* ------------------------------------------------------------------ */
/*  Usage / help                                                      */
/* ------------------------------------------------------------------ */

static void print_usage(void)
{
    printf("cloc-c v%s — Fast Code Line Counter\n",
           CLOCC_VERSION);
    printf("Usage: clocc [options] <path...>\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help           Show this help\n");
    printf("  -v, --version        Show version\n");
    printf("  -f FORMAT            Output format: table (default),"
           " json, csv, yaml\n");
    printf("  -s FIELD             Sort by: code (default),"
           " files, lines, comment, blank, mixed\n");
    printf("  -r, --sort-reverse   Sort in reverse order\n");
    printf("  -t LANGS             Filter by language"
           " (comma-separated, e.g., -t C,Python)\n");
    printf("  -j N                 Use N threads (0=auto)\n");
    printf("  --no-color           Disable colored output\n");
    printf("  --exclude-empty      Exclude languages with"
           " zero files\n");
    printf("  --verbose            Verbose output\n");
}

/* ------------------------------------------------------------------ */
/*  Argument parsing                                                  */
/* ------------------------------------------------------------------ */

static int parse_args(int argc, char **argv, clocc_config_t *config)
{
    /* Defaults */
    config->paths = NULL;
    config->path_count = 0;
    config->format = CLOCC_OUTPUT_TABLE;
    config->sort_by = CLOCC_SORT_CODE;
    config->sort_descending = 1;
    config->filter_langs = NULL;
    config->filter_lang_count = 0;
    config->thread_count = 0;
    config->incremental = 0;
    config->prev_json_path = NULL;
    config->show_colors = 1;
    config->exclude_empty = 0;
    config->verbose = 0;
    config->progress_cb = NULL;
    config->progress_data = NULL;
    config->files = NULL;
    config->file_count = 0;

    int path_cap = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 ||
            strcmp(argv[i], "--help") == 0) {
            print_usage();
            exit(0);
        } else if (strcmp(argv[i], "-v") == 0 ||
                   strcmp(argv[i], "--version") == 0) {
            printf("cloc-c v%s\n", CLOCC_VERSION);
            exit(0);
        } else if (strcmp(argv[i], "-f") == 0) {
            if (++i >= argc) {
                fprintf(stderr,
                        "error: -f requires an argument\n");
                return -1;
            }
            if (strcmp(argv[i], "table") == 0)
                config->format = CLOCC_OUTPUT_TABLE;
            else if (strcmp(argv[i], "json") == 0)
                config->format = CLOCC_OUTPUT_JSON;
            else if (strcmp(argv[i], "csv") == 0)
                config->format = CLOCC_OUTPUT_CSV;
            else if (strcmp(argv[i], "yaml") == 0)
                config->format = CLOCC_OUTPUT_YAML;
            else {
                fprintf(stderr,
                        "error: unknown format '%s'\n", argv[i]);
                return -1;
            }
        } else if (strcmp(argv[i], "-s") == 0) {
            if (++i >= argc) {
                fprintf(stderr,
                        "error: -s requires an argument\n");
                return -1;
            }
            if (strcmp(argv[i], "code") == 0)
                config->sort_by = CLOCC_SORT_CODE;
            else if (strcmp(argv[i], "files") == 0)
                config->sort_by = CLOCC_SORT_FILES;
            else if (strcmp(argv[i], "lines") == 0)
                config->sort_by = CLOCC_SORT_LINES;
            else if (strcmp(argv[i], "comment") == 0)
                config->sort_by = CLOCC_SORT_COMMENT;
            else if (strcmp(argv[i], "blank") == 0)
                config->sort_by = CLOCC_SORT_BLANK;
            else if (strcmp(argv[i], "mixed") == 0)
                config->sort_by = CLOCC_SORT_MIXED;
            else {
                fprintf(stderr,
                        "error: unknown sort field '%s'\n",
                        argv[i]);
                return -1;
            }
        } else if (strcmp(argv[i], "-r") == 0 ||
                   strcmp(argv[i], "--sort-reverse") == 0) {
            config->sort_descending = 0;
        } else if (strcmp(argv[i], "-t") == 0) {
            if (++i >= argc) {
                fprintf(stderr,
                        "error: -t requires an argument\n");
                return -1;
            }
            char *dup = strdup(argv[i]);
            if (!dup) return -1;
            char *tok = strtok(dup, ",");
            while (tok) {
                while (*tok == ' ') tok++;
                char *end = tok + strlen(tok) - 1;
                while (end > tok && *end == ' ')
                    *end-- = '\0';
                if (*tok) {
                    int n = config->filter_lang_count++;
                    const char **tmp = realloc(
                        config->filter_langs,
                        (size_t)config->filter_lang_count *
                        sizeof(const char *));
                    if (!tmp) {
                        free(dup);
                        return -1;
                    }
                    config->filter_langs = tmp;
                    config->filter_langs[n] = strdup(tok);
                    if (!config->filter_langs[n]) {
                        free(dup);
                        return -1;
                    }
                }
                tok = strtok(NULL, ",");
            }
            free(dup);
        } else if (strcmp(argv[i], "-j") == 0) {
            if (++i >= argc) {
                fprintf(stderr,
                        "error: -j requires an argument\n");
                return -1;
            }
            config->thread_count = atoi(argv[i]);
        } else if (strcmp(argv[i], "--no-color") == 0) {
            config->show_colors = 0;
        } else if (strcmp(argv[i], "--exclude-empty") == 0) {
            config->exclude_empty = 1;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            config->verbose = 1;
        } else if (argv[i][0] == '-') {
            fprintf(stderr,
                    "error: unknown option '%s'\n", argv[i]);
            return -1;
        } else {
            /* Path argument */
            if (config->path_count >= path_cap) {
                path_cap = path_cap == 0 ? 8 : path_cap * 2;
                config->paths = realloc(
                    config->paths,
                    (size_t)path_cap * sizeof(const char *));
                if (!config->paths) return -1;
            }
            config->paths[config->path_count++] = argv[i];
        }
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Main                                                              */
/* ------------------------------------------------------------------ */

int cli_main(int argc, char **argv)
{
    clocc_os_init();

#ifdef _WIN32
    /* On Windows, argv is in ANSI encoding which breaks non-ASCII paths.
       Re-acquire arguments in Unicode and convert to UTF-8. */
    int wargc;
    LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    char **utf8_argv = NULL;
    if (wargv && wargc == argc) {
        utf8_argv = malloc((size_t)wargc * sizeof(char *));
        if (utf8_argv) {
            for (int i = 0; i < wargc; i++) {
                int len = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1,
                                              NULL, 0, NULL, NULL);
                utf8_argv[i] = malloc((size_t)len);
                if (utf8_argv[i]) {
                    WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1,
                                        utf8_argv[i], len, NULL, NULL);
                }
            }
            argv = utf8_argv;
        }
    }
    if (wargv) LocalFree(wargv);
#endif

    /* Detect if launched by double-click (no arguments) */
    int interactive = (argc == 1);
    int ret = 0;

    clocc_config_t config;
    if (parse_args(argc, argv, &config) != 0) {
        if (interactive) {
            fprintf(stderr, "\nPress Enter to exit...");
            getchar();
        }
        ret = 1;
        goto cleanup_argv;
    }

    if (config.path_count == 0) {
        if (interactive) {
            /* Interactive mode: prompt user for a path */
            printf("clocity - Blazing-fast code line counter\n");
            printf("Enter a directory path to count (or press Enter to exit): ");
            char input[CLOCC_MAX_PATH];
            if (fgets(input, sizeof(input), stdin) != NULL) {
                /* Trim trailing newline */
                size_t len = strlen(input);
                while (len > 0 && (input[len-1] == '\n' ||
                                   input[len-1] == '\r'))
                    input[--len] = '\0';
                if (len > 0) {
                    config.paths = malloc(sizeof(const char *));
                    if (config.paths) {
                        config.paths[0] = strdup(input);
                        config.path_count = 1;
                    }
                }
            }
            if (config.path_count == 0) {
                printf("No path provided. Exiting.\n");
                goto cleanup_argv;
            }
        } else {
            print_usage();
            ret = 1;
            goto cleanup_argv;
        }
    }

    clocc_lang_init();

    /* Auto-detect thread count */
    if (config.thread_count == 0)
        config.thread_count = clocc_os_cpu_count();

    /* Set up progress bar */
    config.progress_cb = cli_progress;

    clocc_thread_init(config.thread_count);

    double t_start = clocc_os_time();

    /* Scan all directories, collecting file paths into one array */
    const char **all_files = NULL;
    int all_file_count = 0;
    int all_file_cap = 0;

    for (int p = 0; p < config.path_count; p++) {
        char **files = NULL;
        int file_count = 0;

        if (clocc_scan_directory(config.paths[p], &config,
                                 &files, &file_count) != 0) {
            fprintf(stderr, "clocc: error scanning '%s'\n",
                    config.paths[p]);
            continue;
        }

        if (config.verbose) {
            fprintf(stderr, "clocc: found %d files in '%s'\n",
                    file_count, config.paths[p]);
        }

        for (int f = 0; f < file_count; f++) {
            if (all_file_count >= all_file_cap) {
                all_file_cap = all_file_cap == 0 ? 256
                                                 : all_file_cap * 2;
                all_files = realloc(
                    all_files,
                    (size_t)all_file_cap * sizeof(const char *));
                if (!all_files) {
                    fprintf(stderr,
                            "clocc: out of memory\n");
                    free(files);
                    for (int i = 0; i < all_file_count; i++)
                        free((void *)all_files[i]);
                    free(all_files);
                    clocc_thread_cleanup();
                    ret = 1;
                    goto cleanup_argv;
                }
            }
            all_files[all_file_count++] = files[f];
        }

        /* Free the scan list but not the strings (owned by all_files) */
        free(files);
    }

    /* Process files via thread pool */
    config.files = all_files;
    config.file_count = all_file_count;

    clocc_result_t agg;
    memset(&agg, 0, sizeof(agg));

    if (clocc_thread_process(&config, &agg) != 0) {
        fprintf(stderr, "clocc: error processing files\n");
        free(all_files);
        clocc_thread_cleanup();
        ret = 1;
        goto cleanup_argv;
    }

    /* Output results */
    switch (config.format) {
    case CLOCC_OUTPUT_TABLE:
        clocc_output_table(&agg, &config);
        break;
    case CLOCC_OUTPUT_JSON:
        clocc_output_json(&agg, &config);
        break;
    case CLOCC_OUTPUT_CSV:
        clocc_output_csv(&agg, &config);
        break;
    case CLOCC_OUTPUT_YAML:
        clocc_output_yaml(&agg, &config);
        break;
    }

    double t_end = clocc_os_time();
    printf("\n%.3f seconds\n", t_end - t_start);

    /* In interactive mode, wait for keypress before exiting */
    if (interactive) {
        printf("\nPress Enter to exit...");
        getchar();
    }

    /* Cleanup */
    free(agg.languages);
    for (int i = 0; i < all_file_count; i++)
        free((void *)all_files[i]);
    free(all_files);
    for (int i = 0; i < config.filter_lang_count; i++)
        free((void *)config.filter_langs[i]);
    free(config.filter_langs);
    free(config.paths);
    clocc_thread_cleanup();

cleanup_argv:
#ifdef _WIN32
    if (utf8_argv) {
        for (int i = 0; i < argc; i++)
            free(utf8_argv[i]);
        free(utf8_argv);
    }
#endif
    return ret;
}

/* When building CLI-only (without GUI), this is the entry point */
#ifndef CLOCC_GUI_BUILD
int main(int argc, char **argv)
{
    return cli_main(argc, argv);
}
#endif
