/*
 * drivers/soc/sunxi/pm/standby/standby_serial.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef __STANDBY_SERIAL_H__
#define __STANDBY_SERIAL_H__

#ifdef CONFIG_ARCH_SUN8IW8P1
#define SUART_BASE_PA	(SUNXI_UART2_PBASE)
#elif defined(CONFIG_ARCH_SUN8IW10P1)
#define SUART_BASE_PA	(AW_UART0_PBASE)
#else
#define SUART_BASE_PA	(SUNXI_UART0_PBASE)
#endif

#define SUART_BASE_VA	IO_ADDRESS(SUART_BASE_PA)

#define SUART_RBR_PA	(SUART_BASE_PA + 0x00)
#define SUART_THR_PA	(SUART_BASE_PA + 0x00)
#define SUART_DLL_PA	(SUART_BASE_PA + 0x00)
#define SUART_DLH_PA	(SUART_BASE_PA + 0x04)
#define SUART_FCR_PA	(SUART_BASE_PA + 0x08)
#define SUART_LCR_PA	(SUART_BASE_PA + 0x0c)
#define SUART_USR_PA	(SUART_BASE_PA + 0x7c)
#define SUART_HALT_PA	(SUART_BASE_PA + 0xa4)

#define SUART_RBR	(SUART_BASE_VA + 0x00)
#define SUART_THR	(SUART_BASE_VA + 0x00)
#define SUART_DLL	(SUART_BASE_VA + 0x00)
#define SUART_DLH	(SUART_BASE_VA + 0x04)
#define SUART_FCR	(SUART_BASE_VA + 0x08)
#define SUART_LCR	(SUART_BASE_VA + 0x0c)
#define SUART_USR	(SUART_BASE_VA + 0x7c)
#define SUART_HALT	(SUART_BASE_VA + 0xa4)

#define SUART_BAUDRATE	(115200)

void serial_init(__u32 port_id);
void serial_exit(void);
void serial_init_manager(void);
void serial_exit_manager(void);
__s32 serial_puts(const char *string);
__u32 serial_gets(char *buf, __u32 n);
void serial_init_nommu(__u32 port_id);
__s32 serial_puts_nommu(const char *string);
__u32 serial_gets_nommu(char *buf, __u32 n);

#endif				/* __STANDBY_SERIAL_H__ */
