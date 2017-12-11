/*
 * include/linux/dma/sunxi-dma.h
 *
 * Copyright (C) 2015-2020 Allwinnertech Co., Ltd
 *
 * Author: Wim Hwang <huangwei@allwinnertech.com>
 *
 * Sunxi DMA controller driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef __SUNXI_DMA_H__
#define __SUNXI_DMA_H__

#include <linux/dmaengine.h>

/*
 * The source and destination DRQ type and port corresponding relation
 */

#if defined(CONFIG_ARCH_SUN8IW7)
#include "sunxi/dma-sun8iw7.h"
#endif
#if defined(CONFIG_ARCH_SUN8IW11)
#include "sunxi/dma-sun8iw11.h"
#endif
#if defined(CONFIG_ARCH_SUN8IW7)
#include "sunxi/dma-sun8iw7.h"
#endif
#if defined(CONFIG_ARCH_SUN8IW12)
#include "sunxi/dma-sun8iw12.h"
#endif
#if defined(CONFIG_ARCH_SUN8IW15)
#include "sunxi/dma-sun8iw15.h"
#endif

#define sunxi_slave_id(d, s) (((d)<<16) | (s))
#define GET_SRC_DRQ(x)	((x) & 0x000000ff)
#define GET_DST_DRQ(x)	((x) & 0x00ff0000)

#define SUNXI_DMA_DRV	"sunxi_dmac"
#define SUNXI_RDMA_DRV	"sunxi_rdmac"

#define SUNXI_FILTER(name)	name##_filter_fn

bool sunxi_rdma_filter_fn(struct dma_chan *chan, void *param);
bool sunxi_dma_filter_fn(struct dma_chan *chan, void *param);

struct sunxi_dma_info {
	char name[16];
	u32 port;
	struct device *use_dev;
};

#endif /* __SUNXI_DMA_H__ */
