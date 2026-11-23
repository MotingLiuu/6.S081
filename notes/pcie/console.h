#ifndef CONSOLE_H
#define CONSOLE_H

void uart_putc(char c);
void uart_puts(const char *s);
void kprintf(const char *fmt, ...);
void hexdump(const void *data, int len);

#endif
