#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "spinlock.h"

enum {
    WRITER_COUNT = 2,
    READER_COUNT = 10,
    OPERATIONS_PER_THREAD = 20000,
    INCREMENT = 10,
    TIMEOUT_SECONDS = 5
};

typedef struct {
    int use_rwlock;
    unsigned int read_work;
    spinlock_t spinlock;
    rwlock_t rwlock;
    pthread_mutex_t start_mutex;
    pthread_cond_t start_cond;
    int started;
    /* 即使锁实现有缺陷，测试中的共享数据访问仍然是原子的。 */
    _Atomic uint64_t data;
    _Atomic uint64_t mirror;
} benchmark_t;

typedef struct {
    benchmark_t *benchmark;
    uint64_t first;
    uint64_t last;
    uint64_t checksum;
    int monotonic;
    unsigned int mismatches;
} reader_result_t;

static void check_pthread(int error, const char *operation)
{
    if (error != 0) {
        fprintf(stderr, "%s: %s\n", operation, strerror(error));
        exit(EXIT_FAILURE);
    }
}

static void timeout_handler(int signal_number)
{
    (void)signal_number;
    static const char message[] = "Timed out waiting for the benchmark to finish.\n";
    (void)write(STDERR_FILENO, message, sizeof(message) - 1);
    _exit(124);
}

static void wait_for_start(benchmark_t *benchmark)
{
    check_pthread(pthread_mutex_lock(&benchmark->start_mutex), "pthread_mutex_lock");
    while (!benchmark->started)
        check_pthread(pthread_cond_wait(&benchmark->start_cond, &benchmark->start_mutex),
                      "pthread_cond_wait");
    check_pthread(pthread_mutex_unlock(&benchmark->start_mutex), "pthread_mutex_unlock");
}

static void *writer(void *arg)
{
    benchmark_t *benchmark = arg;
    wait_for_start(benchmark);

    for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
        if (benchmark->use_rwlock)
            w_lock(&benchmark->rwlock);
        else
            spin_lock(&benchmark->spinlock);

        uint64_t next = atomic_load_explicit(&benchmark->data, memory_order_relaxed)
                        + INCREMENT;
        atomic_store_explicit(&benchmark->data, next, memory_order_relaxed);
        atomic_store_explicit(&benchmark->mirror, next, memory_order_relaxed);

        if (benchmark->use_rwlock)
            w_unlock(&benchmark->rwlock);
        else
            spin_unlock(&benchmark->spinlock);
    }
    return NULL;
}

static void *reader(void *arg)
{
    reader_result_t *result = arg;
    benchmark_t *benchmark = result->benchmark;
    wait_for_start(benchmark);
    result->monotonic = 1;

    for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
        if (benchmark->use_rwlock)
            r_lock(&benchmark->rwlock);
        else
            spin_lock(&benchmark->spinlock);

        uint64_t snapshot = atomic_load_explicit(&benchmark->data, memory_order_relaxed);
        uint64_t computed = 0;
        if (benchmark->read_work != 0) {
            /* 线程局部变量让持锁计算在 -O2 下仍逐轮执行。 */
            volatile uint64_t value = snapshot + (uint64_t)i;
            for (unsigned int j = 0; j < benchmark->read_work; ++j)
                value = value * UINT64_C(6364136223846793005) + 1;
            computed = value;
        }
        uint64_t mirror = atomic_load_explicit(&benchmark->mirror, memory_order_relaxed);

        if (benchmark->use_rwlock)
            r_unlock(&benchmark->rwlock);
        else
            spin_unlock(&benchmark->spinlock);

        result->checksum ^= computed;
        if (i == 0)
            result->first = snapshot;
        if (snapshot != mirror)
            ++result->mismatches;
        if (snapshot < result->last)
            result->monotonic = 0;
        result->last = snapshot;
    }
    return NULL;
}

static struct timespec now(void)
{
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time) == -1) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return time;
}

