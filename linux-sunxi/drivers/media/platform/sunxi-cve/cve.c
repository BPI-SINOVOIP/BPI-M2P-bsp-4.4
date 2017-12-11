/*
 * drivers\media\platform\cve.c
 * (C) Copyright 2016-2017
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * gaofeng<gaofeng@allwinnertech.com>
 *
 * Driver for compuer vision engine(CVE).
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */
#include <linux/signal.h>
#include <linux/delay.h>
#include <linux/mpp.h>
#include "cve.h"

static void __iomem *cve_regs_init;
static u32 *cve_regs_temp, *dma_regs_temp;
static struct fasync_struct *async_queue;
static unsigned long cve_end_flag, cve_int_flag;

static unsigned long dma_int_flag, list_int_flag;
static unsigned long cve_sig_sel, r_data;
static unsigned char if_error, if_sleep, if_finish, if_run;
static unsigned long cve_irq_id;
static struct dentry *debugfsP;
static DECLARE_WAIT_QUEUE_HEAD(wait_cve);
static spinlock_t cve_spin_lock;
unsigned int cve_irq_flag;
unsigned int cve_irq_value;
static struct clk *cve_clk;
static struct cve_register regClk;

#define CVE_REGISTER_NUM 16
#define DMA_REGISTER_NUM 15
#define BIT_HIGH_8 0xff00
#define DMA_REGISTER_OFFSET 0x0050

#define CVE_MODULE_NAME "CVE"
#define CVE_MODULE_VERS "V1.0 Release"
#define CVE_MODULE_BRANCH "v5-dev"
#define CVE_MODULE_DATE "20170809"
#define CVE_MODULE_AUTHOR "yisongsong"
#define CVE_MODULE_COMMIT "a42a0d8376cf2008b8ee2b366fd6969068f3d779"

static int cve_reset(void)
{
	printk(KERN_INFO "[CVE_MOD_RESET] !!\n");
	writel(0x0000000f, cve_regs_init + 0x08);
	writel(0x0000000f, cve_regs_init + 0x58);
	mdelay(5);
	while (1) {
		r_data = readl(cve_regs_init + 0x28);
		r_data &= BIT0;
		if (r_data > 0)
			break;
	}
	writel(BIT8, cve_regs_init);
	writel(0x00000000, cve_regs_init);
	writel(BIT8, cve_regs_init + 0x50);
	writel(0x00000000, cve_regs_init + 0x50);
	mdelay(5);
	return 0;
}
static irqreturn_t cve_interrupt(unsigned long data)
{
	unsigned long list_status, list_mask;
	unsigned long dma_status, dma_mask;

	list_mask = readl(cve_regs_init + 0x04);
	list_mask = list_mask & 0x0f;

	list_status = readl(cve_regs_init + 0x08);
	list_int_flag = (list_status & list_mask);

	dma_mask = readl(cve_regs_init + 0x54);
	dma_mask = dma_mask & 0x07;

	dma_status = readl(cve_regs_init + 0x58);
	dma_int_flag = (dma_status & dma_mask);
	cve_int_flag = (list_int_flag << 16) | (dma_int_flag);

	if (cve_int_flag) {
		if ((cve_int_flag & ((BIT3<<16) | BIT2))) {
			if_error = 2;
			printk(KERN_ERR"[Driver] TIMEOUT: cve_int_flag = %#010lx\n",
				cve_int_flag);
			cve_reset();
			sunxi_periph_reset_assert(cve_clk);
			sunxi_periph_reset_deassert(cve_clk);
		} else if (cve_int_flag & (BIT0<<16)) {
			writel(BIT0, cve_regs_init + 0x08);
			if_error = 0;
		} else if (cve_int_flag & (BIT1<<16)) {
			writel(BIT1, cve_regs_init + 0x08);
			if_error = 0;
			cve_irq_value = 1;
			cve_irq_flag = 1;
			if_finish = 1;
			if (!if_sleep) {
				wake_up(&wait_cve);
				if_finish = 0;
			}
		} else if (cve_int_flag & BIT0) {
			writel(BIT0, cve_regs_init + 0x58);
			if_error = 0;
			cve_irq_value = 1;
			cve_irq_flag = 1;
			if_finish = 1;
			if (!if_sleep) {
				wake_up(&wait_cve);
				if_finish = 0;
			}
		} else {
			printk(KERN_INFO "[Driver] ERROR: Interrupt but cve_int_flag == 0!\n");
		}
	}
	return IRQ_HANDLED;
}

