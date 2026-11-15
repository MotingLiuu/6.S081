#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdatomic.h>

typedef struct {
    atomic_flag locked;
} spinlock_t;

#define SPINLOCK_INITIALIZER { .locked = ATOMIC_FLAG_INIT }

static inline void spin_lock(spinlock_t *lock)
{
    /* 原子地写入 true，返回旧值；旧值为 false 才表示抢锁成功。 */
    while (atomic_flag_test_and_set_explicit(&lock->locked,
                                            memory_order_acquire)) {
        /* busy wait：锁被占用时不断重试，不进入睡眠。 */
    }
}

static inline void spin_unlock(spinlock_t *lock)
{
    /* 发布临界区中的写入，让下一个成功 acquire 的线程可见。 */
    atomic_flag_clear_explicit(&lock->locked, memory_order_release);
}

#endif
