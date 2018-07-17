/*

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.


   Copyright (C) 2006-2007 - Motorola
   Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.

   Date         Author           Comment
   -----------  --------------   --------------------------------
   2006-Apr-28	Motorola	 The kernel module for running the Bluetooth(R)
				 Sleep-Mode Protocol from the Host side
   2006-Sep-08  Motorola         Added workqueue for handling sleep work.
   2007-Jan-24  Motorola         Added mbm_handle_ioi() call to ISR.

*/

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>

#include <linux/irq.h>
#include <linux/param.h>
#include <linux/bitops.h>
#include <linux/termios.h>
#include <linux/wakelock.h>
#include <linux/sunxi-gpio.h>
#include <linux/gpio.h>
//#include <linux/sys_config.h>
#include <linux/of_gpio.h>
#include <linux/power/scenelock.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include <linux/serial_core.h>
#include <linux/input.h>

#define BT_SLEEP_DBG
#undef  BT_DBG
#undef  BT_ERR
#ifdef BT_SLEEP_DBG
#define BT_DBG(fmt, arg...) \
printk(KERN_DEBUG "[BT_LPM] %s: " fmt "\n" , __func__ , ## arg)
#else
#define BT_DBG(fmt, arg...)
#endif
#define BT_ERR(fmt, arg...) \
printk(KERN_ERR "[BT_LPM] %s: " fmt "\n" , __func__ , ## arg)

/*
 * Defines
 */

#define VERSION		"1.3"
#define PROC_DIR	"bluetooth/sleep"

static int bluesleep_start(void);
static void bluesleep_stop(void);

struct bluesleep_info {
	unsigned host_wake;
	unsigned ext_wake;
	unsigned host_wake_irq;
	struct wake_lock wake_lock;
	struct uart_port *uport;
	unsigned host_wake_assert:1;
	unsigned ext_wake_assert:1;
};

static bool has_lpm_enabled;
static struct bluesleep_info *bsi;

/*
 * Global variables
 */
struct proc_dir_entry *bluetooth_dir, *sleep_dir;

/*
static struct input_dev *power_key_dev;
*/
static bool has_enter_inter;
static bool has_enter_suspend;
static bool wake_from_suspend;
/*
 * Local functions
 */

static int bluesleep_lpm_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "lpm enable: %d\n", has_lpm_enabled);
	return 0;
}

static int bluesleep_lpm_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bluesleep_lpm_proc_show, NULL);
}

static ssize_t bluesleep_write_proc_lpm(struct file *file,
					const char __user *buffer,
					size_t count, loff_t *pos)
{
	char b;

	if (count < 1)
		return -EINVAL;

	if (copy_from_user(&b, buffer, 1))
		return -EFAULT;

	if (b == '0') {
		if (has_lpm_enabled) {
			bluesleep_stop();
			has_lpm_enabled = false;
		}
	} else if (b == '1') {
		if (!has_lpm_enabled) {
			has_lpm_enabled = true;
			bluesleep_start();
		}
	} else
		BT_ERR("%s, error value: %c", __FUNCTION__, b);

	return count;
}

static int bluesleep_btwrite_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "it's not support\n");
	return 0;
}

static int bluesleep_btwrite_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bluesleep_btwrite_proc_show, NULL);
}

static ssize_t bluesleep_write_proc_btwrite(struct file *file,
					const char __user *buffer,
					size_t count, loff_t *pos)
{
	char b;

	if (count < 1)
		return -EINVAL;

	if (copy_from_user(&b, buffer, 1))
		return -EFAULT;

	if (b == '0')
		gpio_set_value(bsi->ext_wake, !(bsi->ext_wake_assert));
	else if (b == '1')
		gpio_set_value(bsi->ext_wake, bsi->ext_wake_assert);
	else
		BT_ERR("%s, error value: %c", __FUNCTION__, b);

	return count;
}

static const struct file_operations lpm_fops = {
	.owner		= THIS_MODULE,
	.open		= bluesleep_lpm_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= bluesleep_write_proc_lpm,
};

static const struct file_operations btwrite_fops = {
	.owner		= THIS_MODULE,
	.open		= bluesleep_btwrite_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= bluesleep_write_proc_btwrite,
};


static int bluesleep_suspend_proc_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static ssize_t bluesleep_suspend_proc_read(struct file *file,
					char __user *buf, size_t count, loff_t *ptr)
{
	signed char ret = 0;

	if (wake_from_suspend)
		ret = 1;
	else
		ret = 0;

	if (copy_to_user(buf, &ret, 1))
		return -EFAULT;

	return 1;
}

static ssize_t bluesleep_suspend_proc_write(struct file *file,
					const char __user *buffer,
					size_t count, loff_t *pos)
{
	char b;

	if (count < 1)
		return -EINVAL;

	if (copy_from_user(&b, buffer, 1))
		return -EFAULT;

	if (b == '1')
		wake_from_suspend = false;
	else
		BT_ERR("%s, error value: %c", __FUNCTION__, b);

	return count;
}

