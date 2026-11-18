#ifndef SPINLOCK_H
#define SPINLOCK_H

typedef struct {
    int locked;
} spinlock_t;

typedef struct {
    int wlocked;
    int rcount;
} rwlock_t;

#define SPINLOCK_INITIALIZER { .locked = 0 }
#define RWLOCK_INITIALIZE { .wlocked = 0, .rcount = 0}

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

static inline void w_lock(rwlock_t *lock) {
    while (__atomic_exchange_n(&lock->wlocked, 1, __ATOMIC_ACQUIRE))
        ;
    int expected = 0;
    while (__atomic_compare_exchange_n(&lock->rcount, &expected, -1, 1, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE))
        ;
}

static inline void r_lock(rwlock_t *lock) {
    while (__atomic_load_n(&lock->wlocked, __ATOMIC_ACQUIRE) == 1)
        ;
    int curreader;
    while ((curreader = __atomic_exchange_n(&lock->rcount, -1, __ATOMIC_ACQUIRE)) == -1)
        ;
    __atomic_exchange_n(&lock->rcount, curreader + 1, __ATOMIC_ACQUIRE);
}

static inline void w_unlock(rwlock_t *lock) {
    __atomic_store_n(&lock->wlocked, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&lock->rcount, 0, __ATOMIC_RELEASE);
}

static inline void r_unlock(rwlock_t *lock) {
    __atomic_fetch_sub(&lock->rcount, 1, __ATOMIC_RELEASE);
}

#endif
