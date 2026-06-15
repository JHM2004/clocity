#ifdef _WIN32

#include "clocc.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

void clocc_os_init(void)
{
    HANDLE hOut;
    DWORD mode;

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        if (GetConsoleMode(hOut, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, mode);
        }
    }
}

double clocc_os_time(void)
{
    static LARGE_INTEGER freq = {0};
    static int freq_init = 0;
    LARGE_INTEGER counter;

    if (!freq_init) {
        QueryPerformanceFrequency(&freq);
        freq_init = 1;
    }

    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
}

int clocc_os_cpu_count(void)
{
    SYSTEM_INFO info;

    GetSystemInfo(&info);
    if (info.dwNumberOfProcessors < 1)
        return 1;

    return (int)info.dwNumberOfProcessors;
}

FILE *clocc_fopen(const char *path, const char *mode)
{
    /* Convert UTF-8 path to wide string and use _wfopen */
    int wpath_len = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    if (wpath_len <= 0) return NULL;
    wchar_t *wpath = malloc((size_t)wpath_len * sizeof(wchar_t));
    if (!wpath) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wpath_len);

    int wmode_len = MultiByteToWideChar(CP_UTF8, 0, mode, -1, NULL, 0);
    wchar_t *wmode = NULL;
    FILE *fp = NULL;
    if (wmode_len > 0) {
        wmode = malloc((size_t)wmode_len * sizeof(wchar_t));
        if (wmode) {
            MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, wmode_len);
            fp = _wfopen(wpath, wmode);
            free(wmode);
        }
    }

    free(wpath);
    return fp;
}

#endif /* _WIN32 */
