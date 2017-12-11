/*
 * drivers\media\platform\ise
 * Copyright (c) 2017 by Allwinnertech Co., Ltd.
 * http://www.allwinnertech.com
 *
 * Authors:Tu Qiang <tuqiang@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/time.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/mpp.h>

#include "ise.h"
#include "ise_api.h"

#define ISE_MODULE_VERS "V2.0"
#define ISE_MODULE_NAME "ISE"

#define ISE_CLK_HIGH_WATER  (700)
#define ISE_CLK_LOW_WATER   (400)

static unsigned int *ise_regs_init;
static unsigned int irq_flag = 0;
static unsigned int ise_irq_id = 0;

static unsigned long interrupt_times;
static unsigned long err_cnt;

struct clk *ise_moduleclk;
struct clk *ise_parent_pll_clk;
static unsigned long ise_parent_clk_rate = 432000000;

static int clk_status;
static int frame_status;
static spinlock_t ise_spin_lock;

static struct dentry *debugfsP;

#if defined(CONFIG_OF)
static struct of_device_id sunxi_ise_match[] = {
	{ .compatible = "allwinner,sunxi-ise",},
	{}
};
MODULE_DEVICE_TABLE(of, sunxi_ise_match);
#endif


static DECLARE_WAIT_QUEUE_HEAD(wait_ise);

static irqreturn_t ise_interrupt(unsigned long data)
{
	int irq_status;

	writel(0x00, ise_regs_init + (ISE_INTERRUPT_EN>>2));

	irq_status = readl(ise_regs_init + (ISE_INTERRUPT_STATUS>>2));
	if (0x01 != (irq_status & 0x01))
		err_cnt++;

	interrupt_times++;
	irq_flag = 1;
	wake_up(&wait_ise);

	return IRQ_HANDLED;
}


static int isedev_open(struct inode *node, struct file *filp)
{
	return 0;
}


static int isedev_close(struct inode *node, struct file *filp)
{
	return 0;
}


static int ise_debugfs_show_main(struct seq_file *m, void *v)
{
	int ise_clk;
	int ise_in_size, in_width, in_height;
	int ise_out_size, out_width, out_height;
	int ise_ocfg;
	int flip, mirror;
	int stride_y_out, stride_c_out;
	int plane_mode;

	seq_printf(m, "---------------ISE  MAIN  PARAM---------------\n");

	/* ISE MODULE CLK */
	ise_clk = clk_get_rate(ise_moduleclk);
	seq_printf(m, "ISE MODULE CLK:%dMHz\n", ise_clk/1000000);

	/* W_IN、H_IN、FMT_IN */
	ise_in_size = readl(ise_regs_init + (ISE_IN_SIZE>>2));
	in_width = 0x0000ffff & ise_in_size;
	in_height = ise_in_size >> 16;

	seq_printf(m, "W_IN:%d\n", in_width);
	seq_printf(m, "H_IN:%d\n", in_height);

	/* W_OUT、H_OUT、FMT_OUT */
	ise_out_size = readl(ise_regs_init + (ISE_OUT_SIZE>>2));
	out_width = 0x0000ffff & ise_out_size;
	out_height = ise_out_size >> 16;

	seq_printf(m, "W_OUT:%d\n", out_width);
	seq_printf(m, "H_OUT:%d\n", out_height);

	/* FLIP、MIRR */
	ise_ocfg = readl(ise_regs_init + (ISE_OCFG_REG>>2));

	flip = 0X10 & ise_ocfg;
	if (flip)
		seq_printf(m, "FLIP:TRUE\n");
	else
		seq_printf(m, "FLIP:FALSE\n");

	mirror = 0X08 & ise_ocfg;
	if (mirror)
		seq_printf(m, "MIRR:TRUE\n");
	else
		seq_printf(m, "MIRR:FALSE\n");

	stride_y_out = ((0X3ff00000 & ise_ocfg) >> 20) * 8;
	seq_printf(m, "STRIDE_Y_OUT:%d\n", stride_y_out);

	stride_c_out = ((0X3ff00 & ise_ocfg) >> 8) * 8;
	seq_printf(m, "STRIDE_C_OUT:%d\n", stride_c_out);

	plane_mode = 0X03 & ise_ocfg;
	switch (plane_mode) {
	case 0:
		seq_printf(m, "FMT_OUT:YUV420\n");
		break;
	case 1:
		seq_printf(m, "FMT_OUT:YVU420\n");
		break;
	case 2:
		seq_printf(m, "FMT_OUT:YUV420p\n");
		break;
	case 4:
		seq_printf(m, "FMT_OUT:YUV422\n");
		break;
	case 5:
		seq_printf(m, "FMT_OUT:YVU422\n");
		break;
	case 6:
		seq_printf(m, "FMT_OUT:YUV422p\n");
		break;
	default:
		seq_printf(m, "FMT_OUT:OUT FORMAT ERR!\n");
	}

	return 0;
}

