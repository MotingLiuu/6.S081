#define _POSIX_C_SOURCE 200809L

#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "spinlock.h"

enum {
    WRITER_COUNT = 2,
    READER_COUNT = 10,
    OPERATIONS_PER_THREAD = 200000
};

static spinlock_t lock = SPINLOCK_INITIALIZER;
/* 普通共享变量：所有并发读写都由同一把 spinlock 保护。 */
static uint64_t data = 0;

typedef struct {
    uint64_t first;
    uint64_t last;
    int monotonic;
} reader_result_t;

static void check_pthread(int error, const char *operation)
{
    if (error != 0) {
        fprintf(stderr, "%s: %s\n", operation, strerror(error));
        exit(EXIT_FAILURE);
    }
}

static void *writer(void *arg)
{
    (void)arg;
    for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
        spin_lock(&lock);
        data++;  /* 写操作：读取、加一、写回，整个过程在锁内。 */
        spin_unlock(&lock);
    }
    return NULL;
}

static void *reader(void *arg)
{
    reader_result_t *result = arg;
    result->monotonic = 1;

    for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
        spin_lock(&lock);
        uint64_t snapshot = data;  /* 读操作也必须持锁。 */
        spin_unlock(&lock);

        /* snapshot 是本线程的局部副本，可以在解锁后处理。 */
        if (i == 0)
            result->first = snapshot;
        else if (snapshot < result->last)
            result->monotonic = 0;
        result->last = snapshot;
    }
    return NULL;
}

int main(void)
{
    struct timespec start, end;
    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        perror("clock_gettime");
        return EXIT_FAILURE;
    }

    pthread_t writers[WRITER_COUNT];
    pthread_t readers[READER_COUNT];
    reader_result_t results[READER_COUNT] = {0};

    for (int i = 0; i < READER_COUNT; ++i)
        check_pthread(pthread_create(&readers[i], NULL, reader, &results[i]),
                      "pthread_create reader");
    for (int i = 0; i < WRITER_COUNT; ++i)
        check_pthread(pthread_create(&writers[i], NULL, writer, NULL),
                      "pthread_create writer");

    for (int i = 0; i < WRITER_COUNT; ++i)
        check_pthread(pthread_join(writers[i], NULL), "pthread_join writer");
    for (int i = 0; i < READER_COUNT; ++i)
        check_pthread(pthread_join(readers[i], NULL), "pthread_join reader");

    /* 所有线程已退出，主线程现在可以直接读取 data 和 results。 */
    const uint64_t expected = (uint64_t)WRITER_COUNT * OPERATIONS_PER_THREAD;
    int ok = data == expected;
    printf("data = %" PRIu64 ", expected = %" PRIu64 "\n", data, expected);
    for (int i = 0; i < READER_COUNT; ++i) {
        printf("reader %d: first = %" PRIu64 ", last = %" PRIu64
               ", monotonic = %s\n", i, results[i].first, results[i].last,
               results[i].monotonic ? "yes" : "no");
        ok = ok && results[i].monotonic && results[i].last <= expected;
    }
    if (clock_gettime(CLOCK_MONOTONIC, &end) == -1) {
        perror("clock_gettime");
        return EXIT_FAILURE;
    }
    const double elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0
                            + (end.tv_nsec - start.tv_nsec) / 1000000.0;
    printf("Elapsed time: %.3f ms\n", elapsed_ms);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
