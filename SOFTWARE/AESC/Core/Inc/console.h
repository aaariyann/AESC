#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

/* USART1 PB6/PB7 is reserved for a future console implementation. */
void console_init(void);
void console_task(void);
void console_puts(const char *text);

#endif
