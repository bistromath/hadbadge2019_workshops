/*
 * Copyright 2019 Jeroen Domburg <jeroen@spritesmods.com>
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <stdint.h>
#include "mach_defines.h"
#include "uart_irda.h"

extern volatile uint32_t UART[];
#define UARTREG(i) UART[i/4]

void uart_irda_putchar(char c) {
//	while (!(UARTREG(UART_FLAG_REG)&UART_FLAG_TXDONE)) ;
//	if (c=='\n') uart_irda_putchar('\r');
	UARTREG(UART_IRDA_DATA_REG)=c;
}

void uart_irda_write(const char *buf, int len) {
	while(len) {
		uart_irda_putchar(*buf++);
		len--;
	}
}

int uart_irda_getchar() {
    uint32_t uartval;
    do {
        uartval = UARTREG(UART_IRDA_DATA_REG);
    } while(uartval & (1<<31)); //while FIFO empty
    return uartval;
}
