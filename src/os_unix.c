#ifndef _WIN32

#include "clocc.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <locale.h>

void clocc_os_init(void)
{
    if (setlocale(LC_ALL, "C.UTF-8") == NULL) {
        setlocale(LC_ALL, "en_US.UTF-8");
    }
}

double clocc_os_time(void)
{
#ifdef CLOCK_MONOTONIC
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
    }
#endif
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0) {
        return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
    }
    return 0.0;
}

int clocc_os_cpu_count(void)
{
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count <= 0) {
        return 1;
    }
    return (int)count;
}

FILE *clocc_fopen(const char *path, const char *mode)
{
    return fopen(path, mode);
}

#endif /* _WIN32 */
