/*
 * drivers/soc/sunxi/pm/standby/standby.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef _STANDBY_H
#define _STANDBY_H

#ifndef IO_ADDRESS
#define IO_ADDRESS(x)		((x) + 0xf0000000)
#endif

#define readb(addr)		(*((volatile unsigned char  *)(addr)))
#define readw(addr)		(*((volatile unsigned short *)(addr)))
#define readl(addr)		(*((volatile unsigned long  *)(addr)))

#define writeb(v, addr)		\
	(*((volatile unsigned char  *)(addr)) = (unsigned char)(v))
#define writew(v, addr)		\
	(*((volatile unsigned short *)(addr)) = (unsigned short)(v))
#define writel(v, addr)		\
	(*((volatile unsigned long  *)(addr)) = (unsigned long)(v))

extern char *__bss_start;
extern char *__bss_end;

typedef void (*losc_enter_ss_func)(void);

#endif /*_STANDBY_H*/
