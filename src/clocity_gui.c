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
#define ID_BTN_CANCEL      1013
#define ID_BTN_BACK        1014

#define WM_PROGRESS        (WM_USER + 100)
#define WM_COUNT_DONE      (WM_USER + 101)

#define VIEW_SUMMARY  0
#define VIEW_DETAIL   1

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

/* Detail view state */
static int g_current_view = VIEW_SUMMARY;
static int g_detail_lang_index = -1;  /* which language row was clicked */
static HWND g_hBtnBack;

/* ------------------------------------------------------------------ */
/*  GUI progress callback (called from worker threads)                */
/* ------------------------------------------------------------------ */

static void gui_progress(int phase, int done, int total, void *user_data)
{
    (void)user_data;
    HWND hwnd = g_hWndMain;
    if (!hwnd) return;

    /* Pack phase + done + total into wParam/lParam for PostMessage */
    /* wParam = phase, lParam = MAKELONG(done, total) */
    PostMessageW(hwnd, WM_PROGRESS, (WPARAM)phase,
                 MAKELPARAM(done, total));
}

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

    /* Free previous results if any */
    if (g_has_result && g_result.languages) {
        free(g_result.languages);
        g_result.languages = NULL;
    }
    if (g_has_result && g_result.file_results) {
        free(g_result.file_results);
        g_result.file_results = NULL;
        g_result.file_result_count = 0;
    }
    g_has_result = 0;

    PostMessageW(g_hWndMain, WM_PROGRESS, 0, MAKELPARAM(0, 0));

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
    config.progress_cb = gui_progress;
    config.progress_data = NULL;

    clocc_thread_init(config.thread_count);

    double t_start = clocc_os_time();

    char **files = NULL;
    int file_count = 0;

    if (clocc_scan_directory(p->path, &config, &files, &file_count) != 0) {
        PostMessageW(g_hWndMain, WM_COUNT_DONE, 0,
                     MAKELPARAM(0, 0));  /* error */
        clocc_thread_cleanup();
        free(p);
        return 1;
    }

    config.files = (const char **)files;
    config.file_count = file_count;

    memset(&g_result, 0, sizeof(g_result));
    if (clocc_thread_process(&config, &g_result) != 0) {
        PostMessageW(g_hWndMain, WM_COUNT_DONE, 0,
                     MAKELPARAM(0, 0));  /* error */
        clocc_free_file_list(files, file_count);
        clocc_thread_cleanup();
        free(p);
        return 1;
    }

    double t_end = clocc_os_time();
    double elapsed = t_end - t_start;

    g_has_result = 1;
    g_config = config;

    /* Notify main thread to update UI — pass elapsed time as pointer */
    double *elapsed_ptr = malloc(sizeof(double));
    if (elapsed_ptr) *elapsed_ptr = elapsed;

    clocc_free_file_list(files, file_count);
    clocc_thread_cleanup();
    free(p);

    PostMessageW(g_hWndMain, WM_COUNT_DONE, 1,
                 (LPARAM)elapsed_ptr);  /* success */
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Export results to file                                            */
/* ------------------------------------------------------------------ */

