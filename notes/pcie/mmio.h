#ifndef MMIO_H
#define MMIO_H

#include <stdint.h>

/*
 * 所有设备寄存器访问都必须用 volatile：
 * 否则编译器会认为"读了没用"而删掉，或者合并两次写。
 */
static inline uint8_t mmio_read8(uint64_t addr)
{
    return *(volatile uint8_t *)addr;
}

static inline uint16_t mmio_read16(uint64_t addr)
{
    return *(volatile uint16_t *)addr;
}

static inline uint32_t mmio_read32(uint64_t addr)
{
    return *(volatile uint32_t *)addr;
}

static inline uint64_t mmio_read64(uint64_t addr)
{
    return *(volatile uint64_t *)addr;
}

static inline void mmio_write8(uint64_t addr, uint8_t val)
{
    *(volatile uint8_t *)addr = val;
}

static inline void mmio_write16(uint64_t addr, uint16_t val)
{
    *(volatile uint16_t *)addr = val;
}

static inline void mmio_write32(uint64_t addr, uint32_t val)
{
    *(volatile uint32_t *)addr = val;
}

static inline void mmio_write64(uint64_t addr, uint64_t val)
{
    *(volatile uint64_t *)addr = val;
}

#endif