static int ise_debugfs_show_scale(struct seq_file *m, void *v)
{
	int ctrl_val;
	int sc0_en, sc1_en, sc2_en;

	int ise_out_size_chn0, out_width_chn0, out_height_chn0;
	int ise_ocfg_chn0, flip_chn0, mirror_chn0;
	int stride_y_out_chn0, stride_c_out_chn0;

	int ise_out_size_chn1, out_width_chn1, out_height_chn1;
	int ise_ocfg_chn1, flip_chn1, mirror_chn1;
	int stride_y_out_chn1, stride_c_out_chn1;

	int ise_out_size_chn2, out_width_chn2, out_height_chn2;
	int ise_ocfg_chn2, flip_chn2, mirror_chn2;
	int stride_y_out_chn2, stride_c_out_chn2;

	seq_printf(m, "--------------ISE  SCALE  PARAM---------------\n");

	/* W_OUT_CHN0、H_OUT_CHN0 */
	ctrl_val = readl(ise_regs_init + (ISE_CTRL_REG>>2));
	sc0_en = 0x010 & ctrl_val;
	sc1_en = 0x020 & ctrl_val;
	sc2_en = 0x040 & ctrl_val;

	if (sc0_en) {
		seq_printf(m, "SCALE_CHN0_EN:TRUE\n");
		/* W_OUT_CHN0、H_OUT_CHN0、FMT_OUT_CHN0 */
		ise_out_size_chn0 = readl(ise_regs_init + (ISE_SC_OUT_SIZE_0>>2));
		out_width_chn0 = 0x0000ffff & ise_out_size_chn0;
		out_height_chn0 = ise_out_size_chn0 >> 16;

		seq_printf(m, "W_OUT_CHN0:%d\n", out_width_chn0);
		seq_printf(m, "H_OUT_CHN0:%d\n", out_height_chn0);

		/* CHN0 FLIP、MIRR */
		ise_ocfg_chn0 = readl(ise_regs_init + (ISE_SC_OCFG_REG_0>>2));

		flip_chn0 = 0X10 & ise_ocfg_chn0;
		if (flip_chn0)
			seq_printf(m, "FLIP_CHN0:TRUE\n");
		else
			seq_printf(m, "FLIP_CHN0:FALSE\n");

		mirror_chn0 = 0X08 & ise_ocfg_chn0;
		if (mirror_chn0)
			seq_printf(m, "MIRR_CHN0:TRUE\n");
		else
			seq_printf(m, "MIRR_CHN0:FALSE\n");

		stride_y_out_chn0 = ((0X3ff00000 & ise_ocfg_chn0) >> 20) * 8;
		seq_printf(m, "STRIDE_Y_OUT_CHN0:%d\n", stride_y_out_chn0);

		stride_c_out_chn0 = ((0X3ff00 & ise_ocfg_chn0) >> 8) * 8;
		seq_printf(m, "STRIDE_C_OUT_CHN0:%d\n", stride_y_out_chn0);
	} else
		seq_printf(m, "\nSCALE_CHN0_EN:FALSE\n");

	if (sc1_en) {
		seq_printf(m, "SCALE_CHN1_EN:TRUE\n");
		/* W_OUT_CHN1、H_OUT_CHN1、FMT_OUT_CHN1 */
		ise_out_size_chn1 = readl(ise_regs_init + (ISE_SC_OUT_SIZE_1>>2));
		out_width_chn1 = 0x0000ffff & ise_out_size_chn1;
		out_height_chn1 = ise_out_size_chn1 >> 16;

		seq_printf(m, "W_OUT_CHN1:%d\n", out_width_chn1);
		seq_printf(m, "H_OUT_CHN1:%d\n", out_height_chn1);

		/* CHN1 FLIP、MIRR */
		ise_ocfg_chn1 = readl(ise_regs_init + (ISE_SC_OCFG_REG_1>>2));

		flip_chn1 = 0X10 & ise_ocfg_chn1;
		if (flip_chn1)
			seq_printf(m, "FLIP_CHN1:TRUE\n");
		else
			seq_printf(m, "FLIP_CHN1:FALSE\n");

		mirror_chn1 = 0X08 & ise_ocfg_chn1;
		if (mirror_chn1)
			seq_printf(m, "MIRR_CHN1:TRUE\n");
		else
			seq_printf(m, "MIRR_CHN1:FALSE\n");

		stride_y_out_chn1 = ((0X3ff00000 & ise_ocfg_chn1) >> 20) * 8;
		seq_printf(m, "STRIDE_Y_OUT_CHN1:%d\n", stride_y_out_chn1);

		stride_c_out_chn1 = ((0X3ff00 & ise_ocfg_chn1) >> 8) * 8;
		seq_printf(m, "STRIDE_C_OUT_CHN1:%d\n", stride_y_out_chn1);
	} else
		seq_printf(m, "\nSCALE_CHN1_EN:FALSE\n");

	if (sc2_en) {
		seq_printf(m, "SCALE_CHN2_EN:TRUE\n");
		/* W_OUT_CHN2、H_OUT_CHN2、FMT_OUT_CHN2 */
		ise_out_size_chn2 = readl(ise_regs_init + (ISE_SC_OUT_SIZE_2>>2));
		out_width_chn2 = 0x0000ffff & ise_out_size_chn2;
		out_height_chn2 = ise_out_size_chn2 >> 16;

		seq_printf(m, "W_OUT_CHN2:%d\n", out_width_chn2);
		seq_printf(m, "H_OUT_CHN2:%d\n", out_height_chn2);

		/* CHN2 FLIP、MIRR */
		ise_ocfg_chn2 = readl(ise_regs_init + (ISE_SC_OCFG_REG_2>>2));

		flip_chn2 = 0X10 & ise_ocfg_chn2;
		if (flip_chn2)
			seq_printf(m, "FLIP_CHN2:TRUE\n");
		else
			seq_printf(m, "FLIP_CHN2:FALSE\n");

		mirror_chn2 = 0X08 & ise_ocfg_chn2;
		if (mirror_chn2)
			seq_printf(m, "MIRR_CHN2:TRUE\n");
		else
			seq_printf(m, "MIRR_CHN2:FALSE\n");

		stride_y_out_chn2 = ((0X3ff00000 & ise_ocfg_chn2) >> 20) * 8;
		seq_printf(m, "STRIDE_Y_OUT_CHN2:%d\n", stride_y_out_chn2);

		stride_c_out_chn2 = ((0X3ff00 & ise_ocfg_chn2) >> 8) * 8;
		seq_printf(m, "STRIDE_C_OUT_CHN2:%d\n", stride_y_out_chn2);
	} else
		seq_printf(m, "\nSCALE_CHN2_EN:FALSE\n");

	return 0;
}


