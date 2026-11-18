# Spinlock 读写示例

使用 GCC / Clang 的 `__atomic` 内建函数实现 spinlock，使用 pthread 创建线程：

```sh
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -pthread main.c -o spinlock_demo
./spinlock_demo
```

`spinlock.h` 用普通 `int` 保存锁状态：0 表示空闲，1 表示占用。

- 加锁：`__atomic_exchange_n(&lock->locked, 1, __ATOMIC_ACQUIRE)` 原子地写入 1 并返回旧值。旧值为 0 表示成功，否则继续自旋。
- 解锁：`__atomic_store_n(&lock->locked, 0, __ATOMIC_RELEASE)`。release 与后续成功加锁的 acquire 配对，使临界区中的更新对后续持锁线程可见。

`main.c` 启动 2 个写线程和 2 个读线程，每个线程执行 200000 次操作：

- 写线程加锁后执行 `data++`，然后解锁。
- 读线程加锁后把 `data` 复制到局部变量 `snapshot`，然后解锁，在锁外处理快照。
- 主线程 join 所有线程后，检查最终计数和每个读线程观察到的数值是否单调不减。

最终计数应为 `data = 400000, expected = 400000`，每个读线程的 `monotonic` 应为 `yes`。首次和最后一次读到的数值由调度决定，读线程可能在写线程完成前结束，也可能只观察到同一个值。检查结果不能单独构成并发正确性的证明。

读写使用同一把独占锁，因此读和读之间也互斥。这不是读写锁。普通共享变量 `data` 的所有并发访问都必须持锁；线程退出后主线程通过 join 完成同步，可以直接读取最终结果。`snapshot` 是局部副本，解锁后仍然有效，但可能已经不是 data 的最新值。

不能把获取锁拆成独立的“检查 0”和“写入 1”，即使各自使用原子操作也无法保证整体原子性。不能用 volatile 替代原子操作。此锁忙等、不保证公平、不能递归加锁，持锁期间应避免打印或其他阻塞操作。
