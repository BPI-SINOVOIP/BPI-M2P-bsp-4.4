/*
 * include/linux/ion_sunxi.h
 *
 * Copyright(c) 2013-2015 Allwinnertech Co., Ltd.
 *      http://www.allwinnertech.com
 *
 * Author: liugang <liugang@allwinnertech.com>
 *
 * sunxi ion header file
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _UAPI_LINUX_SUNXI_ION_H
#define _UAPI_LINUX_SUNXI_ION_H

struct sunxi_cache_range {
	long start;
	long end;
};

struct sunxi_phys_data {
	ion_user_handle_t handle;
	unsigned int phys_addr;
	unsigned int size;
};

#define ION_IOC_SUNXI_FLUSH_RANGE           5
#define ION_IOC_SUNXI_PHYS_ADDR             7
#define ION_IOC_SUNXI_TEE_ADDR              17

#endif
