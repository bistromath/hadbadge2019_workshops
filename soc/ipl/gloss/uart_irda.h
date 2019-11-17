#pragma once

void uart_irda_putchar(char c);
void uart_irda_write(const char *buf, int len);
int uart_irda_getchar();
