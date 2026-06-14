/*
 * clocity_gui.c — Win32 GUI for clocity
 *
 * Pure Win32 API, zero external dependencies.
 * Compile with -DCLOCC_GUI_BUILD -mwindows and link:
 *   -lkernel32 -lcomdlg32 -lshell32 -lcomctl32 -lgdi32 -lole32
 */

#define UNICODE
#define _UNICODE

#include "clocc.h"

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Constants                                                         */
/* ------------------------------------------------------------------ */

#define ID_BTN_FOLDER      1001
#define ID_BTN_FILE        1002
#define ID_BTN_COUNT       1003
#define ID_BTN_EXPORT      1004
#define ID_EDIT_PATH       1005
#define ID_LIST_RESULTS    1006
#define ID_STATUS_BAR      1007
#define ID_COMBO_SORT      1008
#define ID_CHECK_EMPTY     1010
#define ID_SPIN_THREADS    1011
#define ID_EDIT_THREADS    1012

#define APP_TITLE   L"clocity \u2014 Code Line Counter"
#define APP_WIDTH   900
#define APP_HEIGHT  600

/* ------------------------------------------------------------------ */
/*  Globals                                                           */
/* ------------------------------------------------------------------ */

static HINSTANCE g_hInst;
static HWND g_hWndMain;
static HWND g_hEditPath;
static HWND g_hListResults;
static HWND g_hStatusBar;
static HWND g_hComboSort;
static HWND g_hCheckEmpty;
static HWND g_hSpinThreads;
static HWND g_hEditThreads;
static HWND g_hBtnCount;
static HWND g_hBtnExport;

static clocc_result_t g_result;
static clocc_config_t g_config;
static int g_has_result = 0;

/* ------------------------------------------------------------------ */
/*  UTF-8 / Wide conversion                                           */
/* ------------------------------------------------------------------ */

static wchar_t *utf8_to_wide_alloc(const char *s)
{
    if (!s) return NULL;
    int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (len <= 0) return NULL;
    wchar_t *w = malloc((size_t)len * sizeof(wchar_t));
    if (!w) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, len);
    return w;
}

static char *wide_to_utf8_alloc(const wchar_t *w)
{
    if (!w) return NULL;
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (len <= 0) return NULL;
    char *s = malloc((size_t)len);
    if (!s) return NULL;
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s, len, NULL, NULL);
    return s;
}

/* ------------------------------------------------------------------ */
/*  Apply font to all child windows                                   */
/* ------------------------------------------------------------------ */

static BOOL CALLBACK set_font_cb(HWND hwnd, LPARAM lParam)
{
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)lParam, TRUE);
    return TRUE;
}

/* ------------------------------------------------------------------ */
/*  Folder browser dialog                                             */
/* ------------------------------------------------------------------ */

static int browse_folder(HWND parent, wchar_t *buf, int buf_len)
{
    BROWSEINFOW bi;
    memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = parent;
    bi.lpszTitle = L"Select a folder to count code lines";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return 0;

    if (!SHGetPathFromIDListW(pidl, buf)) {
        CoTaskMemFree(pidl);
        return 0;
    }

    CoTaskMemFree(pidl);
    (void)buf_len;
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Run counting in background thread                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    char path[CLOCC_MAX_PATH];
    int sort_by;
    int exclude_empty;
    int thread_count;
} count_params_t;

