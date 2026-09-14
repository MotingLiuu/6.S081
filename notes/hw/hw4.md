1. what goes wrong? 

replace 
```c
  while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
    ;
```
by
```c
 while(*(volatile int *) &lk->locked != 0)
    ;
  lk->locked = 1;
```

```txt

xv6 kernel is booting

hart 1 starting
hart 2 starting
panic: release
```

This happens because `hart 1` and `hart 2` knows that `lk->locked == 0` simultaneously. Assume that `hart1` call `lk->cpu = mycpu()` first, `lk->cpu = 1`. `hart2` call `lk->cpu = mycpu()` second, `lk->cpu = 2`. If `hart1` call `release` first, it would check whether `hart1` holding the `lk`, the result is false. `panic`
