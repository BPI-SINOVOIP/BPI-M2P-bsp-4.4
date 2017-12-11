/*
 * drivers/soc/sunxi/pm/resource/mem_mapping.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef __MEM_MAPPING_H__
#define __MEM_MAPPING_H__

/*mem_mapping.c*/
extern void create_mapping(void);
extern void save_mapping(unsigned long vaddr);
extern void restore_mapping(void);

#endif				/* __MEM_MAPPING_H__ */
