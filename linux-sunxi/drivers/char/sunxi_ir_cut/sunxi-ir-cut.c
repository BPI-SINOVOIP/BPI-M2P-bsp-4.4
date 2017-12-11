/* Copyright (C) 2017 ALLWINNERTECH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/sunxi-gpio.h>

#define SUNXI_IR_CUT_NAME	"sunxi-ir-cut"

#define ENABLE_IR_CUT 1
#define DISABLE_IR_CUT 0
#define IR_CUT_MAX  10

wait_queue_head_t ir_cut_queue;
EXPORT_SYMBOL_GPL(ir_cut_queue);

int ir_cut_condition, ir_cut_data, ir_cut_volt_data;
EXPORT_SYMBOL_GPL(ir_cut_condition);
EXPORT_SYMBOL_GPL(ir_cut_data);
EXPORT_SYMBOL_GPL(ir_cut_volt_data);

struct ircut_data {
	struct gpio_config gpioconfig;
	int gpio;
};

static struct ircut_data ircutdat[IR_CUT_MAX];
static int ir_cut_num;

static struct cdev ir_cut_cdev;
static int ir_cut_major;
static struct class *ir_cut_class;

static void ir_cut_enable(void)
{
	int i = 0;

	for (i = 0; i < IR_CUT_MAX; i++) {
		if (gpio_is_valid(ircutdat[i].gpio))
			__gpio_set_value(ircutdat[i].gpio, 1);
	}
}

static void ir_cut_disable(void)
{
	int i = 0;

	for (i = 0; i < IR_CUT_MAX; i++) {
		if (gpio_is_valid(ircutdat[i].gpio))
			__gpio_set_value(ircutdat[i].gpio, 0);
	}
}

static int ir_cut_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t ir_cut_fops_write(struct file *filp, const char __user *ubuf,
		size_t count, loff_t *offp)
{
	int buf_get;

	if (count == 0) {
		return 0;
	}

	if (copy_from_user((void *)&buf_get, (const void __user *)ubuf, count)) {
		return -1;
	}

	if (buf_get == ENABLE_IR_CUT) {
		ir_cut_enable();
	} else if (buf_get == DISABLE_IR_CUT) {
		ir_cut_disable();
	}

	return count;
}

static ssize_t ir_cut_fops_read(struct file *filp, char __user *ubuf,
		size_t count, loff_t *offp)
{
	int err = 0;

	if (count == 0) {
		return 0;
	}

	if (ir_cut_data == 0) {
		if (filp->f_flags & O_NONBLOCK) {
			return -1;
		} else {
			wait_event_interruptible(ir_cut_queue, ir_cut_condition);
		}
	}

	err = copy_to_user(ubuf, (void *)&ir_cut_volt_data, 4);

	ir_cut_condition = 0;
	ir_cut_data = 0;

	return count;
}

static int sunxi_ir_cut_startup(struct platform_device *pdev)
{
	struct device_node *node = NULL;
	int gpio = 0, i = 0;
	u32 ret = 0;
	char main_key[256];

	node = pdev->dev.of_node;
	if (!node) {
		return -1;
	}

	ret = of_property_read_u32(node, "ir_cut_num", &ir_cut_num);
	if (ret || !ir_cut_num) {
		pr_err("these is ir_cut number \n");
		return -1;
	}

	for (i = 0; i < IR_CUT_MAX; i++) {
		snprintf(main_key, sizeof(main_key), "ir_cut_gpio%d", i);
		gpio = of_get_named_gpio_flags(node, main_key, 0, (enum of_gpio_flags *)(&ircutdat[i].gpioconfig));
		if (!gpio_is_valid(gpio))
			continue;

		if (devm_gpio_request(&pdev->dev, gpio, "IR_CUT") < 0)
			continue;

		ircutdat[i].gpio = gpio;
		gpio_direction_output(gpio, 0);
	}

	return 0;
}

static const struct file_operations ir_cut_fops = {
	.owner = THIS_MODULE,
	.open  = ir_cut_open,
	.write = ir_cut_fops_write,
	.read  = ir_cut_fops_read,
};

static int sunxi_ir_cut_probe(struct platform_device *pdev)
{
	int result = 0;
	dev_t devno;

	pr_debug("[%s]: ir_cut_probe start!!\n", __func__);
	sunxi_ir_cut_startup(pdev);
	result = alloc_chrdev_region(&devno, 0, 1, SUNXI_IR_CUT_NAME);
	if (result < 0) {
		return -1;
	}
	ir_cut_major = MAJOR(devno);
	cdev_init(&ir_cut_cdev, &ir_cut_fops);
	cdev_add(&ir_cut_cdev, devno, 1);

	ir_cut_class = class_create(THIS_MODULE, "ircut");
	device_create(ir_cut_class, NULL, MKDEV(ir_cut_major, 0),  NULL, SUNXI_IR_CUT_NAME);

	init_waitqueue_head(&ir_cut_queue);

	pr_debug("[%s]: ir_cut_probe end!!\n", __func__);
	return 0;
}

static int sunxi_ir_cut_remove(struct platform_device *pdev)
{
	device_destroy(ir_cut_class, MKDEV(ir_cut_major, 0));
	class_destroy(ir_cut_class);
	cdev_del(&ir_cut_cdev);
	unregister_chrdev_region(MKDEV(ir_cut_major, 0), 1);

	return 0;
}

#ifdef CONFIG_OF
/* Translate OpenFirmware node properties into platform_data */
static struct of_device_id const sunxi_ir_cut_of_match[] = {
	{ .compatible = "allwinner,ir_cut", },
	{ },
};
/*MODULE_DEVICE_TABLE(of, sunxi_ir_recv_of_match);*/
#else /* !CONFIG_OF */
#endif

static struct platform_driver sunxi_ir_cut_driver = {
	.probe  = sunxi_ir_cut_probe,
	.remove = sunxi_ir_cut_remove,
	.driver = {
		.name   = SUNXI_IR_CUT_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(sunxi_ir_cut_of_match),
	},
};

module_platform_driver(sunxi_ir_cut_driver);
MODULE_DESCRIPTION("SUNXI IR Receiver driver");
MODULE_AUTHOR("lee");
MODULE_LICENSE("GPL v2");
