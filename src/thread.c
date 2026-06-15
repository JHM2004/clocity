#include "clocc.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

/* ── Thread pool state ─────────────────────────────────────────────── */

static clocc_file_result_t *task_queue = NULL;
static int task_count;
static int task_next;

static clocc_file_result_t *results = NULL;
static int results_count;
static int results_cap;

#ifdef _WIN32
static CRITICAL_SECTION queue_lock;
static HANDLE queue_sem;       /* semaphore: counts available tasks */
static HANDLE done_event;      /* auto-reset event: signaled when all done */
#else
static pthread_mutex_t queue_lock;
static pthread_cond_t queue_cv;
static pthread_cond_t done_cv;
#endif

static int stop_flag;
static int total_tasks;

#ifdef _WIN32
static HANDLE thread_handles[CLOCC_MAX_THREADS];
#else
static pthread_t thread_ids[CLOCC_MAX_THREADS];
#endif

static int active_threads;

/* Progress callback (set before processing starts) */
static clocc_progress_cb g_progress_cb;
static void *g_progress_data;
static int g_total_for_progress;
static volatile long g_progress_last_report;

/* ── Worker thread ─────────────────────────────────────────────────── */

#ifdef _WIN32
static unsigned __stdcall worker(void *arg)
#else
static void *worker(void *arg)
#endif
{
    (void)arg;

    for (;;) {
#ifdef _WIN32
        /* Wait for a task to be available */
        WaitForSingleObject(queue_sem, INFINITE);

        EnterCriticalSection(&queue_lock);
#else
        pthread_mutex_lock(&queue_lock);

        while (task_next >= task_count && !stop_flag) {
            pthread_cond_wait(&queue_cv, &queue_lock);
        }
#endif

        if (stop_flag) {
#ifdef _WIN32
            LeaveCriticalSection(&queue_lock);
#else
            pthread_mutex_unlock(&queue_lock);
#endif
            break;
        }

        if (task_next >= task_count) {
#ifdef _WIN32
            LeaveCriticalSection(&queue_lock);
#else
            pthread_mutex_unlock(&queue_lock);
#endif
            continue;
        }

        int idx = task_next++;
        clocc_file_result_t task = task_queue[idx];

#ifdef _WIN32
        LeaveCriticalSection(&queue_lock);
#else
        pthread_mutex_unlock(&queue_lock);
#endif

        /* Process the file */
        clocc_file_result_t file_res;
        memset(&file_res, 0, sizeof(file_res));
        file_res.path = task.path;
        file_res.lang_index = task.lang_index;
        file_res.is_binary = task.is_binary;

        if (task.is_binary) {
            /* Binary/unknown file — no line counting needed */
        } else {
            if (clocc_count_file(task.path, task.lang_index, &file_res) != 0) {
                file_res.is_binary = 1;
                file_res.lang_index = -1;
            }
        }

        /* Store result under lock */
#ifdef _WIN32
        EnterCriticalSection(&queue_lock);
#else
        pthread_mutex_lock(&queue_lock);
#endif

        if (results_count >= results_cap) {
            results_cap = results_cap == 0 ? 256 : results_cap * 2;
            clocc_file_result_t *tmp = realloc(results,
                (size_t)results_cap * sizeof(clocc_file_result_t));
            if (tmp) results = tmp;
        }
        if (results_count < results_cap) {
            results[results_count++] = file_res;
        }

        int done = (results_count >= total_tasks);

        /* Invoke progress callback (throttled: ~1% steps) */
        clocc_progress_cb cb = g_progress_cb;
        if (cb) {
            int current = results_count;
            int tot = g_total_for_progress;
            int step = (tot > 200) ? (tot / 100) : 1;
            if (step < 1) step = 1;
            long last = g_progress_last_report;
            if (current >= last + step || current >= tot) {
#ifdef _WIN32
                InterlockedExchange(&g_progress_last_report, current);
#else
                __sync_lock_test_and_set(&g_progress_last_report, current);
#endif
                void *ud = g_progress_data;
#ifdef _WIN32
                LeaveCriticalSection(&queue_lock);
                cb(1, current, tot, ud);
#else
                cb(1, current, tot, ud);
                pthread_mutex_unlock(&queue_lock);
#endif
            } else {
#ifdef _WIN32
                LeaveCriticalSection(&queue_lock);
#else
                pthread_mutex_unlock(&queue_lock);
#endif
            }
        } else {
#ifdef _WIN32
            LeaveCriticalSection(&queue_lock);
#else
            pthread_mutex_unlock(&queue_lock);
#endif
        }

        if (done) {
#ifdef _WIN32
            SetEvent(done_event);
#else
            pthread_cond_signal(&done_cv);
#endif
        }
    }

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* ── Public API ────────────────────────────────────────────────────── */

int clocc_thread_init(int thread_count)
{
    if (thread_count <= 0)
        thread_count = clocc_os_cpu_count();

    if (thread_count > CLOCC_MAX_THREADS)
        thread_count = CLOCC_MAX_THREADS;

    if (thread_count < 1)
        thread_count = 1;

    /* Reset state */
    task_count = 0;
    task_next = 0;
    results_count = 0;
    results_cap = 0;
    stop_flag = 0;
    total_tasks = 0;
    active_threads = thread_count;

    /* Allocate task queue and results */
    task_queue = malloc((size_t)CLOCC_MAX_FILES *
                        sizeof(clocc_file_result_t));
    if (!task_queue) {
        active_threads = 0;
        return -1;
    }
    results = NULL; /* dynamically grown */

    /* Initialize synchronization primitives */
#ifdef _WIN32
    InitializeCriticalSection(&queue_lock);
    queue_sem = CreateSemaphoreW(NULL, 0, CLOCC_MAX_FILES, NULL);
    done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!queue_sem || !done_event) {
        free(task_queue);
        DeleteCriticalSection(&queue_lock);
        active_threads = 0;
        return -1;
    }
#else
    pthread_mutex_init(&queue_lock, NULL);
    pthread_cond_init(&queue_cv, NULL);
    pthread_cond_init(&done_cv, NULL);
#endif

    /* Create worker threads */
    for (int i = 0; i < thread_count; i++) {
#ifdef _WIN32
        thread_handles[i] = (HANDLE)_beginthreadex(
            NULL, 0, worker, NULL, 0, NULL);
        if (thread_handles[i] == NULL) {
            stop_flag = 1;
            /* Release sem so threads can exit */
            for (int j = 0; j < i; j++)
                ReleaseSemaphore(queue_sem, 1, NULL);
            for (int j = 0; j < i; j++)
                WaitForSingleObject(thread_handles[j], INFINITE);
            CloseHandle(queue_sem);
            CloseHandle(done_event);
            DeleteCriticalSection(&queue_lock);
            free(task_queue);
            active_threads = 0;
            return -1;
        }
#else
        if (pthread_create(&thread_ids[i], NULL, worker, NULL) != 0) {
            stop_flag = 1;
            pthread_cond_broadcast(&queue_cv);
            for (int j = 0; j < i; j++)
                pthread_join(thread_ids[j], NULL);
            pthread_mutex_destroy(&queue_lock);
            pthread_cond_destroy(&queue_cv);
            pthread_cond_destroy(&done_cv);
            free(task_queue);
            active_threads = 0;
            return -1;
        }
#endif
    }

    return 0;
}