static int ise_debugfs_show_status(struct seq_file *m, void *v)
{
	int ise_interrupt_status;
	int time_out;

	int ise_err_flag;
	int roi_err;

	seq_printf(m, "------------------ISE STATUS------------------\n");

	/* INTERRUPT_TIMES */
	seq_printf(m, "INTERRUPT_TIMES:%ld\n", interrupt_times);
	seq_printf(m, "ISE_ERR_CNT:%ld\n", err_cnt);

	/*interrupt_times = 0;*/
	/*err_cnt = 0;*/

	/* TIMEOUT_ERR */
	ise_interrupt_status = readl(ise_regs_init + (ISE_INTERRUPT_STATUS>>2));
	time_out = 0x04 & ise_interrupt_status;
	if (time_out)
		seq_printf(m, "TIMEOUT_ERR:YES\n");
	else
		seq_printf(m, "TIMEOUT_ERR:NO\n");

	/* ROI_ERR */
	ise_err_flag = readl(ise_regs_init + (ISE_ERROR_FLAG>>2));
	roi_err = 0x08 & ise_err_flag;
	if (roi_err)
		seq_printf(m, "ROI_ERR:YES\n");
	else
		seq_printf(m, "ROI_ERR:NO\n");

	return 0;
}


static int ise_debugfs_show(struct seq_file *m, void *v)
{
	seq_printf(m, "\n[%s] Version:[%s Debug]!\n", ISE_MODULE_NAME, ISE_MODULE_VERS);

	ise_debugfs_show_main(m, v);

	ise_debugfs_show_scale(m, v);

	ise_debugfs_show_status(m, v);

	return 0;
}

static int ise_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, ise_debugfs_show, NULL);
}