static const struct file_operations suspend_fops = {
	.owner		= THIS_MODULE,
	.open		= bluesleep_suspend_proc_open,
	.read		= bluesleep_suspend_proc_read,
	.write		= bluesleep_suspend_proc_write,
};

/**
 * Schedules a tasklet to run when receiving an interrupt on the
 * <code>HOST_WAKE</code> GPIO pin.
 * @param irq Not used.
 * @param dev_id Not used.
 */
static irqreturn_t bluesleep_hostwake_isr(int irq, void *dev_id)
{
/*
	BT_ERR("Enter interrupt function...");
*/
	has_enter_inter = true;

	return IRQ_HANDLED;
}

/**
 * Starts the Sleep-Mode Protocol on the Host.
 * @return On success, 0. On error, -1, and <code>errno</code> is set
 * appropriately.
 */
static int bluesleep_start(void)
{
	int retval;

	/* assert BT_WAKE */
	gpio_set_value(bsi->ext_wake, bsi->ext_wake_assert);
	retval = request_irq(bsi->host_wake_irq, bluesleep_hostwake_isr,
				 IRQF_TRIGGER_FALLING
				 | IRQF_NO_SUSPEND,
				"bluetooth hostwake", NULL);
	if (retval  < 0) {
		BT_ERR("Couldn't acquire BT_HOST_WAKE IRQ");
		goto fail;
	}

	retval = enable_wakeup_src(CPUS_GPIO_SRC, bsi->host_wake);
	if (retval < 0) {
		BT_ERR("Couldn't enable BT_HOST_WAKE as wakeup interrupt");
		free_irq(bsi->host_wake_irq, NULL);
		goto fail;
	}

	return 0;
fail:
	return retval;
}

static void bluesleep_stop(void)
{
	/* assert BT_WAKE */
	gpio_set_value(bsi->ext_wake, bsi->ext_wake_assert);

	if (disable_wakeup_src(CPUS_GPIO_SRC, bsi->host_wake))
		BT_ERR("Couldn't disable hostwake IRQ wakeup mode\n");
	free_irq(bsi->host_wake_irq, NULL);
}

static int __init bluesleep_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct gpio_config config;
	int ret;

	bsi = devm_kzalloc(&pdev->dev, sizeof(struct bluesleep_info),
			GFP_KERNEL);
	if (!bsi)
		return -ENOMEM;

	bsi->host_wake = of_get_named_gpio_flags(np, "bt_hostwake", 0,
					(enum of_gpio_flags *)&config);
	if (!gpio_is_valid(bsi->host_wake)) {
		BT_ERR("get gpio bt_hostwake failed\n");
		return -EINVAL;
	}

	BT_DBG("bt_hostwake gpio=%d mul-sel=%d pull=%d drv_level=%d data=%d\n",
			config.gpio,
			config.mul_sel,
			config.pull,
			config.drv_level,
			config.data);

	ret = devm_gpio_request(dev, bsi->host_wake, "bt_hostwake");
	if (ret < 0) {
		BT_ERR("can't request bt_hostwake gpio %d\n",
			bsi->host_wake);
		return ret;
	}
	ret = gpio_direction_input(bsi->host_wake);
	if (ret < 0) {
		BT_ERR("can't request input direction bt_wake gpio %d\n",
			bsi->host_wake);
		return ret;
	}

	bsi->ext_wake = of_get_named_gpio_flags(np, "bt_wake", 0,
					(enum of_gpio_flags *)&config);
	if (!gpio_is_valid(bsi->ext_wake)) {
		BT_ERR("get gpio bt_wake failed\n");
		return -EINVAL;
	}

	BT_DBG("bt_wake gpio=%d  mul-sel=%d  pull=%d  drv_level=%d  data=%d\n",
			config.gpio,
			config.mul_sel,
			config.pull,
			config.drv_level,
			config.data);

	ret = devm_gpio_request(dev, bsi->ext_wake, "bt_wake");
	if (ret < 0) {
		BT_ERR("can't request bt_wake gpio %d\n",
			bsi->ext_wake);
		return ret;
	}

	/* 1.set bt_wake as output and the level is assert, assert bt wake */
	ret = gpio_direction_output(bsi->ext_wake, bsi->ext_wake_assert);
	if (ret < 0) {
		BT_ERR("can't request output direction bt_wake gpio %d\n",
			bsi->ext_wake);
		return ret;
	}

	/* set ext_wake_assert and host_wake_assert */
	bsi->ext_wake_assert = bsi->host_wake_assert = config.data;

	/* 2.get bt_host_wake gpio irq */
	bsi->host_wake_irq = gpio_to_irq(bsi->host_wake);
	if (IS_ERR_VALUE(bsi->host_wake_irq)) {
		BT_ERR("map gpio [%d] to virq failed, errno = %d\n",
		bsi->host_wake, bsi->host_wake_irq);
		ret = -ENODEV;
		return ret;
	}

	/* register input device */
	/*
	power_key_dev = input_allocate_device();
	if (!power_key_dev) {
		BT_ERR("ir_dev: not enough memory for input device\n");
		return -ENOMEM;
	}

	power_key_dev->name = "bt-powerkey";
	power_key_dev->id.bustype = BUS_HOST;

	power_key_dev->evbit[0] = BIT_MASK(EV_KEY);
	set_bit(KEY_POWER, power_key_dev->keybit);

	ret = input_register_device(power_key_dev);
	if (ret) {
		input_free_device(power_key_dev);
		BT_ERR("ir_rx_init: register input device exception, exit\n");
		return -EBUSY;
	}
	*/

	has_enter_inter = false;
	has_enter_suspend = false;
	wake_from_suspend = false;

	return 0;
}

