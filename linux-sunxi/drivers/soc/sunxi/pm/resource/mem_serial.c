/*
 * drivers/soc/sunxi/pm/resource/mem_serial.c
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <linux/io.h>
#include "mem_serial.h"

static int get_uart_port(void)
{
	static int uart_port = -1;
	int i;
	unsigned int reg;

	if (uart_port < 0) {
		reg = readl(CCMU_REG_BASE + CCMU_UART_BGR_OFFSET);
		for (i = 0; i < UART_PORT_MAX; i++)
			if (reg & (0x1 << (CCMU_UART_GATING_OFFSET + i)))
				uart_port = i;
	}

	return uart_port;
}

static void serial_clk_init(int uart_port)
{
	unsigned int reg;
	int i;

	/* reset */
	reg = readl(CCMU_REG_BASE + CCMU_UART_BGR_OFFSET);
	reg &= ~(1 << (CCMU_UART_RST_OFFSET + uart_port));
	writel(reg, CCMU_REG_BASE + CCMU_UART_BGR_OFFSET);

	for (i = 0; i < 100; i++)
		;

	reg |= (1 << (CCMU_UART_RST_OFFSET + uart_port));
	writel(reg, CCMU_REG_BASE + CCMU_UART_BGR_OFFSET);

	/* gate */
	reg = readl(CCMU_REG_BASE + CCMU_UART_BGR_OFFSET);
	reg &= ~(1 << (CCMU_UART_GATING_OFFSET + uart_port));
	writel(reg, CCMU_REG_BASE + CCMU_UART_BGR_OFFSET);

	for (i = 0; i < 100; i++)
		;

	reg |=  (1 << (CCMU_UART_GATING_OFFSET + uart_port));
	writel(reg, CCMU_REG_BASE + CCMU_UART_BGR_OFFSET);
}

static void serial_hw_init(int uart_port)
{
	unsigned int lcr;
	unsigned int clk;
	void *uart_reg_base = UART_REG_BASE(uart_port);

	clk = (24000000 + (UART_BAUDRATE << 3)) / (UART_BAUDRATE << 4);

	/* set DLL and DLM */
	lcr = readl(uart_reg_base + UART_LCR_OFFSET);
	writel(lcr | 0x80, uart_reg_base + UART_LCR_OFFSET);

	/* set baudrate */
	writel((clk >> 8) & 0xff, uart_reg_base + UART_DLH_OFFSET);
	writel(clk & 0xff, uart_reg_base + UART_DLL_OFFSET);

	/* set RBR, THR and IER */
	writel(lcr & (~0x80), uart_reg_base + UART_LCR_OFFSET);

	/* set mode, Set Lin Control Register */
	writel(0x3, uart_reg_base + UART_LCR_OFFSET);

	/* enable fifo */
	writel(0xe1, uart_reg_base + UART_FCR_OFFSET);
}

int serial_init(void)
{
	int uart_port = get_uart_port();

	/*
	 * we should not init uart gpio
	 */

	serial_clk_init(uart_port);

	serial_hw_init(uart_port);

	return 0;
}