static int run_benchmark(int use_rwlock, unsigned int read_work)
{
    benchmark_t benchmark = {
        .use_rwlock = use_rwlock,
        .read_work = read_work,
        .spinlock = SPINLOCK_INITIALIZER,
        .rwlock = RWLOCK_INITIALIZE
    };
    check_pthread(pthread_mutex_init(&benchmark.start_mutex, NULL), "pthread_mutex_init");
    check_pthread(pthread_cond_init(&benchmark.start_cond, NULL), "pthread_cond_init");

    printf("%s (%d readers, %d writers, %d operations per thread, %u work iterations per read)\n",
           use_rwlock ? "rwlock" : "spinlock",
           READER_COUNT, WRITER_COUNT, OPERATIONS_PER_THREAD, read_work);
    fflush(stdout);
    alarm(TIMEOUT_SECONDS);

    pthread_t writers[WRITER_COUNT];
    pthread_t readers[READER_COUNT];
    reader_result_t results[READER_COUNT] = {0};
    for (int i = 0; i < READER_COUNT; ++i) {
        results[i].benchmark = &benchmark;
        check_pthread(pthread_create(&readers[i], NULL, reader, &results[i]),
                      "pthread_create reader");
    }
    for (int i = 0; i < WRITER_COUNT; ++i)
        check_pthread(pthread_create(&writers[i], NULL, writer, &benchmark),
                      "pthread_create writer");

    check_pthread(pthread_mutex_lock(&benchmark.start_mutex), "pthread_mutex_lock");
    struct timespec start = now();
    benchmark.started = 1;
    check_pthread(pthread_cond_broadcast(&benchmark.start_cond), "pthread_cond_broadcast");
    check_pthread(pthread_mutex_unlock(&benchmark.start_mutex), "pthread_mutex_unlock");

    for (int i = 0; i < WRITER_COUNT; ++i)
        check_pthread(pthread_join(writers[i], NULL), "pthread_join writer");
    for (int i = 0; i < READER_COUNT; ++i)
        check_pthread(pthread_join(readers[i], NULL), "pthread_join reader");

    struct timespec end = now();
    alarm(0);
    rwlock_destroy(&benchmark.rwlock);
    check_pthread(pthread_cond_destroy(&benchmark.start_cond), "pthread_cond_destroy");
    check_pthread(pthread_mutex_destroy(&benchmark.start_mutex), "pthread_mutex_destroy");

    const uint64_t expected = (uint64_t)WRITER_COUNT * OPERATIONS_PER_THREAD * INCREMENT;
    uint64_t data = atomic_load_explicit(&benchmark.data, memory_order_relaxed);
    int ok = data == expected
             && atomic_load_explicit(&benchmark.mirror, memory_order_relaxed) == expected;
    uint64_t checksum = 0;
    printf("data = %" PRIu64 ", expected = %" PRIu64 "\n", data, expected);
    for (int i = 0; i < READER_COUNT; ++i) {
        checksum ^= results[i].checksum;
        printf("reader %d: first = %" PRIu64 ", last = %" PRIu64
               ", monotonic = %s, mismatches = %u\n",
               i, results[i].first, results[i].last,
               results[i].monotonic ? "yes" : "no", results[i].mismatches);
        ok = ok && results[i].monotonic && results[i].mismatches == 0
             && results[i].last <= expected;
    }
    if (read_work != 0)
        printf("Read workload checksum: %016" PRIx64 "\n", checksum);

    double elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0
                        + (end.tv_nsec - start.tv_nsec) / 1000000.0;
    printf("Elapsed time: %.3f ms, result: %s\n", elapsed_ms, ok ? "PASS" : "FAIL");
    return ok;
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "both";
    int run_spin = strcmp(mode, "rwlock") != 0;
    int run_rw = strcmp(mode, "spinlock") != 0;
    if (argc > 3 || (run_spin && run_rw && strcmp(mode, "both") != 0)) {
        fprintf(stderr, "Usage: %s [both|spinlock|rwlock [read_work]]\n", argv[0]);
        return EXIT_FAILURE;
    }

    unsigned int read_work = 0;
    if (argc == 3) {
        char *end;
        errno = 0;
        unsigned long value = strtoul(argv[2], &end, 10);
        if (argv[2][0] < '0' || argv[2][0] > '9' || *end != '\0'
            || errno == ERANGE || value > UINT_MAX) {
            fprintf(stderr, "read_work must be an integer from 0 to %u\n", UINT_MAX);
            return EXIT_FAILURE;
        }
        read_work = (unsigned int)value;
    }

    struct sigaction action = { .sa_handler = timeout_handler };
    if (sigemptyset(&action.sa_mask) == -1 || sigaction(SIGALRM, &action, NULL) == -1) {
        perror("install timeout handler");
        return EXIT_FAILURE;
    }

    int spin_ok = !run_spin || run_benchmark(0, read_work);
    int rw_ok = !run_rw || run_benchmark(1, read_work);
    return spin_ok && rw_ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
