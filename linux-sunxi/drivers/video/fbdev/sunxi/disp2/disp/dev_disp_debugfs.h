/*
 * Allwinner SoCs display driver.
 *
 * Copyright (C) 2016 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#if defined(CONFIG_DISP2_SUNXI_DEBUG)
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include "de/bsp_display.h"
#include "de/disp_tv.h"

int dispdbg_init(void);
int dispdbg_exit(void);

extern struct disp_layer *disp_get_layer(u32 disp, u32 chn, u32 layer_id);
extern struct disp_layer *disp_get_layer_1(u32 disp, u32 layer_id);
extern struct disp_smbl *disp_get_smbl(u32 disp);
extern struct disp_enhance *disp_get_enhance(u32 disp);
extern struct disp_capture *disp_get_capture(u32 disp);
extern struct disp_device *disp_get_lcd(u32 disp);
extern struct disp_manager *disp_get_layer_manager(u32 disp);
extern unsigned int composer_dump(char *buf);
extern int disp_suspend(struct device *dev);
extern int disp_resume(struct device *dev);
#if defined(CONFIG_SUNXI_MPP)
extern struct dentry *debugfs_mpp_root;
#endif /*endif CONFIG_SUNXI_MPP */
#endif /*endif CONFIG_DISP2_SUNXI_DEBUG */

