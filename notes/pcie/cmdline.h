#ifndef CMDLINE_H
#define CMDLINE_H

#include <stdint.h>

/*
 * 实验选择。
 *
 * QEMU 的 -append 字符串会进入设备树 /chosen/bootargs，例如：
 *
 *   -append "exp=5"
 *   -append "exp=4,5"
 *   -append "exp=1-3"
 *   -append "exp=all"  /  "exp=none"
 *
 * cmdline_init() 从 FDT 里把它取出来并解析成位图。
 */
#define EXP_MAX 6
#define EXP_ALL_MASK 0x7eu          /* bit1..bit6 */

void cmdline_init(uint64_t fdt);
int  exp_enabled(int n);
const char *cmdline_expr(void);

#endif