static int cve_fasync(int fd, struct file *filp, int mode)
{
	return fasync_helper(fd, filp, mode, &async_queue);
}

static int cvedev_open(struct inode *node, struct file *filp)
{
	if_run = 1;
	return 0;
}

static int cvedev_close(struct inode *node, struct file *filp)
{
	if_run = 0;
	return cve_fasync(-1, filp, 0);
}

static int cve_write(struct file *filp,
					  const char __user *buf,
					  size_t count,
					  loff_t *ppos)
{
	if (copy_from_user(&cve_end_flag,
						(void __user *)buf,
						sizeof(unsigned long))) {
		printk(KERN_ERR "Copy from user failed, addr is %#010x\n",
				(unsigned int)buf);
		return -EFAULT;
	}
	return 0;
}

static int cve_read(struct file *filp,
					char __user *buf,
					size_t count,
					loff_t *ppos)
{
	if (copy_to_user((char *)buf, &cve_int_flag, sizeof(unsigned long))) {
		printk(KERN_ERR "Copy to user failed,addr:%#010x\n", (unsigned int)buf);
		return -EFAULT;
	}
	cve_sig_sel = 0x00;
	return 0;
}

static void cve_open_clk(struct cve_register *regP, int en)
{
	struct cve_register reg = *regP;
	unsigned long rate;

	reg.value = reg.value | 0x80000000;
	rate = 24 * (1 + ((reg.value&BIT_HIGH_8)>>8));
	rate /= 1 + ((reg.value&0x2)>>1);
	rate /= 1 + (reg.value&0x1);
	if (clk_set_rate(cve_clk, rate*1000000) < 0)
		printk(KERN_INFO "[CVE]: Set clk failed, use default!\n");
	if (en > 0) {
		if (clk_prepare_enable(cve_clk) < 0) {
			printk(KERN_INFO "[CVE]: Open clk failed!\n");
			return;
		}
		sunxi_periph_reset_assert(cve_clk);
		sunxi_periph_reset_deassert(cve_clk);
	}
}

static void cve_close_clk(void)
{
	clk_disable_unprepare(cve_clk);
}

static long cve_ioctl(struct file *filp,
					  unsigned int cmd,
					  unsigned long arg)
{
	struct cve_register reg;
	unsigned long rData = 0;
	int cve_timeout = 0;
	unsigned long flags;

	if (unlikely(copy_from_user(&reg, (void __user *)arg,
				    sizeof(struct cve_register)))) {
		printk(KERN_ERR"Copy from user failed,addr:%#010x\n",
		       (unsigned int)arg);
		return -EFAULT;
	}

BACK_POINT:
	if (likely(cmd == CVE_WRITE_REGISTER)) {
		writel(reg.value, cve_regs_init + reg.addr);
		return 0;
	} else if (cmd == CVE_READ_RESNUM) {
		if (if_error > 0) {
			if (if_error == 2)
				goto Do_RESET;
			return 0;
		}
		return readl(cve_regs_init + reg.addr);
	}

	switch (cmd) {
	case CVE_READ_REGISTER:
		rData = readl(cve_regs_init + reg.addr);
			return	rData;
	case CVE_OPEN_CLK:
		/*sys_reset after open_clk*/
		cve_open_clk(&reg, 1);
		regClk = reg;
		break;
	case CVE_SYS_RESET:
		printk(KERN_INFO "[CVE_SYS_RESET] !!!\n");
		sunxi_periph_reset_assert(cve_clk);
		sunxi_periph_reset_deassert(cve_clk);
		break;
Do_RESET:
	case CVE_MOD_RESET:
		if_error = 1;
		cve_reset();
		goto BACK_POINT;

	case CVE_CLOSE_CLK:
		cve_close_clk();
		break;
	case CVE_PLL_SET:
		cve_open_clk(&reg, -1);
		break;
	case IOCTL_WAIT_CVE:
		/*cve_timeout = (int)arg;*/
		cve_timeout = 1;
		cve_irq_value = 0;
		spin_lock_irqsave(&cve_spin_lock, flags);
		if (cve_irq_flag)
			cve_irq_value = 1;
		spin_unlock_irqrestore(&cve_spin_lock, flags);
		wait_event_timeout(wait_cve, cve_irq_flag, cve_timeout*HZ);
		/*
		 * wait_event_interruptible(wait_cve, cve_irq_flag);
		 * printk(KERN_ERR"[Driver] IOCTL_WAIT_CVE:
		 *			cve_int_flag = %#010x\n", cve_int_flag);
		 * udelay(100);
		 */
		cve_irq_flag = 0;
		return cve_irq_value;
	default:
		break;
	}

	return 0;
}

