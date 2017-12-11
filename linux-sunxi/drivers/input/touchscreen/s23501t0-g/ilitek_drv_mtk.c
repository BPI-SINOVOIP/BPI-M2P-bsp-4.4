/*//////////////////////////////////////////////////////////////////////////////*/
/**/
/* Copyright (c) 2006-2014 MStar Semiconductor, Inc.*/
/* All rights reserved.*/
/**/
/* Unless otherwise stipulated in writing, any and all information contained*/
/* herein regardless in any format shall remain the sole proprietary of*/
/* MStar Semiconductor Inc. and be kept in strict confidence*/
/* (??MStar Confidential Information??) by the recipient.*/
/* Any unauthorized act including without limitation unauthorized disclosure, */
/* copying, use, reproduction, sale, distribution, modification, disassembling,*/
/* reverse engineering and compiling of the contents of MStar Confidential*/
/* Information is unlawful and strictly prohibited. MStar hereby reserves the*/
/* rights to any and all damages, losses, costs and expenses resulting therefrom.*/
/**/
/*//////////////////////////////////////////////////////////////////////////////*/

/**
 *
 * @file    ilitek_drv_mtk.c
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */

/*=============================================================*/
/* INCLUDE FILE*/
/*=============================================================*/

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>

#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/namei.h>
#include <linux/vmalloc.h>
#include <linux/sunxi-gpio.h>


#if defined(CONFIG_WAKEUP_BY_MSG2XXX)
#include <linux/pm_wakeup.h>
#include <linux/power/scenelock.h>
#include <linux/regulator/consumer.h>
#endif

#include "ilitek_drv_common.h"


#define DEBUG_INT 1

#define ERROR_INT 2

#define TPD_DMESG(fmt, arg...)
/*printk(KERN_EMERG"<<TPD_DEBUG>>[%s:%d] "fmt"\n", __func__, __LINE__, ##arg)*/
/*#define TPD_DMESG(fmt, arg...) \
  { \
  if (likely(fmt & DUENG_INT)) \
  printk(KERN_EMERG"<<TPD_DEBUG>>[%s:%d] "fmt"\n", __func__, __LINE__, ##arg); \
  else if (likely(fmt & ERROR_INT)) \
  printk(KERN_EMERG"<<TPD_ERROR>>[%s:%d] "fmt"\n", __func__, __LINE__, ##arg); \
  }*/
/*=============================================================*/
/* CONSTANT VALUE DEFINITION*/
/*=============================================================*/

#ifdef CONFIG_ENABLE_CHIP_TYPE_ILI21XX
#define TP_IC_NAME "ili212x" /*"ili2120" or "ili2121" Please define the ilitek touch ic name based on the mutual-capacitive ic or self capacitive ic that you are using */
#else /*CONFIG_ENABLE_CHIP_TYPE_MSG22XX || CONFIG_ENABLE_CHIP_TYPE_MSG28XX*/
#define TP_IC_NAME "msg2xxx" /*"msg22xx" or "msg28xx" Please define the mstar touch ic name based on the mutual-capacitive ic or self capacitive ic that you are using */
#endif /*CONFIG_ENABLE_CHIP_TYPE_ILI21XX*/

#define I2C_BUS_ID   (0)       /* i2c bus id : 0 or 1*/

#define TPD_OK (0)

/*=============================================================*/
/* EXTERN VARIABLE DECLARATION*/
/*=============================================================*/
#ifdef CONFIG_TP_HAVE_KEY
extern int g_TpVirtualKey[];

#ifdef CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE
extern int g_TpVirtualKeyDimLocal[][4];
#endif /*CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE*/
#endif /*CONFIG_TP_HAVE_KEY*/

#if defined(CONFIG_WAKEUP_BY_MSG2XXX)
int gt1x_suspend_flag;
static struct scene_lock normal_scene_lock;
static struct regulator *vdd_ctp;
extern int MS_TS_MSG_IC_GPIO_INT;
#endif

extern struct tpd_device *tpd;
extern void MsDrvInterfaceTouchDeviceResume(void);
extern void MsDrvInterfaceTouchDeviceSuspend(void);


/*=============================================================*/
/* LOCAL VARIABLE DEFINITION*/
/*=============================================================*/

struct i2c_client *g_I2cClient = NULL;

int revert_x_flag;
int revert_y_flag;
int exchange_x2y;
int tpd_abs_x_max;
int tpd_abs_y_max;

/*=============================================================*/
/* FUNCTION DECLARATION*/
/*=============================================================*/

static void tpd_dt_parse(struct device dev)
{
	struct device_node *np;

	np = dev.of_node;
	if (of_property_read_u32(np, "ctp_exchange_x_y_flag", &exchange_x2y)) {
		exchange_x2y = 0;
	}
	if (of_property_read_u32(np, "ctp_revert_x_flag", &revert_x_flag)) {
		revert_x_flag = 0;
	}
	if (of_property_read_u32(np, "ctp_revert_y_flag", &revert_y_flag)) {
		revert_y_flag = 0;
	}

	if (of_property_read_u32(np, "ctp_screen_max_x", &tpd_abs_x_max)) {
		tpd_abs_x_max = 4096;
	}
	if (of_property_read_u32(np, "ctp_screen_max_x", &tpd_abs_y_max)) {
		tpd_abs_y_max = 4096;
	}
}

