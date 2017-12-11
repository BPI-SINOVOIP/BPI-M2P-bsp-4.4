/*
 * drivers/soc/sunxi/pm/resource/mem_serial.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef __MEM_SERIAL_H__
#define __MEM_SERIAL_H__

#define UART_BAUDRATE	(115200)
#define UART_PORT_MAX	(4)

#define CCMU_REG_BASE			((void *)(0xf3001000))
#define CCMU_UART_BGR_OFFSET		(0x90c)
/* internal offset of register */
#define CCMU_UART_GATING_OFFSET		(0)
#define CCMU_UART_RST_OFFSET		(16)

#define UART_REG_BASE(uart_port)	((void *)0xf5000000 + 0x400 * uart_port)
#define UART_DLL_OFFSET			(0x00)
#define UART_DLH_OFFSET			(0x04)
#define UART_FCR_OFFSET			(0x08)
#define UART_LCR_OFFSET			(0x0c)

int serial_init(void);
#endif				/* __MEM_SERIAL_H__ */