/* Helper: write output functions to a FILE* by capturing printf output */
static void write_output_to_file(FILE *fp, clocc_output_format_t fmt)
{
    /* Use the same output functions as the CLI */
    clocc_config_t export_config;
    memset(&export_config, 0, sizeof(export_config));
    export_config.sort_by = g_config.sort_by;
    export_config.sort_descending = g_config.sort_descending;
    export_config.exclude_empty = g_config.exclude_empty;

    if (fmt == CLOCC_OUTPUT_JSON) {
        clocc_output_json_fp(&g_result, &export_config, fp);
    } else {
        clocc_output_csv_fp(&g_result, &export_config, fp);
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

    FILE *fp = clocc_fopen(path_utf8, "w");
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

/* Set up list view columns for file detail view */
static void init_list_view_detail(HWND hList)
{
    ListView_SetExtendedListViewStyle(hList,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    struct {
        const wchar_t *text;
        int width;
    } cols[] = {
        { L"File",     400 },
        { L"Blank",     80 },
        { L"Comment",   80 },
        { L"Code",     100 },
        { L"Mixed",     70 },
        { L"Lines",    100 },
    };

    LVCOLUMNW lvc;
    memset(&lvc, 0, sizeof(lvc));
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

    for (int i = 0; i < 6; i++) {
        lvc.fmt = (i == 0) ? LVCFMT_LEFT : LVCFMT_RIGHT;
        lvc.cx = cols[i].width;
        lvc.pszText = (LPWSTR)cols[i].text;
        lvc.iSubItem = i;
        ListView_InsertColumn(hList, i, &lvc);
    }
}

/* Remove all columns from list view */
static void clear_list_view_columns(HWND hList)
{
    while (ListView_DeleteColumn(hList, 0))
        ;
}

/* Show summary view (per-language) */
static void show_summary_view(void)
{
    g_current_view = VIEW_SUMMARY;
    g_detail_lang_index = -1;

    ShowWindow(g_hBtnBack, SW_HIDE);

    ListView_DeleteAllItems(g_hListResults);
    clear_list_view_columns(g_hListResults);
    init_list_view(g_hListResults);

    if (!g_has_result) return;

    int row = 0;
    for (int i = 0; i < g_result.lang_count; i++) {
        clocc_lang_result_t *lr = &g_result.languages[i];

        if (g_config.exclude_empty && lr->file_count == 0)
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
}

/* Show detail view for a specific language */
static void show_detail_view(int lang_idx)
{
    if (!g_has_result || lang_idx < 0 || lang_idx >= g_result.lang_count)
        return;

    g_current_view = VIEW_DETAIL;
    g_detail_lang_index = lang_idx;

    ShowWindow(g_hBtnBack, SW_SHOW);

    const char *lang_name = g_result.languages[lang_idx].name;

    ListView_DeleteAllItems(g_hListResults);
    clear_list_view_columns(g_hListResults);
    init_list_view_detail(g_hListResults);

    /* Update window title */
    wchar_t wtitle[256];
    wchar_t wlang[128];
    MultiByteToWideChar(CP_UTF8, 0, lang_name, -1, wlang, 128);
    swprintf(wtitle, 256, L"%s files", wlang);
    SetWindowTextW(g_hWndMain, wtitle);

    /* Populate with per-file results for this language */
    if (!g_result.file_results || g_result.file_result_count <= 0) return;

    int row = 0;
    for (int i = 0; i < g_result.file_result_count; i++) {
        clocc_file_result_t *fr = &g_result.file_results[i];

        /* Check if this file belongs to the selected language */
        int match = 0;
        if (fr->is_binary || fr->lang_index < 0) {
            /* Binary/unknown — match by extension name */
            if (fr->ext[0]) {
                /* Compare uppercase extension with lang_name */
                char upper_ext[16];
                int k = 0;
                const char *e = fr->ext;
                while (*e && k < 15) {
                    char c = *e;
                    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
                    upper_ext[k++] = c;
                    e++;
                }
                upper_ext[k] = '\0';
                if (clocc_str_icmp(upper_ext, lang_name) == 0)
                    match = 1;
            } else {
                if (clocc_str_icmp("Other", lang_name) == 0)
                    match = 1;
            }
        } else {
            const clocc_lang_t *lang = clocc_lang_get(fr->lang_index);
            if (lang && clocc_str_icmp(lang->name, lang_name) == 0)
                match = 1;
        }
        if (!match) continue;

        /* Extract just the filename from the path */
        const char *display_path = fr->path;
        const char *slash1 = strrchr(fr->path, '\\');
        const char *slash2 = strrchr(fr->path, '/');
        const char *last_slash = NULL;
        if (slash1 && slash2) last_slash = (slash1 > slash2) ? slash1 : slash2;
        else if (slash1) last_slash = slash1;
        else if (slash2) last_slash = slash2;
        if (last_slash) display_path = last_slash + 1;

        wchar_t wpath[512];
        MultiByteToWideChar(CP_UTF8, 0, display_path, -1, wpath, 512);

        LVITEMW lvi;
        memset(&lvi, 0, sizeof(lvi));
        lvi.mask = LVIF_TEXT;
        lvi.iItem = row;
        lvi.iSubItem = 0;
        lvi.pszText = wpath;
        int idx = (int)SendMessageW(g_hListResults, LVM_INSERTITEMW,
                                    0, (LPARAM)&lvi);
        if (idx < 0) continue;

        wchar_t wbuf[32];
        #define SET_COL(col, val) \
            swprintf(wbuf, 32, L"%d", (val)); \
            ListView_SetItemText(g_hListResults, idx, (col), wbuf);

        SET_COL(1, fr->blank_lines)
        SET_COL(2, fr->comment_lines)
        SET_COL(3, fr->code_lines)
        SET_COL(4, fr->mixed_lines)
        SET_COL(5, fr->code_lines + fr->comment_lines +
                   fr->blank_lines + fr->mixed_lines)
        #undef SET_COL

        row++;
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

        /* ---- Back button (hidden by default, shown in detail view) ---- */
        g_hBtnBack = CreateWindowW(L"BUTTON", L"\u2190 Back",
                      WS_CHILD | BS_PUSHBUTTON,
                      10, APP_HEIGHT - 42, 90, 28, hwnd,
                      (HMENU)ID_BTN_BACK, g_hInst, NULL);

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

            EnableWindow(g_hBtnCount, FALSE);
            CreateThread(NULL, 0, count_thread, p, 0, NULL);
            break;
        }

        case ID_BTN_EXPORT:
            export_results();
            break;

        case ID_BTN_BACK:
            show_summary_view();
            SetWindowTextW(g_hWndMain, APP_TITLE);
            break;
        }
        break;

    case WM_NOTIFY: {
        NMHDR *nmh = (NMHDR *)lParam;
        if (nmh->idFrom == ID_LIST_RESULTS &&
            nmh->code == NM_DBLCLK &&
            g_current_view == VIEW_SUMMARY &&
            g_has_result) {
            NMITEMACTIVATE *nmia = (NMITEMACTIVATE *)lParam;
            int sel = nmia->iItem;
            if (sel >= 0) {
                /* Map the selected row to the language index
                   (skipping empty rows if exclude_empty is set) */
                int lang_idx = 0;
                int visible_row = 0;
                for (int i = 0; i < g_result.lang_count; i++) {
                    if (g_config.exclude_empty &&
                        g_result.languages[i].file_count == 0)
                        continue;
                    if (visible_row == sel) {
                        lang_idx = i;
                        break;
                    }
                    visible_row++;
                }
                /* Don't open detail for SUM row */
                if (visible_row == sel && lang_idx < g_result.lang_count) {
                    show_detail_view(lang_idx);
                }
            }
        }
        break;
    }

    case WM_PROGRESS: {
        int phase = (int)wParam;
        int done = LOWORD(lParam);
        int total = HIWORD(lParam);
        wchar_t status[256];
        if (phase == 0) {
            swprintf(status, 256, L"Scanning... %d files found", done);
        } else {
            int pct = (total > 0) ? (done * 100 / total) : 0;
            swprintf(status, 256, L"Counting [%d%%] %d/%d files",
                     pct, done, total);
        }
        SendMessageW(g_hStatusBar, SB_SETTEXTW, 0, (LPARAM)status);
        break;
    }

    case WM_COUNT_DONE: {
        int success = (int)wParam;
        double *elapsed_ptr = (double *)lParam;
        double elapsed = elapsed_ptr ? *elapsed_ptr : 0.0;
        free(elapsed_ptr);

        EnableWindow(g_hBtnCount, TRUE);

        if (!success || !g_has_result) {
            SendMessageW(g_hStatusBar, SB_SETTEXTW, 0,
                         (LPARAM)L"Error: failed to count files");
            break;
        }

        EnableWindow(g_hBtnExport, TRUE);

        /* Update list view with summary */
        show_summary_view();

        /* Status bar */
        wchar_t status[256];
        swprintf(status, 256,
                 L"Done: %d files, %d lines in %.3f seconds (%d languages)",
                 g_result.total_files, g_result.total_lines, elapsed,
                 g_result.lang_count);
        SendMessageW(g_hStatusBar, SB_SETTEXTW, 0, (LPARAM)status);
        break;
    }

    case WM_SIZE: {
        int cx = LOWORD(lParam);
        int cy = HIWORD(lParam);
        MoveWindow(g_hListResults, 10, 88, cx - 20, cy - 88 - 50, TRUE);
        MoveWindow(g_hStatusBar, 0, cy - 24, cx, 24, TRUE);
        MoveWindow(g_hBtnExport, cx - 120, cy - 48, 100, 28, TRUE);
        MoveWindow(g_hBtnBack, 10, cy - 48, 90, 28, TRUE);
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
        if (g_has_result) {
            free(g_result.languages);
            free(g_result.file_results);
        }
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