static long ise_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct ise_register reg;
	unsigned int rData = 0x5a5a5a5a;

	if (unlikely(copy_from_user(&reg, (void __user *)arg, sizeof(struct ise_register))))
		return -EFAULT;
	else {
		switch (cmd) {
		case ISE_WRITE_REGISTER:
			writel(reg.value, ise_regs_init + (reg.addr>>2));
			/*ise_debug("[ISE] write to %#010x, value %#010x\n",
				(unsigned int)(ise_regs_init+(reg.addr>>2)), reg.value);*/
			return 0;
		case ISE_READ_REGISTER:
			rData = readl(ise_regs_init + (reg.addr>>2));
			/*ise_debug("[ISE] read from %#010x, value %#010x\n",
				(unsigned int)(ise_regs_init+(reg.addr>>2)), rData);*/
			return rData;
		case ENABLE_ISE:
			if (clk_prepare_enable(ise_moduleclk))
				ise_err("[SYS] enable ise_moduleclk failed!\n");
			break;
		case DISABLE_ISE:
			if ((NULL == ise_moduleclk) || IS_ERR(ise_moduleclk)) {
				ise_err("[SYS] ise_moduleclk is invalid!\n");
				return -EFAULT;
			} else {
				clk_disable_unprepare(ise_moduleclk);
				ise_print("[SYS] disable ise_moduleclk!\n");
			}
			break;
		case WAIT_ISE_FINISH:
			wait_event_timeout(wait_ise, irq_flag, reg.value*HZ/30*3);
			if (0 == irq_flag)
				ise_print("[SYS] wait_event_timeout!\n");
			irq_flag = 0;
			frame_status = 1;
			break;
		case SET_ISE_FREQ:
		{
			unsigned int arg_rate = reg.value;

			if (arg_rate >= ISE_CLK_LOW_WATER &&
				arg_rate <= ISE_CLK_HIGH_WATER) {
				if (!clk_set_rate(ise_parent_pll_clk, arg_rate*1000000)) {
					ise_parent_clk_rate = clk_get_rate(ise_parent_pll_clk);
					if (clk_set_rate(ise_moduleclk, ise_parent_clk_rate))
						ise_err("[SYS] set ise clock failed!\n");
				} else
					ise_err("set pll clock failed!\n");
			}

			break;
		}
		default:
			ise_warn("[SYS] do not supprot this ioctrl now!\n");
			break;
		}

		return 0;
	}
}


int enable_ise_hw_clk(void)
{
	unsigned long flags;
	int res = -EFAULT;

	spin_lock_irqsave(&ise_spin_lock, flags);

	if (clk_status == 0) {
		res = 0;
		goto out;
	}
	clk_status = 0;

	if ((NULL == ise_moduleclk) || (IS_ERR(ise_moduleclk)))
		ise_err("ise_moduleclk is invalid!\n");
	else
		clk_prepare_enable(ise_moduleclk);

out:
	spin_unlock_irqrestore(&ise_spin_lock, flags);
	return 0;
}


int disable_ise_hw_clk(void)
{
	unsigned long flags;
	int res = -EFAULT;

	spin_lock_irqsave(&ise_spin_lock, flags);

	if (clk_status == 1) {
		res = 1;
		goto out;
	}
	clk_status = 1;

	if ((NULL == ise_moduleclk) || (IS_ERR(ise_moduleclk)))
		ise_err("ise_moduleclk is invalid!\n");
	else
		clk_disable_unprepare(ise_moduleclk);

out:
	spin_unlock_irqrestore(&ise_spin_lock, flags);
	return 0;
}


static int snd_sw_ise_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	int times = 0;

	while (times < 50) {
		if (1 == frame_status)
			break;
		else
			mdelay(1);
	}

	ret = disable_ise_hw_clk();

	if (ret < 0) {
		ise_err("ise clk disable somewhere error!\n");
		return -EFAULT;
	}

	printk("[ise] standby suspend finish!\n");

	return 0;
}


static int snd_sw_ise_resume(struct platform_device *pdev)
{
	int ret = 0;

	ret = enable_ise_hw_clk();
	if (ret < 0) {
		ise_err("[ise] ise clk enable somewhere error!\n");
		return -EFAULT;
	}

	frame_status = 0;

	printk("[ise] standby resume finish!\n");

	return 0;
}


static struct file_operations ise_fops =
{
	.owner          = THIS_MODULE,
	.unlocked_ioctl = ise_ioctl,
	.open           = isedev_open,
	.release        = isedev_close
};


