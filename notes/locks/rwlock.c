#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "spinlock.h"

#include <stdlib.h>

#ifdef __linux__
#include <errno.h>
#include <limits.h>
#include <linux/futex.h>
#include <stdatomic.h>
#include <sys/syscall.h>
#include <unistd.h>

/*
 * Bits 0–15 count active readers. Bits 16–29 count queued writers.
 * Bit 30 marks an active writer. Bit 31 records readers that may be asleep.
 * Writer registration and reader admission use CAS on this same word.
 */
static const uint32_t reader_mask = UINT32_C(0xffff);
static const uint32_t writer_wait_one = UINT32_C(1) << 16;
static const uint32_t writer_wait_mask = UINT32_C(0x3fff) << 16;
static const uint32_t writer_active = UINT32_C(1) << 30;
static const uint32_t reader_waiting = UINT32_C(1) << 31;

enum { SPIN_LIMIT = 64, READER_QUEUE = 1, WRITER_QUEUE = 2 };

_Static_assert(sizeof(uint32_t) == 4, "Futex requires a 32-bit word");
_Static_assert(_Generic((uint32_t)0, unsigned int: ATOMIC_INT_LOCK_FREE,
                        unsigned long: ATOMIC_LONG_LOCK_FREE,
                        unsigned short: ATOMIC_SHORT_LOCK_FREE, default: 0) == 2,
               "Futex state must support lock-free atomics");

static void cpu_relax(void)
{
#if defined(__i386__) || defined(__x86_64__)
    __asm__ __volatile__("pause");
#elif defined(__aarch64__)
    __asm__ __volatile__("yield");
#else
    __atomic_signal_fence(__ATOMIC_ACQ_REL);
#endif
}

static void spin_or_wait(rwlock_t *lock, uint32_t state, unsigned int *spins,
                         unsigned int queue)
{
    if (*spins < SPIN_LIMIT) {
        ++*spins;
        cpu_relax();
        return;
    }

    /* The kernel checks the observed value before atomically parking us. */
    int saved_errno = errno;
    long result = syscall(SYS_futex, &lock->state, FUTEX_WAIT_BITSET_PRIVATE,
                          state, (void *)0, (void *)0, queue);
    if (result == -1 && errno != EAGAIN && errno != EINTR)
        abort();
    errno = saved_errno;
}

static void wake(rwlock_t *lock, int count, unsigned int queue)
{
    int saved_errno = errno;
    if (syscall(SYS_futex, &lock->state, FUTEX_WAKE_BITSET_PRIVATE, count,
                (void *)0, (void *)0, queue) == -1)
        abort();
    errno = saved_errno;
}

void r_lock(rwlock_t *lock)
{
    unsigned int spins = 0;
    for (;;) {
        uint32_t state = __atomic_load_n(&lock->state, __ATOMIC_RELAXED);
        if ((state & (writer_active | writer_wait_mask)) == 0) {
            if ((state & reader_mask) == reader_mask)
                abort();
            if (__atomic_compare_exchange_n(&lock->state, &state, state + 1,
                                            1, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
                return;
            continue;
        }

        if (spins == SPIN_LIMIT && (state & reader_waiting) == 0) {
            if (!__atomic_compare_exchange_n(&lock->state, &state,
                                             state | reader_waiting, 1,
                                             __ATOMIC_RELAXED, __ATOMIC_RELAXED))
                continue;
            state |= reader_waiting;
        }
        spin_or_wait(lock, state, &spins, READER_QUEUE);
    }
}

void w_lock(rwlock_t *lock)
{
    uint32_t state = 0;
    if (__atomic_compare_exchange_n(&lock->state, &state, writer_active,
                                    0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
        return;

    /* Once registered, this writer prevents new readers from entering. */
    do {
        if ((state & writer_wait_mask) == writer_wait_mask)
            abort();
    } while (!__atomic_compare_exchange_n(&lock->state, &state,
                                           state + writer_wait_one, 1,
                                           __ATOMIC_RELAXED, __ATOMIC_RELAXED));

    unsigned int spins = 0;
    for (;;) {
        state = __atomic_load_n(&lock->state, __ATOMIC_RELAXED);
        if ((state & (writer_active | reader_mask)) == 0) {
            uint32_t next = (state - writer_wait_one) | writer_active;
            if (__atomic_compare_exchange_n(&lock->state, &state, next,
                                            1, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
                return;
            continue;
        }
        spin_or_wait(lock, state, &spins, WRITER_QUEUE);
    }
}

void r_unlock(rwlock_t *lock)
{
    uint32_t state = __atomic_fetch_sub(&lock->state, 1, __ATOMIC_RELEASE);
    if ((state & reader_mask) == 1 && (state & writer_wait_mask) != 0)
        wake(lock, 1, WRITER_QUEUE);
}

void w_unlock(rwlock_t *lock)
{
    uint32_t state = __atomic_load_n(&lock->state, __ATOMIC_RELAXED);
    uint32_t next;
    do {
        next = state & ~writer_active;
        if ((state & writer_wait_mask) == 0)
            next &= ~reader_waiting;
    } while (!__atomic_compare_exchange_n(&lock->state, &state, next,
                                           1, __ATOMIC_RELEASE, __ATOMIC_RELAXED));

    /* Publish the unlocked state before making the selected queue runnable. */
    if ((state & writer_wait_mask) != 0)
        wake(lock, 1, WRITER_QUEUE);
    else if ((state & reader_waiting) != 0)
        wake(lock, INT_MAX, READER_QUEUE);
}

void rwlock_destroy(rwlock_t *lock)
{
    (void)lock;
}
#else
void r_lock(rwlock_t *lock)
{
    if (pthread_rwlock_rdlock(lock) != 0)
        abort();
}

void w_lock(rwlock_t *lock)
{
    if (pthread_rwlock_wrlock(lock) != 0)
        abort();
}

void r_unlock(rwlock_t *lock)
{
    if (pthread_rwlock_unlock(lock) != 0)
        abort();
}

void w_unlock(rwlock_t *lock)
{
    if (pthread_rwlock_unlock(lock) != 0)
        abort();
}

void rwlock_destroy(rwlock_t *lock)
{
    if (pthread_rwlock_destroy(lock) != 0)
        abort();
}
#endif
