#ifndef TULIP_H
#define TULIP_H

#include <stdint.h>

/*
 * DEC 21140 (Tulip) 的寄存器块（CSR，Control and Status Register）。
 * 这些寄存器在网卡芯片内部，只能通过 BAR 指到的那段地址访问。
 * 偏移量来自 21140 数据手册：CSR n 在 n*8 处，每个寄存器占 8 字节。
 */
#define TULIP_CSR(n) (0x08 * (n))

#define TULIP_CSR0  0x00   /* 总线模式 / 软复位 */
#define TULIP_CSR5  0x28   /* 状态 */
#define TULIP_CSR6  0x30   /* 操作模式（收发使能） */
#define TULIP_CSR7  0x38   /* 中断屏蔽 */
#define TULIP_CSR9  0x48   /* 引导 ROM / MII / 串行 ROM */
#define TULIP_CSR11 0x58   /* 通用定时器 */

uint32_t tulip_read(uint32_t base, int csr);
void     tulip_write(uint32_t base, int csr, uint32_t val);

#endif