static const struct file_operations ise_debugfs_fops =
{
	.owner      = THIS_MODULE,
	.open       = ise_debugfs_open,
	.read       = seq_read,
	.llseek     = seq_lseek,
	.release    = single_release,
};


static struct miscdevice ise_dev =
{
	.minor  = MISC_DYNAMIC_MINOR,
	.name   = DEVICE_NAME,
	.fops   = &ise_fops
};

int sunxi_ise_debug_register_driver(void)
{
	debugfsP = debugfs_create_file("sunxi-ise", 0444, debugfs_mpp_root,
								NULL, &ise_debugfs_fops);
	if (IS_ERR_OR_NULL(debugfsP))
		printk(KERN_INFO "Warning: fail to create debugfs node!\n");

	return 0;
}

void sunxi_ise_debug_unregister_driver(void)
{
	debugfs_remove(debugfsP);
}


static int sunxi_ise_init(struct platform_device *pdev)
{
	struct device_node *node;
	int ret;

	ret = misc_register(&ise_dev);
	if (ret >= 0)
		ise_print("ISE device register success! %d\n", ret);
	else {
		ise_err("ISE device register failed! %d\n", ret);
		return -EINVAL;
	}

	node = of_find_compatible_node(NULL, NULL, "allwinner,sunxi-ise");
	ise_irq_id = irq_of_parse_and_map(node, 0);

	ise_print("irq_id = %d\n", ise_irq_id);

	ret = request_irq(ise_irq_id, (irq_handler_t)ise_interrupt, 0, "sunxi_ise", NULL);
	if (ret < 0) {
		ise_err("Request ISE Irq error! return %d\n", ret);
		return -EINVAL;
	} else
		ise_print("Request ISE Irq success! irq_id = %d, return %d\n", ise_irq_id, ret);

	ise_regs_init = (unsigned int *)of_iomap(node, 0);


	ise_parent_pll_clk = of_clk_get(node, 0);
	if ((!ise_parent_pll_clk) || IS_ERR(ise_parent_pll_clk)) {
		ise_warn("[ise] try to get ise_parent_pll_clk failed!\n");
		return -EINVAL;
	}

	ise_moduleclk = of_clk_get(node, 1);
	if (!ise_moduleclk || IS_ERR(ise_moduleclk)) {
		ise_warn("[ise] get ise_moduleclk failed!\n");
		return -EINVAL;
	}

	ret = sunxi_ise_debug_register_driver();
	if (ret) {
		ise_err("Sunxi ise debug register driver failed!\n");
		return -EINVAL;
	}

	return 0;
}


static void sunxi_ise_exit(void)
{
	sunxi_ise_debug_unregister_driver();

	if (NULL == ise_moduleclk || IS_ERR(ise_moduleclk)) {
		ise_warn("[ise] ise_moduleclk handle "
			"is invalid,just return!\n");
	} else {
		clk_put(ise_moduleclk);
		ise_moduleclk = NULL;
	}

	if (NULL == ise_parent_pll_clk || IS_ERR(ise_parent_pll_clk)) {
		ise_warn("[ise] ise_parent_pll_clk "
			"handle is invalid,just return!\n");
	} else {
		clk_put(ise_parent_pll_clk);
		ise_parent_pll_clk = NULL;
	}

	free_irq(ise_irq_id, NULL);
	iounmap(ise_regs_init);

	misc_deregister(&ise_dev);
	ise_print("ISE device has been removed!\n");
}


static int	sunxi_ise_remove(struct platform_device *pdev)
{
	sunxi_ise_exit();
	return 0;
}

static int	sunxi_ise_probe(struct platform_device *pdev)
{
	sunxi_ise_init(pdev);
	return 0;
}


static struct platform_driver sunxi_ise_driver = {
	.probe		= sunxi_ise_probe,
	.remove		= sunxi_ise_remove,
	.suspend	= snd_sw_ise_suspend,
	.resume		= snd_sw_ise_resume,

	.driver		= {
		.name	= "sunxi-ise",
		.owner	= THIS_MODULE,
		.of_match_table = sunxi_ise_match,
	},

};


static int __init ise_init(void)
{
	printk("sunxi ise version 2.0 \n");

	return platform_driver_register(&sunxi_ise_driver);
}


static void __exit ise_exit(void)
{
	platform_driver_unregister(&sunxi_ise_driver);
};

module_init(ise_init);
module_exit(ise_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("TuQiang<tuqiang@allwinnertech.com>");
MODULE_DESCRIPTION("ISE device driver");
MODULE_VERSION("2.0");
MODULE_ALIAS("platform:ISE(AW1721)");

