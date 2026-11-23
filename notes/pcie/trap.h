#ifndef TRAP_H
#define TRAP_H

#include <stdint.h>

extern volatile int trap_expected;
extern volatile int trap_taken;
extern volatile uint64_t trap_cause;
extern volatile uint64_t trap_epc;
extern volatile uint64_t trap_addr;

void trap_init(void);
const char *trap_name(uint64_t cause);

/*
 * 尝试读一个物理地址。如果这条 lw 触发访问异常，
 * 陷阱处理程序会跳过它，函数返回 -1 而不是让机器挂死。
 */
int try_read32(uint64_t addr, uint32_t *out);

#endif
