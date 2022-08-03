/*
 * console.c
 *
 * Copyright (C) 2019-2021 Sylvain Munaut
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdint.h>

#include "config.h"
#include "mini-printf.h"


struct sifive_uart {
	uint32_t tx;
	uint32_t rx;
} __attribute__((packed,aligned(4)));

static volatile struct sifive_uart * const uart_regs = (void*)(UART_BASE);


void
console_init(void)
{
	// nothing yet
}

void
console_putchar(char c)
{
	while (uart_regs->tx < 0);
	uart_regs->tx = c;
}

char
console_getchar(void)
{
	int32_t c;
	do {
		c = uart_regs->rx;
	} while (c <= 0);
	return c;
}

int
console_getchar_nowait(void)
{
	int32_t c = uart_regs->rx;
	return c > 0 ? (c & 0xff) : -1;
}

void
console_puts(const char *p)
{
	char c;
	while ((c = *(p++)) != 0x00) {
		while (uart_regs->tx < 0);
		uart_regs->tx = c;
	}
}

int
console_printf(const char *fmt, ...)
{
	static char _printf_buf[128];
        va_list va;
        int l;

        va_start(va, fmt);
        l = mini_vsnprintf(_printf_buf, 128, fmt, va);
        va_end(va);

	console_puts(_printf_buf);

	return l;
}