static int sunxi_cve_show(struct seq_file *m, void *v)
{
	unsigned long rdata1, rdata2, rdata3;

	rdata1 = readl(cve_regs_init + 0x08);
	seq_printf(m, "-------------------MODULE STATUS-----------------\n");
	seq_printf(m, "           -----CVE Interrupt Stauts-----\n");
	seq_printf(m, "  BUSY     TIMEOUT  CMD_FINISH  CMD_LIST_FINISH\n");
	seq_printf(m, "   %ld        %ld          %ld          %ld\n\n",
				(rdata1&BIT16)>>16, (rdata1&BIT3)>>3,
				(rdata1&BIT0), (rdata1&BIT1)>>1);
	rdata1 = readl(cve_regs_init + 0x10);
	rdata2 = readl(cve_regs_init + 0x14);
	rdata3 = readl(cve_regs_init + 0x18);
	seq_printf(m, "  CVE List CMD Number , Doing CMD ID , Done CMD ID\n");
	seq_printf(m,  "         %ld              %ld            %ld\n\n",
				rdata1&0xff, rdata2&0xffff, rdata3&0xffff);
	rdata2 = readl(cve_regs_init + 0x58);
	seq_printf(m, "           -----DMA Interrupt Stauts-----\n");
	seq_printf(m, "  BUSY    TIMEOUT  FINISH\n");
	seq_printf(m, "   %ld        %ld      %ld\n\n",
				(rdata2&BIT16)>>16, (rdata2&BIT2)>>2,
				(rdata2)&BIT0);
	return 0;
}
static int sunxi_cve_open(struct inode *inode,
	struct file *file)
{
	return single_open(file, sunxi_cve_show, NULL);
}

/*fops for debugfs*/
static const struct file_operations cve_proc_fops = {
	.owner	=	THIS_MODULE,
	.open	=	sunxi_cve_open,
	.read	=	seq_read,
	.llseek =	seq_lseek,
	.release =	single_release,
};

static int cve_suspend(struct platform_device *pdev,
	pm_message_t state){

	int i = 0;
	unsigned long rData = 0;
	if (!if_run)
		return 0;
	cve_regs_temp = kmalloc_array(CVE_REGISTER_NUM,
					sizeof(u32), GFP_KERNEL);
	dma_regs_temp = kmalloc_array(DMA_REGISTER_NUM,
					sizeof(u32), GFP_KERNEL);

	if (IS_ERR_OR_NULL(cve_regs_temp) && IS_ERR_OR_NULL(dma_regs_temp)) {
		printk(KERN_INFO "[CVE]: Error: malloc failed!\n");
		goto CLOSE_;
	}
	for (i = 0; i < CVE_REGISTER_NUM; i++) {
		rData = readl(cve_regs_init + (i << 2));
		cve_regs_temp[i] = rData;
	}
	for (i = 0; i < DMA_REGISTER_NUM; i++) {
		rData = readl(cve_regs_init + DMA_REGISTER_OFFSET + (i << 2));
		dma_regs_temp[i] = rData;
	}
CLOSE_:
	if_sleep = 1;
	while (if_finish == 1)
		;
	cve_reset();
	cve_close_clk();
	return 0;
}

