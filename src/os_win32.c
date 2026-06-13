#ifdef _WIN32

#include "clocc.h"
#include <windows.h>

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

#endif /* _WIN32 */