static DWORD WINAPI count_thread(LPVOID param)
{
    count_params_t *p = (count_params_t *)param;

    EnableWindow(g_hBtnCount, FALSE);

    SendMessageW(g_hStatusBar, SB_SETTEXTW, 0,
                 (LPARAM)L"Scanning files...");

    clocc_lang_init();

    clocc_config_t config;
    memset(&config, 0, sizeof(config));
    config.paths = (const char **)&p->path;
    config.path_count = 1;
    config.format = CLOCC_OUTPUT_TABLE;
    config.sort_by = p->sort_by;
    config.sort_descending = 1;
    config.show_colors = 0;
    config.exclude_empty = p->exclude_empty;
    config.verbose = 0;
    config.thread_count = p->thread_count;

    clocc_thread_init(config.thread_count);

    double t_start = clocc_os_time();

    char **files = NULL;
    int file_count = 0;

    if (clocc_scan_directory(p->path, &config, &files, &file_count) != 0) {
        wchar_t msg[512];
        swprintf(msg, 512, L"Error: cannot scan path");
        SendMessageW(g_hStatusBar, SB_SETTEXTW, 0, (LPARAM)msg);
        EnableWindow(g_hBtnCount, TRUE);
        free(p);
        return 1;
    }

    config.files = (const char **)files;
    config.file_count = file_count;

    wchar_t status[256];
    swprintf(status, 256, L"Counting %d files...", file_count);
    SendMessageW(g_hStatusBar, SB_SETTEXTW, 0, (LPARAM)status);

    memset(&g_result, 0, sizeof(g_result));
    if (clocc_thread_process(&config, &g_result) != 0) {
        SendMessageW(g_hStatusBar, SB_SETTEXTW, 0,
                     (LPARAM)L"Error processing files");
        clocc_free_file_list(files, file_count);
        clocc_thread_cleanup();
        EnableWindow(g_hBtnCount, TRUE);
        free(p);
        return 1;
    }

    double t_end = clocc_os_time();
    double elapsed = t_end - t_start;

    g_has_result = 1;
    g_config = config;

    /* Update list view */
    ListView_DeleteAllItems(g_hListResults);

    int row = 0;
    for (int i = 0; i < g_result.lang_count; i++) {
        clocc_lang_result_t *lr = &g_result.languages[i];

        if (config.exclude_empty && lr->file_count == 0)
            continue;

        wchar_t wname[128];
        MultiByteToWideChar(CP_UTF8, 0, lr->name, -1, wname, 128);

        LVITEMW lvi;
        memset(&lvi, 0, sizeof(lvi));
        lvi.mask = LVIF_TEXT;
        lvi.iItem = row;
        lvi.iSubItem = 0;
        lvi.pszText = wname;
        int idx = (int)SendMessageW(g_hListResults, LVM_INSERTITEMW,
                                    0, (LPARAM)&lvi);

        wchar_t wbuf[32];
        #define SET_COL(col, val) \
            swprintf(wbuf, 32, L"%d", (val)); \
            ListView_SetItemText(g_hListResults, idx, (col), wbuf);

        SET_COL(1, lr->file_count)
        SET_COL(2, lr->blank_lines)
        SET_COL(3, lr->comment_lines)
        SET_COL(4, lr->code_lines)
        SET_COL(5, lr->mixed_lines)
        SET_COL(6, lr->total_lines)
        #undef SET_COL

        row++;
    }

    /* Add SUM row */
    {
        LVITEMW lvi;
        memset(&lvi, 0, sizeof(lvi));
        lvi.mask = LVIF_TEXT;
        lvi.iItem = row;
        lvi.iSubItem = 0;
        lvi.pszText = L"SUM";
        int idx = (int)SendMessageW(g_hListResults, LVM_INSERTITEMW,
                                    0, (LPARAM)&lvi);

        wchar_t wbuf[32];
        #define SET_COL(col, val) \
            swprintf(wbuf, 32, L"%d", (val)); \
            ListView_SetItemText(g_hListResults, idx, (col), wbuf);

        SET_COL(1, g_result.total_files)
        SET_COL(2, g_result.total_blank)
        SET_COL(3, g_result.total_comment)
        SET_COL(4, g_result.total_code)
        SET_COL(5, g_result.total_mixed)
        SET_COL(6, g_result.total_lines)
        #undef SET_COL
    }

    /* Status bar */
    swprintf(status, 256,
             L"Done: %d files, %d lines in %.3f seconds (%d languages)",
             g_result.total_files, g_result.total_lines, elapsed,
             g_result.lang_count);
    SendMessageW(g_hStatusBar, SB_SETTEXTW, 0, (LPARAM)status);

    EnableWindow(g_hBtnCount, TRUE);
    EnableWindow(g_hBtnExport, TRUE);

    clocc_free_file_list(files, file_count);
    clocc_thread_cleanup();
    free(p);

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Export results to file                                            */
/* ------------------------------------------------------------------ */

/* Helper: write output functions to a FILE* by capturing printf output */
static void write_output_to_file(FILE *fp, clocc_output_format_t fmt)
{
    /* We need to generate output without using stdout redirection.
       Instead, we'll build the output string manually. */
    const clocc_lang_result_t *langs = g_result.languages;
    int lang_count = g_result.lang_count;

    if (fmt == CLOCC_OUTPUT_JSON) {
        fprintf(fp, "{\n  \"clocity\": {\n");
        fprintf(fp, "    \"total_files\": %d,\n", g_result.total_files);
        fprintf(fp, "    \"total_lines\": %d,\n", g_result.total_lines);
        fprintf(fp, "    \"total_code\": %d,\n", g_result.total_code);
        fprintf(fp, "    \"total_comment\": %d,\n", g_result.total_comment);
        fprintf(fp, "    \"total_blank\": %d,\n", g_result.total_blank);
        fprintf(fp, "    \"total_mixed\": %d,\n", g_result.total_mixed);
        fprintf(fp, "    \"languages\": {\n");
        for (int i = 0; i < lang_count; i++) {
            fprintf(fp, "      \"%s\": {\n", langs[i].name);
            fprintf(fp, "        \"files\": %d,\n", langs[i].file_count);
            fprintf(fp, "        \"lines\": %d,\n", langs[i].total_lines);
            fprintf(fp, "        \"code\": %d,\n", langs[i].code_lines);
            fprintf(fp, "        \"comment\": %d,\n", langs[i].comment_lines);
            fprintf(fp, "        \"blank\": %d,\n", langs[i].blank_lines);
            fprintf(fp, "        \"mixed\": %d\n", langs[i].mixed_lines);
            fprintf(fp, "      }%s\n", (i < lang_count - 1) ? "," : "");
        }
        fprintf(fp, "    }\n  }\n}\n");
    } else {
        /* CSV */
        fprintf(fp, "Language,Files,Blank,Comment,Code,Mixed,Lines\n");
        for (int i = 0; i < lang_count; i++) {
            fprintf(fp, "%s,%d,%d,%d,%d,%d,%d\n",
                    langs[i].name, langs[i].file_count,
                    langs[i].blank_lines, langs[i].comment_lines,
                    langs[i].code_lines, langs[i].mixed_lines,
                    langs[i].total_lines);
        }
        fprintf(fp, "SUM,%d,%d,%d,%d,%d,%d\n",
                g_result.total_files, g_result.total_blank,
                g_result.total_comment, g_result.total_code,
                g_result.total_mixed, g_result.total_lines);
    }
}

static void export_results(void)
{
    if (!g_has_result) return;

    OPENFILENAMEW ofn;
    wchar_t szFile[MAX_PATH] = L"results";

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"JSON\0*.json\0CSV\0*.csv\0All\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"json";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameW(&ofn)) return;

    char *path_utf8 = wide_to_utf8_alloc(szFile);

    const char *ext = strrchr(path_utf8, '.');
    clocc_output_format_t fmt = CLOCC_OUTPUT_JSON;
    if (ext && clocc_str_icmp(ext, ".csv") == 0)
        fmt = CLOCC_OUTPUT_CSV;

    FILE *fp = fopen(path_utf8, "w");
    if (!fp) {
        MessageBoxW(g_hWndMain, L"Cannot write to file", L"Error",
                    MB_ICONERROR);
        free(path_utf8);
        return;
    }

    write_output_to_file(fp, fmt);
    fclose(fp);

    wchar_t msg[512];
    swprintf(msg, 512, L"Results exported to %s", szFile);
    MessageBoxW(g_hWndMain, msg, L"Export Complete", MB_ICONINFORMATION);

    free(path_utf8);
}

/* ------------------------------------------------------------------ */
/*  Create list view columns                                          */
/* ------------------------------------------------------------------ */

static void init_list_view(HWND hList)
{
    ListView_SetExtendedListViewStyle(hList,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    struct {
        const wchar_t *text;
        int width;
    } cols[] = {
        { L"Language", 140 },
        { L"Files",    60 },
        { L"Blank",    80 },
        { L"Comment",  80 },
        { L"Code",     100 },
        { L"Mixed",    70 },
        { L"Lines",    100 },
    };

    LVCOLUMNW lvc;
    memset(&lvc, 0, sizeof(lvc));
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

    for (int i = 0; i < 7; i++) {
        lvc.fmt = (i == 0) ? LVCFMT_LEFT : LVCFMT_RIGHT;
        lvc.cx = cols[i].width;
        lvc.pszText = (LPWSTR)cols[i].text;
        lvc.iSubItem = i;
        ListView_InsertColumn(hList, i, &lvc);
    }
}

/* ------------------------------------------------------------------ */
/*  Font helper                                                       */
/* ------------------------------------------------------------------ */

static HFONT create_ui_font(void)
{
    return CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

/* ------------------------------------------------------------------ */
/*  Window procedure                                                  */
/* ------------------------------------------------------------------ */

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg,
                                 WPARAM wParam, LPARAM lParam)
{
    static HFONT hFont;

    switch (msg) {

    case WM_CREATE: {
        hFont = create_ui_font();

        /* ---- Path input row ---- */
        CreateWindowW(L"STATIC", L"Path:",
                      WS_VISIBLE | WS_CHILD | SS_RIGHT,
                      10, 14, 50, 24, hwnd, NULL, g_hInst, NULL);

        g_hEditPath = CreateWindowW(L"EDIT", L"",
                      WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                      65, 12, 500, 26, hwnd, (HMENU)ID_EDIT_PATH,
                      g_hInst, NULL);

        CreateWindowW(L"BUTTON", L"Folder...",
                      WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                      575, 12, 80, 26, hwnd, (HMENU)ID_BTN_FOLDER,
                      g_hInst, NULL);

        CreateWindowW(L"BUTTON", L"File...",
                      WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                      660, 12, 70, 26, hwnd, (HMENU)ID_BTN_FILE,
                      g_hInst, NULL);

        /* ---- Options row ---- */
        int y2 = 48;

        CreateWindowW(L"STATIC", L"Sort by:",
                      WS_VISIBLE | WS_CHILD | SS_RIGHT,
                      10, y2 + 4, 50, 24, hwnd, NULL, g_hInst, NULL);

        g_hComboSort = CreateWindowW(L"COMBOBOX", L"",
                      WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
                      65, y2, 120, 200, hwnd, (HMENU)ID_COMBO_SORT,
                      g_hInst, NULL);

        const wchar_t *sort_items[] = {
            L"Code", L"Files", L"Lines", L"Comment", L"Blank", L"Mixed"
        };
        for (int i = 0; i < 6; i++) {
            SendMessageW(g_hComboSort, CB_ADDSTRING, 0,
                         (LPARAM)sort_items[i]);
        }
        SendMessageW(g_hComboSort, CB_SETCURSEL, 0, 0);

        CreateWindowW(L"STATIC", L"Threads:",
                      WS_VISIBLE | WS_CHILD | SS_RIGHT,
                      200, y2 + 4, 55, 24, hwnd, NULL, g_hInst, NULL);

        g_hEditThreads = CreateWindowW(L"EDIT", L"0",
                      WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
                      260, y2, 45, 24, hwnd, (HMENU)ID_EDIT_THREADS,
                      g_hInst, NULL);

        g_hSpinThreads = CreateWindowW(UPDOWN_CLASSW, L"",
                      WS_VISIBLE | WS_CHILD |
                      UDS_AUTOBUDDY | UDS_SETBUDDYINT |
                      UDS_ALIGNRIGHT | UDS_ARROWKEYS,
                      0, 0, 0, 0, hwnd, (HMENU)ID_SPIN_THREADS,
                      g_hInst, NULL);

        SendMessageW(g_hSpinThreads, UDM_SETRANGE32, 0, 64);
        SendMessageW(g_hSpinThreads, UDM_SETPOS32, 0, 0);

        g_hCheckEmpty = CreateWindowW(L"BUTTON", L"Exclude empty",
                      WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
                      330, y2 + 2, 120, 24, hwnd,
                      (HMENU)ID_CHECK_EMPTY, g_hInst, NULL);

        /* ---- Count button ---- */
        g_hBtnCount = CreateWindowW(L"BUTTON", L"Count",
                      WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON |
                      BS_DEFPUSHBUTTON,
                      780, y2, 90, 28, hwnd, (HMENU)ID_BTN_COUNT,
                      g_hInst, NULL);

        /* ---- Results list view ---- */
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(icex);
        icex.dwICC = ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&icex);

        g_hListResults = CreateWindowW(WC_LISTVIEWW, L"",
                      WS_VISIBLE | WS_CHILD | LVS_REPORT |
                      LVS_SINGLESEL | WS_BORDER,
                      10, y2 + 40, APP_WIDTH - 25,
                      APP_HEIGHT - y2 - 40 - 50, hwnd,
                      (HMENU)ID_LIST_RESULTS, g_hInst, NULL);

        init_list_view(g_hListResults);

        /* ---- Export button ---- */
        g_hBtnExport = CreateWindowW(L"BUTTON", L"Export...",
                      WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                      680, APP_HEIGHT - 42, 90, 28, hwnd,
                      (HMENU)ID_BTN_EXPORT, g_hInst, NULL);
        EnableWindow(g_hBtnExport, FALSE);

        /* ---- Status bar ---- */
        g_hStatusBar = CreateWindowW(STATUSCLASSNAMEW, L"Ready",
                      WS_VISIBLE | WS_CHILD | SBARS_SIZEGRIP,
                      0, 0, 0, 0, hwnd, (HMENU)ID_STATUS_BAR,
                      g_hInst, NULL);

        /* Apply font to all controls */
        EnumChildWindows(hwnd, set_font_cb, (LPARAM)hFont);

        break;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {

        case ID_BTN_FOLDER: {
            wchar_t folder[MAX_PATH];
            if (browse_folder(hwnd, folder, MAX_PATH)) {
                SetWindowTextW(g_hEditPath, folder);
            }
            break;
        }

        case ID_BTN_FILE: {
            OPENFILENAMEW ofn;
            wchar_t szFile[MAX_PATH] = L"";
            memset(&ofn, 0, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = L"All Files\0*.*\0";
            ofn.Flags = OFN_FILEMUSTEXIST;
            if (GetOpenFileNameW(&ofn)) {
                SetWindowTextW(g_hEditPath, szFile);
            }
            break;
        }

        case ID_BTN_COUNT: {
            wchar_t wpath[CLOCC_MAX_PATH];
            GetWindowTextW(g_hEditPath, wpath, CLOCC_MAX_PATH);
            if (wcslen(wpath) == 0) {
                MessageBoxW(hwnd,
                            L"Please select a folder or file first.",
                            L"clocity", MB_ICONWARNING);
                break;
            }

            char *path_utf8 = wide_to_utf8_alloc(wpath);

            count_params_t *p = malloc(sizeof(count_params_t));
            memset(p, 0, sizeof(*p));
            strncpy(p->path, path_utf8, CLOCC_MAX_PATH - 1);

            int sort_idx = (int)SendMessageW(g_hComboSort, CB_GETCURSEL,
                                             0, 0);
            clocc_sort_field_t sort_fields[] = {
                CLOCC_SORT_CODE, CLOCC_SORT_FILES, CLOCC_SORT_LINES,
                CLOCC_SORT_COMMENT, CLOCC_SORT_BLANK, CLOCC_SORT_MIXED
            };
            p->sort_by = sort_fields[sort_idx];

            p->exclude_empty = (int)SendMessageW(g_hCheckEmpty,
                                                  BM_GETCHECK, 0, 0);

            wchar_t wthreads[16];
            GetWindowTextW(g_hEditThreads, wthreads, 16);
            p->thread_count = _wtoi(wthreads);

            free(path_utf8);

            CreateThread(NULL, 0, count_thread, p, 0, NULL);
            break;
        }

        case ID_BTN_EXPORT:
            export_results();
            break;
        }
        break;

    case WM_SIZE: {
        int cx = LOWORD(lParam);
        int cy = HIWORD(lParam);
        MoveWindow(g_hListResults, 10, 88, cx - 20, cy - 88 - 50, TRUE);
        MoveWindow(g_hStatusBar, 0, cy - 24, cx, 24, TRUE);
        MoveWindow(g_hBtnExport, cx - 120, cy - 48, 100, 28, TRUE);
        break;
    }

    case WM_GETMINMAXINFO: {
        LPMINMAXINFO mmi = (LPMINMAXINFO)lParam;
        mmi->ptMinTrackSize.x = 700;
        mmi->ptMinTrackSize.y = 400;
        break;
    }

    case WM_DESTROY:
        DeleteObject(hFont);
        if (g_has_result)
            free(g_result.languages);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  WinMain — GUI entry point                                         */
/* ------------------------------------------------------------------ */

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInst;
    (void)lpCmdLine;
    g_hInst = hInst;

    clocc_os_init();

    /* If command-line arguments are given, run in CLI mode */
    int argc;
    LPWSTR *argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc > 1) {
        char **argv_utf8 = malloc((size_t)argc * sizeof(char *));
        for (int i = 0; i < argc; i++)
            argv_utf8[i] = wide_to_utf8_alloc(argvW[i]);

        LocalFree(argvW);

        /* Allocate a console for CLI output */
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);

        extern int cli_main(int argc, char **argv);
        int rc = cli_main(argc, argv_utf8);

        for (int i = 0; i < argc; i++)
            free(argv_utf8[i]);
        free(argv_utf8);
        return rc;
    }
    LocalFree(argvW);

    /* Register window class */
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"clocityWndClass";
    wc.hIconSm = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    RegisterClassExW(&wc);

    /* Create main window */
    int screen_cx = GetSystemMetrics(SM_CXSCREEN);
    int screen_cy = GetSystemMetrics(SM_CYSCREEN);
    int x = (screen_cx - APP_WIDTH) / 2;
    int y = (screen_cy - APP_HEIGHT) / 2;

    g_hWndMain = CreateWindowExW(
        0, L"clocityWndClass", APP_TITLE,
        WS_OVERLAPPEDWINDOW,
        x, y, APP_WIDTH, APP_HEIGHT,
        NULL, NULL, hInst, NULL);

    ShowWindow(g_hWndMain, nCmdShow);
    UpdateWindow(g_hWndMain);

    /* Message loop */
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