/*=============================================================*/
/* FUNCTION DEFINITION*/
/*=============================================================*/

/* probe function is used for matching and initializing input device */
static int /*__devinit*/ tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;

	TPD_DMESG(DEBUG_INT, "TPD probe\n");

	if (client == NULL) {
		TPD_DMESG(DEBUG_INT, "i2c client is NULL\n");
		return -1;
	}
	g_I2cClient = client;

	tpd_dt_parse(client->dev);
	MsDrvInterfaceTouchDeviceSetIicDataRate(g_I2cClient, 100000); /* 100 KHz*/

	ret = MsDrvInterfaceTouchDeviceProbe(g_I2cClient, id);

#if defined(CONFIG_WAKEUP_BY_MSG2XXX)
	scene_lock_init(&normal_scene_lock, SCENE_NORMAL_STANDBY, "normal-standby");
	scene_lock(&normal_scene_lock);
	enable_wakeup_src(CPUS_GPIO_SRC, MS_TS_MSG_IC_GPIO_INT);

	device_init_wakeup(&client->dev, 1);
	vdd_ctp = regulator_get(&client->dev, "vcc-ctp");
	if (IS_ERR(vdd_ctp)) {
		printk(KERN_EMERG"regulator_get vcc-ctp failed\n");
		vdd_ctp = NULL;
	}
	regulator_enable(vdd_ctp);
#endif

	TPD_DMESG(DEBUG_INT, "TPD probe done\n");

	return TPD_OK;
}

static int /*__devexit*/ tpd_remove(struct i2c_client *client)
{
#if defined(CONFIG_WAKEUP_BY_MSG2XXX)
	regulator_disable(vdd_ctp);
	regulator_put(vdd_ctp);
	disable_wakeup_src(CPUS_GPIO_SRC, MS_TS_MSG_IC_GPIO_INT);
	scene_unlock(&normal_scene_lock);
	scene_lock_destroy(&normal_scene_lock);
#endif
	MsDrvInterfaceTouchDeviceRemove(client);

	return TPD_OK;
}


/* The I2C device list is used for matching I2C device and I2C device driver. */
static const struct i2c_device_id tpd_device_id[] =
{
	{TP_IC_NAME, 0},
	{}, /* should not omitted */
};

MODULE_DEVICE_TABLE(i2c, tpd_device_id);

static int tpd_pm_resume(struct device *dev)
{
	TPD_DMESG(DEBUG_INT, "TPD wake up\n");
#if defined(CONFIG_WAKEUP_BY_MSG2XXX)
	gt1x_suspend_flag = 0;
#else
	MsDrvInterfaceTouchDeviceResume();
#endif
	TPD_DMESG(DEBUG_INT, "TPD wake up done\n");
	return 0;
}

static int tpd_pm_suspend(struct device *dev)
{
	printk("TPD enter sleep\n");
#if defined(CONFIG_WAKEUP_BY_MSG2XXX)
	gt1x_suspend_flag = 1;
#else
	MsDrvInterfaceTouchDeviceSuspend();
#endif
	TPD_DMESG(DEBUG_INT, "TPD enter sleep done\n");
	return 0;
}

#ifdef GTP_CONFIG_OF
static const struct of_device_id tpd_match_table[] = {
	{.compatible = "s23501to-g", },
	{},
};

/*MODULE_DEVICE_TABLE(of, sunxi_ir_recv_of_match);*/
#endif

/* bus control the suspend/resume procedure */
static const struct dev_pm_ops tpd_ts_pm_ops = {
	.suspend = tpd_pm_suspend,
	.resume = tpd_pm_resume,
};

static struct i2c_driver tpd_i2c_driver = {
	.driver = {
		.name = TP_IC_NAME,
#ifdef GTP_CONFIG_OF
		.of_match_table = tpd_match_table,
#endif
#if defined(CONFIG_PM)
		.pm = &tpd_ts_pm_ops,
#endif
	},
	.probe = tpd_probe,
	.remove = tpd_remove,
	.id_table = tpd_device_id,
};

static int tpd_local_init(void)
{
	TPD_DMESG(DEBUG_INT, "TPD init device driver\n");

	if (i2c_add_driver(&tpd_i2c_driver) != 0) {
		TPD_DMESG(DEBUG_INT, "TPD init done %s, %d\n", __func__, __LINE__);
	}
	return TPD_OK;
}

static int __init tpd_driver_init(void)
{
	TPD_DMESG("ILITEK/MStar touch panel driver init\n");
	if (tpd_local_init()) {
		TPD_DMESG(ERROR_INT, "TPD add ILITEK/MStar TP driver failed\n");
	}

	return 0;
}

static void __exit tpd_driver_exit(void)
{
	TPD_DMESG(DEBUG_INT, "ILITEK/MStar touch panel driver exit\n");
	i2c_del_driver(&tpd_i2c_driver);
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);
MODULE_LICENSE("GPL");
