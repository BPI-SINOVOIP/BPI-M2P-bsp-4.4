/*
 * platform interfaces for XRadio drivers
 *
 * Copyright (c) 2013
 * Xradio Technology Co., Ltd. <www.xradiotech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef XRADIO_PLAT_H_INCLUDED
#define XRADIO_PLAT_H_INCLUDED

#include <generated/uapi/linux/version.h>
#include <linux/kernel.h>
#include <linux/mmc/host.h>

#define MCI_CHECK_READY(h, t)     0

/* platform interfaces */
int  xradio_plat_init(void);
void xradio_plat_deinit(void);
void  xradio_sdio_detect(int enable);
int  xradio_request_gpio_irq(struct device *dev, void *sbus_priv);
void xradio_free_gpio_irq(struct device *dev, void *sbus_priv);
int  xradio_wlan_power(int on);

/* Select hardware platform.*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
extern void sunxi_mmc_rescan_card(unsigned ids);
#define MCI_RESCAN_CARD(id)  sunxi_mmc_rescan_card(id)
extern void sunxi_wlan_set_power(int on);
extern int sunxi_wlan_get_bus_index(void);
extern int sunxi_wlan_get_oob_irq(void);
/*extern int sunxi_wlan_get_oob_irq_flags(void);*/
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0))
#define PLAT_ALLWINNER_SUNXI
extern void sunxi_mci_rescan_card(unsigned id, unsigned insert);
#define MCI_RESCAN_CARD(id, ins)  sunxi_mci_rescan_card(id, ins)
#else
#define PLAT_ALLWINNER_SUN6I
extern void sw_mci_rescan_card(unsigned id, unsigned insert);
#define MCI_RESCAN_CARD(id, ins)  sw_mci_rescan_card(id, ins)
#endif

#ifdef CONFIG_CUSTOM_MAC_ADDRESS
extern void sunxi_wlan_custom_mac_address(u8 *mac);
extern void sunxi_wlan_chipid_mac_address(u8 *mac);
#endif /* CONFIG_CUSTOM_MAC_ADDRESS */

#endif /* XRADIO_PLAT_H_INCLUDED */