static int cve_resume(struct platform_device *pdev)
{

	int i = 0;

	if (!if_run)
		return 0;
	cve_open_clk(&regClk, 1);
	if (IS_ERR_OR_NULL(cve_regs_temp) && IS_ERR_OR_NULL(dma_regs_temp))
		return 0;
	for (i = 0; i < CVE_REGISTER_NUM; i++)
		writel(cve_regs_temp[i], cve_regs_init + (i << 2));
	for (i = 0; i < DMA_REGISTER_NUM; i++)
		writel(dma_regs_temp[i], cve_regs_init + DMA_REGISTER_OFFSET + (i << 2));
	kfree(cve_regs_temp);
	kfree(dma_regs_temp);
	cve_regs_temp = NULL;
	dma_regs_temp = NULL;
	if_sleep = 0;
	wake_up(&wait_cve);
	return 0;
}

static void cve_shutdown(struct platform_device *pdev)
{
}

static const struct of_device_id cve_of_match[] = {
	{ .compatible = "allwinner,sunxi-aie-cve", },
	{}
};
MODULE_DEVICE_TABLE(of, cve_of_match);

static struct platform_driver cve_pm_driver = {
	.driver		= {
		.name	= "CVE_PM",
		.owner	= THIS_MODULE,
		.of_match_table = cve_of_match,
	},
	.suspend	= cve_suspend,
	.resume		= cve_resume,
	.shutdown   = cve_shutdown,
};

static struct file_operations cve_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl	= cve_ioctl,
	.fasync = cve_fasync,
	.read =	cve_read,
	.write =	cve_write,
	.open = cvedev_open,
	.release = cvedev_close
};

static struct miscdevice cve_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name =	DEVICE_NAME,
	.fops = &cve_fops
};

static int __init cve_init(void)
{
	struct device_node *node;
	int r;

	cve_irq_flag = 0;
	cve_irq_value = 0;
	if_run = 0;
	spin_lock_init(&cve_spin_lock);
	r = misc_register(&cve_dev);
	if (r < 0)
		printk(KERN_INFO "CVE device register failed! %d\n", r);
	r = platform_driver_probe(&cve_pm_driver, NULL);
	if (r < 0)
		printk(KERN_INFO "CVE pm driver register failed! %d\n", r);
	node = of_find_compatible_node(NULL, NULL, "allwinner,sunxi-aie-cve");
	cve_irq_id = irq_of_parse_and_map(node, 0);
	cve_clk = of_clk_get(node, 1);
	if ((!cve_clk) || IS_ERR(cve_clk)) {
		printk(KERN_INFO "[CVE]: Error, can't find clk!\n");
		return -EFAULT;
	}
	r = request_irq(cve_irq_id, (irq_handler_t)cve_interrupt, 0, DEVICE_NAME, NULL);
	if (r < 0) {
		printk(KERN_INFO "Request CVE Irq error! return %d\n", r);
		return -EFAULT;
	}
	cve_regs_init = (unsigned int *)of_iomap(node, 0);
	debugfsP = debugfs_create_file("sunxi-cve", 0444, debugfs_mpp_root,
				NULL, &cve_proc_fops);
	if (IS_ERR_OR_NULL(debugfsP))
		printk(KERN_INFO "Warning: Fail to create debugfs node!\n");
	printk(KERN_INFO ">>>>>>>>>>>>>>> Computer Version Engine<<<<<<<<<<<<<<\n");
	printk(KERN_INFO "tag    :%s\n", CVE_MODULE_VERS);
	printk(KERN_INFO "branch :%s\n", CVE_MODULE_BRANCH);
	printk(KERN_INFO "commit :%s\n", CVE_MODULE_COMMIT);
	printk(KERN_INFO "date   :%s\n", CVE_MODULE_DATE);
	printk(KERN_INFO "author :%s\n", CVE_MODULE_AUTHOR);
	if_sleep = 0;
	if_finish = 0;
	return 0;
}

static void __exit cve_exit(void)
{
	free_irq(cve_irq_id, NULL);
	iounmap(cve_regs_init);
	clk_put(cve_clk);
	misc_deregister(&cve_dev);
	platform_driver_unregister(&cve_pm_driver);
	printk(KERN_INFO "CVE device has been removed!\n");
	if (!IS_ERR_OR_NULL(debugfsP))
		debugfs_remove(debugfsP);
};

module_init(cve_init);
module_exit(cve_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("yisongsong<yisongsong@allwinnertech.com>");
MODULE_DESCRIPTION("CVE device driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:CVE(AW1721)");
