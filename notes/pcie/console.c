#include <stdarg.h>
#include <stdint.h>
#include "console.h"

/* QEMU -M virt 的串口是 16550A，寄存器按字节落在 0x10000000。 */
#define UART_BASE 0x10000000UL
#define UART_THR  0
#define UART_LSR  5
#define LSR_THRE  0x20

static void uart_putc_raw(char c)
{
    volatile uint8_t *u = (volatile uint8_t *)UART_BASE;
    while ((u[UART_LSR] & LSR_THRE) == 0)
        ;
    u[UART_THR] = (uint8_t)c;
}

void uart_putc(char c)
{
    if (c == '\n')
        uart_putc_raw('\r');
    uart_putc_raw(c);
}

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

#define BUF_SIZE 2048

static void put(char *buf, int *i, char c)
{
    if (*i < BUF_SIZE - 1)
        buf[(*i)++] = c;
}

static void put_str(char *buf, int *i, const char *s, int n, int width, int left)
{
    int pad = width > n ? width - n : 0;
    if (!left)
        for (int k = 0; k < pad; k++)
            put(buf, i, ' ');
    for (int k = 0; k < n; k++)
        put(buf, i, s[k]);
    if (left)
        for (int k = 0; k < pad; k++)
            put(buf, i, ' ');
}

static void put_num(char *buf, int *i, uint64_t v, int base, int width,
                    int zero, int left, int neg)
{
    const char *digits = "0123456789abcdef";
    char tmp[32];
    int n = 0;

    if (v == 0)
        tmp[n++] = '0';
    while (v) {
        tmp[n++] = digits[v % base];
        v /= base;
    }
    if (neg)
        tmp[n++] = '-';

    int pad = width > n ? width - n : 0;
    if (!left && !zero)
        for (int k = 0; k < pad; k++)
            put(buf, i, ' ');
    if (neg)
        put(buf, i, '-');
    if (!left && zero)
        for (int k = 0; k < pad; k++)
            put(buf, i, '0');
    for (int k = n - 1; k >= 0; k--) {
        if (tmp[k] == '-')
            continue;
        put(buf, i, tmp[k]);
    }
    if (left)
        for (int k = 0; k < pad; k++)
            put(buf, i, ' ');
}

void kprintf(const char *fmt, ...)
{
    va_list ap;
    char buf[BUF_SIZE];
    int oi = 0;

    va_start(ap, fmt);
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            put(buf, &oi, *p);
            continue;
        }
        p++;

        int left = 0, zero = 0, width = 0, is_long = 0;
        for (;; p++) {
            if (*p == '-')
                left = 1;
            else if (*p == '0')
                zero = 1;
            else
                break;
        }
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }
        if (*p == 'l') {
            is_long = 1;
            p++;
        }
        if (width > 200)
            width = 200;

        switch (*p) {
        case 'd': {
            int64_t sv = is_long ? va_arg(ap, int64_t) : va_arg(ap, int);
            int neg = sv < 0;
            uint64_t uv = neg ? (uint64_t)(-sv) : (uint64_t)sv;
            put_num(buf, &oi, uv, 10, width, zero, left, neg);
            break;
        }
        case 'u':
        case 'x': {
            uint64_t uv = is_long ? va_arg(ap, uint64_t)
                                  : va_arg(ap, unsigned int);
            put_num(buf, &oi, uv, *p == 'x' ? 16 : 10, width, zero, left, 0);
            break;
        }
        case 'p': {
            uint64_t uv = (uint64_t)(uintptr_t)va_arg(ap, void *);
            put(buf, &oi, '0');
            put(buf, &oi, 'x');
            put_num(buf, &oi, uv, 16, 16, 1, 0, 0);
            break;
        }
        case 'c':
            put(buf, &oi, (char)va_arg(ap, int));
            break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s)
                s = "(null)";
            int n = 0;
            while (s[n])
                n++;
            put_str(buf, &oi, s, n, width, left);
            break;
        }
        case '%':
            put(buf, &oi, '%');
            break;
        default:
            put(buf, &oi, '%');
            put(buf, &oi, *p);
            break;
        }
    }
    va_end(ap);

    buf[oi] = 0;
    uart_puts(buf);
}

void hexdump(const void *data, int len)
{
    const uint8_t *p = (const uint8_t *)data;

    for (int i = 0; i < len; i += 16) {
        kprintf("  %04x:", i);
        for (int j = 0; j < 16; j++) {
            if (j % 4 == 0)
                kprintf(" ");
            if (i + j < len)
                kprintf(" %02x", p[i + j]);
            else
                kprintf("   ");
        }
        kprintf("  ");
        for (int j = 0; j < 16 && i + j < len; j++) {
            uint8_t c = p[i + j];
            kprintf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        kprintf("\n");
    }
}
