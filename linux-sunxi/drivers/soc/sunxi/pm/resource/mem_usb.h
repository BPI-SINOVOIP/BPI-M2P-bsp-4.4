/*
 * drivers/soc/sunxi/pm/resource/mem_usb.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef __MEM_USB_H__
#define __MEM_USB_H__

extern __s32 mem_usb_init(void);
extern __s32 mem_usb_exit(void);
extern __s32 mem_query_usb_event(void);
extern __s32 mem_is_usb_status_change(__u32 port);

#endif /* __MEM_USB_H__ */