void clocc_thread_cleanup(void)
{
    if (active_threads <= 0)
        return;

    stop_flag = 1;

#ifdef _WIN32
    /* Release semaphore to wake all threads */
    ReleaseSemaphore(queue_sem, (LONG)active_threads, NULL);

    for (int i = 0; i < active_threads; i++)
        WaitForSingleObject(thread_handles[i], INFINITE);

    for (int i = 0; i < active_threads; i++)
        CloseHandle(thread_handles[i]);

    CloseHandle(queue_sem);
    CloseHandle(done_event);
    DeleteCriticalSection(&queue_lock);
#else
    pthread_mutex_lock(&queue_lock);
    pthread_cond_broadcast(&queue_cv);
    pthread_mutex_unlock(&queue_lock);

    for (int i = 0; i < active_threads; i++)
        pthread_join(thread_ids[i], NULL);

    pthread_mutex_destroy(&queue_lock);
    pthread_cond_destroy(&queue_cv);
    pthread_cond_destroy(&done_cv);
#endif

    free(task_queue);
    task_queue = NULL;
    free(results);
    results = NULL;
    active_threads = 0;
}

int clocc_thread_process(clocc_config_t *config, clocc_result_t *result)
{
    if (!config || !result)
        return -1;

    int file_count = config->file_count;
    const char **files = config->files;

    /* Store progress callback for workers */
    g_progress_cb = config->progress_cb;
    g_progress_data = config->progress_data;
    g_total_for_progress = file_count;
    g_progress_last_report = 0;

    if (file_count <= 0 || !files) {
        /* No files to process — return empty result, not an error */
        memset(result, 0, sizeof(*result));
        return 0;
    }

    /* Sequential fallback for single thread */
    if (active_threads <= 1) {
        results_count = 0;
        results_cap = 0;
        free(results);
        results = NULL;

        for (int i = 0; i < file_count; i++) {
            const char *ext = clocc_get_extension(files[i]);
            int lang_idx = -1;
            if (ext)
                lang_idx = clocc_lang_by_extension(ext);
            if (lang_idx < 0 && ext && !clocc_is_binary_ext(ext))
                lang_idx = clocc_lang_by_shebang(files[i]);

            clocc_file_result_t file_res;
            memset(&file_res, 0, sizeof(file_res));
            file_res.path = files[i];

            if (lang_idx < 0) {
                /* Binary or unrecognized file — count only */
                file_res.lang_index = -1;
                file_res.is_binary = 1;
            } else {
                file_res.lang_index = lang_idx;
                if (clocc_count_file(files[i], lang_idx, &file_res) != 0) {
                    /* Count failed — treat as binary */
                    file_res.lang_index = -1;
                    file_res.is_binary = 1;
                    file_res.code_lines = 0;
                    file_res.comment_lines = 0;
                    file_res.blank_lines = 0;
                    file_res.mixed_lines = 0;
                }
            }

            if (results_count >= results_cap) {
                results_cap = results_cap == 0 ? 256
                                               : results_cap * 2;
                results = realloc(results,
                    (size_t)results_cap *
                    sizeof(clocc_file_result_t));
            }
            if (results) {
                results[results_count++] = file_res;
            }

            /* Progress callback for sequential path */
            if (config->progress_cb) {
                config->progress_cb(1, results_count, file_count,
                                    config->progress_data);
            }
        }

        return clocc_aggregate_results(results, results_count, result);
    }

    /* ── Parallel path ─────────────────────────────────────────────── */

#ifdef _WIN32
    EnterCriticalSection(&queue_lock);
#else
    pthread_mutex_lock(&queue_lock);
#endif

    task_count = 0;
    task_next = 0;
    results_count = 0;
    total_tasks = 0;

    for (int i = 0; i < file_count && i < CLOCC_MAX_FILES; i++) {
        const char *ext = clocc_get_extension(files[i]);
        int lang_idx = -1;
        if (ext)
            lang_idx = clocc_lang_by_extension(ext);
        if (lang_idx < 0)
            lang_idx = clocc_lang_by_shebang(files[i]);

        memset(&task_queue[task_count], 0,
               sizeof(clocc_file_result_t));
        task_queue[task_count].path = files[i];

        if (lang_idx < 0) {
            /* Binary or unrecognized — mark for counting only */
            task_queue[task_count].lang_index = -1;
            task_queue[task_count].is_binary = 1;
        } else {
            task_queue[task_count].lang_index = lang_idx;
        }
        task_count++;
    }

    total_tasks = task_count;

#ifdef _WIN32
    LeaveCriticalSection(&queue_lock);

    /* Release semaphore to wake workers */
    ReleaseSemaphore(queue_sem, (LONG)task_count, NULL);

    /* Wait for all tasks to complete */
    WaitForSingleObject(done_event, INFINITE);
#else
    pthread_cond_broadcast(&queue_cv);
    pthread_mutex_unlock(&queue_lock);

    /* Wait for all tasks to complete */
    pthread_mutex_lock(&queue_lock);
    while (results_count < total_tasks) {
        pthread_cond_wait(&done_cv, &queue_lock);
    }
    pthread_mutex_unlock(&queue_lock);
#endif

    /* Aggregate per-file results into per-language totals */
    return clocc_aggregate_results(results, results_count, result);
}
