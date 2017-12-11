/*
 * arch/arm/mach-sunxi/include/mach/memory.h
 *
 * Copyright(c) 2013-2015 Allwinnertech Co., Ltd.
 *      http://www.allwinnertech.com
 *
 * Author: liugang <liugang@allwinnertech.com>
 *	   Sugar <shuge@allwinnertech.com>
 *
 * sunxi memory header file
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 */

#ifndef __SUNXI_MEMORY_H
#define __SUNXI_MEMORY_H

#define __sram	__section(.sram.text)
#define __sramdata __section(.sram.data)

/* For assembly routines */
#define __SRAM		.section	".sram.text", "ax"
#define __SRAMDATA	.section	".sram.text", "aw"

#ifndef SRAM_DDRFREQ_OFFSET
#define SRAM_DDRFREQ_OFFSET 0xF0000000
#endif

#define SUNXI_DDRFREQ_SRAM_SECTION(OFFSET, align) 		\
	. = ALIGN(align);					\
	__sram_start = .;					\
	.sram.text OFFSET : AT(__sram_start) {			\
		__sram_text_start = .;				\
		. = ALIGN(align);				\
		*(.sram.text)					\
		__sram_text_end = .;				\
	}							\
	.sram.data OFFSET + SIZEOF(.sram.text) :		\
		AT(__sram_start + SIZEOF(.sram.text)) {		\
		__sram_data_start = .;				\
		. = ALIGN(align);				\
		*(.sram.data)					\
		__sram_data_end = .;				\
	}							\
	. = __sram_start + SIZEOF(.sram.text) +			\
			SIZEOF(.sram.data);			\
	__sram_end = .;

#define SUNXI_SRAM_SECTION	\
	SUNXI_DDRFREQ_SRAM_SECTION(SRAM_DDRFREQ_OFFSET, 4)

#endif /* __SUNXI_MEMORY_H */