static int bluesleep_remove(struct platform_device *pdev)
{
	/* assert bt wake */
	gpio_set_value(bsi->ext_wake, bsi->ext_wake_assert);
	if (has_lpm_enabled) {
		if (disable_wakeup_src(CPUS_GPIO_SRC, bsi->host_wake))
			BT_ERR("Couldn't disable hostwake IRQ wakeup mode\n");
		free_irq(bsi->host_wake_irq, NULL);
	}

	has_lpm_enabled = false;
	return 0;
}

static int bluesleep_resume(struct platform_device *pdev)
{
	BT_ERR("bluesleep_resume.\n");

	if (has_enter_suspend && has_enter_inter)
		wake_from_suspend = true;

	has_enter_suspend = false;
	has_enter_inter = false;

	return 0;
}

static int bluesleep_suspend(struct platform_device *pdev, pm_message_t state)
{
	BT_ERR("bluesleep_suspend.\n");

	has_enter_inter = false;
	has_enter_suspend = true;
	wake_from_suspend = false;

	return 0;
}

static const struct of_device_id sunxi_btlpm_ids[] = {
	{ .compatible = "allwinner,sunxi-btlpm" },
	{ /* Sentinel */ }
};

static struct platform_driver bluesleep_driver = {
	.remove	= bluesleep_remove,
	.suspend = bluesleep_suspend,
	.resume = bluesleep_resume,
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "sunxi-btlpm",
		.of_match_table	= sunxi_btlpm_ids,
	},
};

static int __init bluesleep_init(void)
{
	int retval;
	struct proc_dir_entry *ent;

	BT_DBG("BlueSleep Mode Driver Ver %s", VERSION);

	retval = platform_driver_probe(&bluesleep_driver, bluesleep_probe);
	if (retval)
		return retval;

	bluetooth_dir = proc_mkdir("bluetooth", NULL);
	if (bluetooth_dir == NULL) {
		BT_ERR("Unable to create /proc/bluetooth directory");
		return -ENOMEM;
	}

	sleep_dir = proc_mkdir("sleep", bluetooth_dir);
	if (sleep_dir == NULL) {
		BT_ERR("Unable to create /proc/%s directory", PROC_DIR);
		return -ENOMEM;
	}

	ent = proc_create("lpm", 0, sleep_dir, &lpm_fops);
	if (ent == NULL) {
		BT_ERR("Unable to create /proc/%s/lpm entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}

	ent = proc_create("btwrite", 0, sleep_dir, &btwrite_fops);
	if (ent == NULL) {
		BT_ERR("Unable to create /proc/%s/btwrite entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}

	ent = proc_create("suspend", 0, sleep_dir, &suspend_fops);
	if (ent == NULL) {
		BT_ERR("Unable to create /proc/%s/suspend entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}

	return 0;

fail:
	remove_proc_entry("suspend", sleep_dir);
	remove_proc_entry("btwrite", sleep_dir);
	remove_proc_entry("lpm", sleep_dir);
	remove_proc_entry("sleep", bluetooth_dir);
	remove_proc_entry("bluetooth", 0);
	return retval;
}

static void __exit bluesleep_exit(void)
{
	platform_driver_unregister(&bluesleep_driver);

	remove_proc_entry("suspend", sleep_dir);
	remove_proc_entry("btwrite", sleep_dir);
	remove_proc_entry("lpm", sleep_dir);
	remove_proc_entry("sleep", bluetooth_dir);
	remove_proc_entry("bluetooth", 0);
}

module_init(bluesleep_init);
module_exit(bluesleep_exit);

MODULE_DESCRIPTION("Bluetooth Sleep Mode Driver ver %s " VERSION);
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
