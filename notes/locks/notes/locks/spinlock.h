#ifndef SPINLOCK_H
#define SPINLOCK_H

typedef struct {
    int locked;
} spinlock_t;

#define SPINLOCK_INITIALIZER { .locked = 0 }

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

#ifdef __linux__
#include <stdint.h>

typedef struct {
    _Alignas(4) uint32_t state;
} rwlock_t;

#define RWLOCK_INITIALIZE { .state = 0 }
#else
#include <pthread.h>

typedef pthread_rwlock_t rwlock_t;

#define RWLOCK_INITIALIZE PTHREAD_RWLOCK_INITIALIZER
#endif

void r_lock(rwlock_t *lock);
void w_lock(rwlock_t *lock);
void r_unlock(rwlock_t *lock);
void w_unlock(rwlock_t *lock);

/* Call only after all users, including waiters, have left the lock. */
void rwlock_destroy(rwlock_t *lock);

#endif
