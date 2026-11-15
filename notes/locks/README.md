# Spinlock 学习示例

编译运行（需要支持 C11 的编译器和 POSIX threads）：

```sh
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -pthread main.c -o spinlock_demo
./spinlock_demo
```

预期输出：

```text
data = 800000, expected = 800000
```

`spinlock.h` 使用 C11 的 `atomic_flag`。它保证 lock-free，具体原子指令由编译器根据 CPU 生成，不需要绑定 x86 或 ARM 汇编。

## 抢锁时发生了什么

`atomic_flag_test_and_set_explicit()` 是不可分割的 read-modify-write：把标志设为 true，同时返回旧值。

- 旧值是 false：之前没有线程持锁，当前线程成功抢锁，退出循环。
- 旧值是 true：锁已被占用，当前线程继续自旋。

例如 A、B 同时抢一个空闲的锁：原子操作保证只有一个线程能读到旧值 false。若 A 成功，B 就反复读到 true，直到 A 解锁。这个锁不保证公平性，B 也可能输给其他竞争者。

## 为什么需要 acquire / release

互斥保证同一时刻只有一个线程进入临界区；内存序还保证临界区的数据正确交接：

```text
A: lock(acquire) → data++ → unlock(release)
                                  ↓ synchronizes-with
B:                         lock(acquire) → data++
```

成功获取锁的 acquire 操作与此前释放锁的 release 操作配对，使 A 解锁前的写入对 B 加锁后的访问可见，并约束编译器和 CPU 的重排。仅仅把标志改成原子变量、全部使用 relaxed，不能正确保护这个普通的 data。`volatile` 也不能替代原子操作或锁。

## data++ 为什么要加锁

`data++` 在语义上包含读取、加一、写回。假设 data 为 10，两个线程都读到 10，然后都写回 11，就会丢失一次更新。在 C 中，无锁并发读写这个普通变量还是 data race，属于未定义行为，因此不能依靠删除锁后“少加了几次”的实验来证明正确性。

程序启动 4 个线程，各自执行 200000 次更新；所有并发的 data 访问均使用同一把锁。主线程等待所有线程退出后检查总数。结果检查能发现计数错误，但并不单独构成并发正确性的证明。

这个实现用于学习：等待时一直消耗 CPU，频繁 test-and-set 还会造成缓存一致性流量；不保证公平，不能递归加锁。临界区应短小，避免持锁执行阻塞操作；一般用户态业务可优先使用 pthread_mutex。它没有 xv6 自旋锁的关闭中断和持锁 CPU 检查逻辑。
