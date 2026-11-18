#ifndef SPINLOCK_H
#define SPINLOCK_H

typedef struct {
    int locked;
} spinlock_t;

typedef struct {
    int state;
    int writer;
} rwlock_t;

#define SPINLOCK_INITIALIZER { .locked = 0 }
#define RWLOCK_INITIALIZE { .state = 0, .writer = 0}

static inline void spin_lock(spinlock_t *lock)
{
    /* 原子地写入 1，返回旧值；旧值为 0 才表示抢锁成功。 */
    while (__atomic_exchange_n(&lock->locked, 1, __ATOMIC_ACQUIRE)) {
        /* busy wait：锁被占用时不断重试，不进入睡眠。 */
    }
}

static inline void spin_unlock(spinlock_t *lock)
{
    /* 发布临界区中的写入，让下一个成功 acquire 的线程可见。 */
    __atomic_store_n(&lock->locked, 0, __ATOMIC_RELEASE);
}

static inline void r_lock(rwlock_t *lock) {
    while (1) {
        int state = __atomic_load_n(&lock->state, __ATOMIC_ACQUIRE);
        if (state == -1 || lock->writer > 0) {
            continue;
        }
        int expected = state;
        if (__atomic_compare_exchange_n(&lock->state, &expected, expected + 1, 1, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE)) {
            break;
        }
    }
}

static inline void w_lock(rwlock_t *lock) {
    __atomic_fetch_add(&lock->writer, 1, __ATOMIC_ACQ_REL);
    while (1) {
        int expected = 0;
        if (__atomic_compare_exchange_n(&lock->state, &expected, -1, 1, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE)) {
            break;
        }
    }
}

static inline void w_unlock(rwlock_t *lock) {
    __atomic_fetch_sub(&lock->writer, 1, __ATOMIC_ACQ_REL);
    __atomic_store_n(&lock->state, 0, __ATOMIC_RELEASE);
}

static inline void r_unlock(rwlock_t *lock) {
    __atomic_fetch_sub(&lock->state, 1, __ATOMIC_RELEASE);
}

#endif
