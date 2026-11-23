# fork() Behavior Summary — `fork_e1` & `fork_e2`

A side-by-side study of two minimal `fork()` programs. The only difference between them is whether `printf` includes a newline (`\n`) — but that one character completely changes how stdio buffering interacts with `fork()`.

---

## The Programs

### `fork_e1.c`

```c
#include <unistd.h>
#include <stdio.h>

int main() {
    fork();
    printf("Hello\n");
}
```

### `fork_e2.c`

```c
#include <unistd.h>
#include <stdio.h>

int main() {
    fork();
    printf("Hello");
}
```

| File         | Source line              | Line-buffered on TTY? |
|--------------|--------------------------|------------------------|
| `fork_e1.c`  | `printf("Hello\n");`     | ✅ yes — `\n` flushes |
| `fork_e2.c`  | `printf("Hello");`       | ❌ no — stays in buffer until process exits |

---

## Execution Trace — Shared

Both programs follow the same process tree. The **only** divergence is what each process emits to stdout.

### 🟢 Step 1 — Initial state

```
         ┌────────┐
         │  P0    │  (single parent process)
         └────────┘
```
**Process count: 1**

### 🔵 Step 2 — `fork()` creates P1

`fork()` returns:
- In P0 (parent): the **PID of the child**
- In P1 (child): **0**

Both processes continue execution from the same point — the line right after `fork()`.

```
              fork()
        ┌────────┴────────┐
     ┌──▼──┐          ┌───▼───┐
     │ P0  │          │  P1   │   (copy of P0's address space)
     │parent│         │ child │
     └─────┘          └───────┘
```
**Process count: 2**

### 🟣 Step 3 — `printf(...)` runs in BOTH processes

Because the instructions after `fork()` exist in **both** processes, each one executes `printf` independently. The output goes to the same stdout (fd 1), but the buffer state diverges by program:

#### `fork_e1.c` — `printf("Hello\n")`

Each `\n` forces a flush (when line-buffered), so the kernel sees the writes right away.

```
   ┌────────┐   write()   ┌────────┐
   │   P0   │────────────▶│ stdout │
   │ "Hello\n"            │ buffer │
   └────────┘             └────────┘

   ┌────────┐   write()
   │   P1   │────────────▶
   │ "Hello\n"
   └────────┘
```

**Order on terminal:**
```
Hello
Hello
```

#### `fork_e2.c` — `printf("Hello")` (no `\n`)

Without `\n`, stdio keeps `"Hello"` in the userspace buffer of **each process**. Each process has its own independent buffer (because `fork()` duplicated the address space).

```
   ┌────────┐  buf="Hello"   ┌────────┐
   │   P0   │───────────────▶│ kernel │
   │  (not  │               │  sees  │
   │flushed)│               │nothing │
   └────────┘               │ yet    │
                            │        │
   ┌────────┐  buf="Hello"   │        │
   │   P1   │───────────────▶│        │
   │  (not  │               │        │
   │flushed)│               │        │
   └────────┘               └────────┘
```

The buffers only flush when each process calls `_exit()` (or `exit()`) at the end of `main`. The actual order on the terminal then depends on which process the kernel schedules to exit first.

---

## Actual Runtime Output

Run from this directory:

| Binary   | Source        | Output                                 |
|----------|---------------|----------------------------------------|
| `./e1`   | `fork_e1.c`   | `Hello` followed by `Hello` (2 lines)  |
| `./e2`   | `fork_e2.c`   | `HelloHello` (concatenated)            |

The exact interleave of the two writes is **non-deterministic** — the OS scheduler decides which of P0 or P1 runs first after `fork()` returns. With `\n`, you always get two lines. Without `\n`, the two `"Hello"`s appear concatenated, in some scheduler-dependent order.

---

## The fork-and-buffer gotcha (preview)

The programs above are intentionally simple so the 2-vs-2 result is easy to verify. **If the `fork()` call appears inside a loop**, the picture changes drastically:

```c
for (int i = 0; i < 2; i++) {
    fork();
    printf("Hello");      // no \n — fully buffered to a file
}
```

Trace:

| Iteration | Processes before `fork()` | After `fork()` | `printf` calls this iter |
|-----------|----------------------------|----------------|--------------------------|
| `i = 0`   | 1                          | 2              | 2                        |
| `i = 1`   | 2                          | 4              | 4                        |

Total `printf` invocations: **6**. But because `fork()` duplicates the **entire address space**, including the stdio buffer, children inherit any unflushed data. When each process eventually exits and flushes:

- Each of the 4 processes ends up with `"HelloHello"` in its buffer (2 inherited + 2 new from its own printf calls). Actually only P0 and P1 did the first printf, so P0/P1 buffers are `"HelloHello"`, while P2/P3 inherit `"Hello"` from their parents and then add one more `"Hello"` → `"HelloHello"`.
- Flushing all four yields **8** total `"Hello"`s.

This is why "How many times does `printf` get called?" (6) is a different question from "How many times does the kernel see a write?" (8 when stdout is fully buffered).

---

## Takeaways

1. **`fork()` duplicates everything in the address space**, including stdio buffers. Two processes after `fork()` have independent copies of any buffered data.
2. **A newline is not decorative** — `printf("Hello\n")` and `printf("Hello")` behave very differently when combined with `fork()`. The first flushes per call (line-buffered on a TTY); the second may flush only at process exit.
3. **Output ordering is non-deterministic**. Only the *count* is deterministic for fixed code.
4. **To avoid the inherited-buffer pitfall**, `fflush(stdout)` before `fork()`, or use unbuffered I/O via `write(STDOUT_FILENO, ...)`. xv6's `printf` is already unbuffered (writes go straight through `write()`), which sidesteps the issue entirely.

---

## Quick Reference

```
fork_e1.c (printf with \n)
   ├── 2 processes
   ├── 2 printf calls
   └── terminal output: "Hello\nHello\n"

fork_e2.c (printf without \n)
   ├── 2 processes
   ├── 2 printf calls
   └── terminal output: "HelloHello"  (order may interleave)

Loop variant (for i<2) with printf("Hello\n")
   ├── 4 processes
   ├── 6 printf calls
   └── 6 lines of "Hello\n"

Loop variant (for i<2) with printf("Hello") + full buffering
   ├── 4 processes
   ├── 6 printf calls
   └── 8 "Hello"s (2 inherited via copied buffer)
```