#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "spinlock.h"

enum { THREAD_COUNT = 4, INCREMENTS_PER_THREAD = 200000 };

static spinlock_t lock = SPINLOCK_INITIALIZER;
/* data 本身不是原子变量；并发访问统一由 lock 保护。 */
static uint64_t data = 0;

static void check_pthread(int error, const char *operation)
{
    if (error != 0) {
        fprintf(stderr, "%s: %s\n", operation, strerror(error));
        exit(EXIT_FAILURE);
    }
}

static void *worker(void *arg)
{
    (void)arg;
    for (int i = 0; i < INCREMENTS_PER_THREAD; ++i) {
        spin_lock(&lock);
        /* 临界区：读取 data、加一、写回，必须作为整体受到保护。 */
        data++;
        spin_unlock(&lock);
    }
    return NULL;
}

int main(void)
{
    pthread_t threads[THREAD_COUNT];

    for (int i = 0; i < THREAD_COUNT; ++i)
        check_pthread(pthread_create(&threads[i], NULL, worker, NULL),
                      "pthread_create");

    for (int i = 0; i < THREAD_COUNT; ++i)
        check_pthread(pthread_join(threads[i], NULL), "pthread_join");

    /* join 完成后工作线程均已退出，此时读取 data 不需要再加锁。 */
    const uint64_t expected = (uint64_t)THREAD_COUNT * INCREMENTS_PER_THREAD;
    printf("data = %" PRIu64 ", expected = %" PRIu64 "\n", data, expected);
    return data == expected ? EXIT_SUCCESS : EXIT_FAILURE;
}
