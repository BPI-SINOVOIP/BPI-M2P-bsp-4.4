/*
 * Platform interfaces for XRadio drivers
 *
 * Copyright (c) 2013
 * Xradio Technology Co., Ltd. <www.xradiotech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <generated/uapi/linux/version.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/delay.h>

#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/ioport.h>

#include <linux/regulator/consumer.h>
/*#include <asm/mach-types.h>*/
/*#include <mach/sys_config.h>*/

#include "xradio.h"
#include "platform.h"
#include "sbus.h"

#include <linux/gpio.h>
#include <linux/types.h>
#include <linux/power/scenelock.h>
#include <linux/power/aw_pm.h>
/*#define WLAN_WOW_SUPPORT*/

static int wlan_bus_id;
static u32 gpio_irq_handle;

static int xradio_get_syscfg(void)
{
	int wlan_bus_index = 0;
	wlan_bus_index = sunxi_wlan_get_bus_index();
	if (wlan_bus_index < 0)
		return wlan_bus_index;
	else
		wlan_bus_id = wlan_bus_index;

	gpio_irq_handle = sunxi_wlan_get_oob_irq();
	return wlan_bus_index;
}

#if 0
static int h64_wlan_soc_power(int on)
{
	struct regulator *ldo = NULL;
	int ret = 0;
	if (on) {
		/* wifi vcc 1.8v .*/
		ldo = regulator_get(NULL, "axp81x_dldo2");
		if (ldo) {
			regulator_set_voltage(ldo, 1800000, 1800000);
			ret = regulator_enable(ldo);
			if (ret < 0)
				printk(KERN_ERR "regulator_enable failed.\n");
			regulator_put(ldo);
		}
	} else {
		/* wifi vcc 1.8v */
		ldo = regulator_get(NULL, "axp81x_dldo2");
		if (ldo) {
			ret = regulator_disable(ldo);
			regulator_put(ldo);
		}
	}
	return ret;
}
#endif

/*********************Interfaces called by xradio core. *********************/
int  xradio_plat_init(void)
{
	return 0;
}

void xradio_plat_deinit(void)
{
}

int xradio_wlan_power(int on)
{
	int ret = 0;
	if (on) {
		ret = xradio_get_syscfg();
		if (ret < 0)
			return ret;
	}
	sunxi_wlan_set_power(on);
	mdelay(100);
	return ret;
}

void xradio_sdio_detect(int enable)
{
	MCI_RESCAN_CARD(wlan_bus_id);
	xradio_dbg(XRADIO_DBG_ALWY, "%s SDIO card %d\n",
			enable ? "Detect" : "Remove", wlan_bus_id);
	mdelay(10);
}

static irqreturn_t xradio_gpio_irq_handler(int irq, void *sbus_priv)
{
	struct sbus_priv *self = (struct sbus_priv *)sbus_priv;
	unsigned long flags;

	SYS_BUG(!self);
	spin_lock_irqsave(&self->lock, flags);
	if (self->irq_handler)
		self->irq_handler(self->irq_priv);
	spin_unlock_irqrestore(&self->lock, flags);
	return IRQ_HANDLED;
}

int xradio_request_gpio_irq(struct device *dev, void *sbus_priv)
{
	int ret = -1;
	ret = devm_request_irq(dev, gpio_irq_handle,
			(irq_handler_t)xradio_gpio_irq_handler,
			IRQF_TRIGGER_RISING|IRQF_NO_SUSPEND, "xradio_irq", sbus_priv);
	if (IS_ERR_VALUE(ret)) {
		gpio_irq_handle = 0;
		xradio_dbg(XRADIO_DBG_ERROR, "%s: request_irq FAIL!ret=%d\n",
				__func__, ret);
	}
#ifdef WLAN_WOW_SUPPORT
	enable_irq_wake(gpio_irq_handle);
#endif
	return ret;
}

void xradio_free_gpio_irq(struct device *dev, void *sbus_priv)
{
	struct sbus_priv *self = (struct sbus_priv *)sbus_priv;
#ifdef WLAN_WOW_SUPPORT
	disable_irq_wake(gpio_irq_handle);
#endif
	devm_free_irq(dev, gpio_irq_handle, self);
	/*gpio_free(wlan_irq_gpio);*/
	gpio_irq_handle = 0;
}
