
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
#include <linux/power/axp_depend.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include <linux/serial_core.h>
#include "hci_uart.h"

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


#define VERSION		"1.2"
#define PROC_DIR	"bluetooth/sleep"

#define DEFAULT_UART_INDEX   1
#define BT_BLUEDROID_SUPPORT 1
#define BT_ENABLE_IRQ_WAKE 0
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

/* work function */
static void bluesleep_sleep_work(struct work_struct *work);

/* work queue */
DECLARE_DELAYED_WORK(sleep_workqueue, bluesleep_sleep_work);

/* Macros for handling sleep work */
#define bluesleep_rx_busy()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_tx_busy()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_rx_idle()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_tx_idle()     schedule_delayed_work(&sleep_workqueue, 0)

/* state variable names and bit positions */
#define BT_PROTO	0x01
#define BT_TXDATA	0x02
#define BT_ASLEEP	0x04
#define BT_SUSPEND	0x10

static bool has_lpm_enabled;

static struct platform_device *bluesleep_uart_dev;

static struct bluesleep_info *bsi;

/* module usage */
static atomic_t open_count = ATOMIC_INIT(1);

/*
 * Global variables
 */

/** Global state flags */
static unsigned long flags;

/** Tasklet to respond to change in hostwake line */
static struct tasklet_struct hostwake_task;

/** Lock for state transitions */
static spinlock_t rw_lock;

struct proc_dir_entry *bluetooth_dir, *sleep_dir;


static void bluesleep_sleep_work(struct work_struct *work)
{
    if (gpio_get_value(bsi->ext_wake) != bsi->ext_wake_assert) {
		gpio_set_value(bsi->ext_wake, bsi->ext_wake_assert);
	}
}

static void bluesleep_hostwake_task(unsigned long data)
{
	/* BT_DBG("hostwake line change"); */
	spin_lock(&rw_lock);

	bluesleep_rx_idle();

	spin_unlock(&rw_lock);
}

/**
 * Handles proper timer action when outgoing data is delivered to the
 * HCI line discipline. Sets BT_TXDATA.
 */
static void bluesleep_outgoing_data(void)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&rw_lock, irq_flags);

	if (gpio_get_value(bsi->ext_wake) != bsi->ext_wake_assert) {
		gpio_set_value(bsi->ext_wake, bsi->ext_wake_assert);
	}

	spin_unlock_irqrestore(&rw_lock, irq_flags);
}

static struct uart_port *bluesleep_get_uart_port(void)
{
	struct uart_port *uport = NULL;
	if (bluesleep_uart_dev) {
		uport = platform_get_drvdata(bluesleep_uart_dev);
		if (uport)
			BT_DBG("%s get uart_port: %s, port irq: %d",
					__func__, bluesleep_uart_dev->name,
					uport->irq);
	}
	return uport;
}

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
		/* HCI_DEV_UNREG */
		bluesleep_stop();
		has_lpm_enabled = false;
		bsi->uport = NULL;
	} else {
		/* HCI_DEV_REG */
		if (!has_lpm_enabled) {
			has_lpm_enabled = true;
			if (bluesleep_uart_dev)
				bsi->uport = bluesleep_get_uart_port();

			/* if bluetooth started, start bluesleep*/
			bluesleep_start();
		}
	}

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

	/* HCI_DEV_WRITE */
	if (b != '0')
		bluesleep_outgoing_data();

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


/**
 * Schedules a tasklet to run when receiving an interrupt on the
 * <code>HOST_WAKE</code> GPIO pin.
 * @param irq Not used.
 * @param dev_id Not used.
 */
static irqreturn_t bluesleep_hostwake_isr(int irq, void *dev_id)
{
	BT_DBG("Enter interrupt function...");

	/* schedule a tasklet to handle the change in the host wake line */
	tasklet_schedule(&hostwake_task);
	wake_lock(&bsi->wake_lock);
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
	unsigned long irq_flags;

	spin_lock_irqsave(&rw_lock, irq_flags);

	if (test_bit(BT_PROTO, &flags)) {
		spin_unlock_irqrestore(&rw_lock, irq_flags);
		return 0;
	}

	spin_unlock_irqrestore(&rw_lock, irq_flags);

	if (!atomic_dec_and_test(&open_count)) {
		atomic_inc(&open_count);
		return -EBUSY;
	}

	/* assert BT_WAKE */
	gpio_set_value(bsi->ext_wake, bsi->ext_wake_assert);

    retval = axp_gpio_irq_request(0, bsi->host_wake, bluesleep_hostwake_isr, NULL);
    if (IS_ERR_VALUE(retval)) {
		BT_ERR("sunxi bt axp gpio request irq failed\n");
		goto fail;
	}
	axp_gpio_irq_set_type(0, bsi->host_wake, AXP_GPIO_IRQF_TRIGGER_RISING);
	axp_gpio_irq_enable(0, bsi->host_wake);

#if BT_ENABLE_IRQ_WAKE
	retval = enable_wakeup_src(CPUS_GPIO_SRC, bsi->host_wake);
	if (retval < 0) {
		BT_ERR("Couldn't enable BT_HOST_WAKE as wakeup interrupt");
		free_irq(bsi->host_wake_irq, NULL);
		goto fail;
	}
#endif
	set_bit(BT_PROTO, &flags);
	wake_lock(&bsi->wake_lock);

	return 0;
fail:
	atomic_inc(&open_count);

	return retval;
}

/**
 * Stops the Sleep-Mode Protocol on the Host.
 */
static void bluesleep_stop(void)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&rw_lock, irq_flags);

	if (!test_bit(BT_PROTO, &flags)) {
		spin_unlock_irqrestore(&rw_lock, irq_flags);
		return;
	}

	/* assert BT_WAKE */
	gpio_set_value(bsi->ext_wake, bsi->ext_wake_assert);

	clear_bit(BT_PROTO, &flags);

	if (test_bit(BT_ASLEEP, &flags)) {
		clear_bit(BT_ASLEEP, &flags);
	}

	atomic_inc(&open_count);

	spin_unlock_irqrestore(&rw_lock, irq_flags);
#if BT_ENABLE_IRQ_WAKE
	if (disable_wakeup_src(CPUS_GPIO_SRC, bsi->host_wake))
		BT_ERR("Couldn't disable hostwake IRQ wakeup mode\n");
#endif
	axp_gpio_irq_free(0, bsi->host_wake);
	wake_lock_timeout(&bsi->wake_lock, HZ / 2);
}

extern struct platform_device *sw_uart_get_pdev(int uart_id);
static int __init bluesleep_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct gpio_config config;
	int ret, uart_index;
	u32 val;

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

	uart_index = DEFAULT_UART_INDEX;
	if (!of_property_read_u32(np, "uart_index", &val)) {
		switch (val) {
		case 0:
		case 1:
		case 2:
		case 3:
			uart_index = val;
			break;
		default:
			BT_ERR("unsupported uart_index (%u)\n", val);
		}
	}
	BT_DBG("uart_index (%u)\n", uart_index);
	bluesleep_uart_dev = sw_uart_get_pdev(uart_index);

	wake_lock_init(&bsi->wake_lock, WAKE_LOCK_SUSPEND, "bluesleep");
	return 0;
}

static int bluesleep_remove(struct platform_device *pdev)
{
	/* assert bt wake */
	gpio_set_value(bsi->ext_wake, bsi->ext_wake_assert);
	if (test_bit(BT_PROTO, &flags)) {
#if BT_ENABLE_IRQ_WAKE
		if (disable_wakeup_src(CPUS_GPIO_SRC, bsi->host_wake))
			BT_ERR("Couldn't disable hostwake IRQ wakeup mode\n");
#endif
		axp_gpio_irq_free(0, bsi->host_wake);
	}

	wake_lock_destroy(&bsi->wake_lock);
	return 0;
}

static int bluesleep_resume(struct platform_device *pdev)
{
	BT_ERR("bluesleep_resume.\n");

/* TODO: disable irq */

	if (test_bit(BT_SUSPEND, &flags)) {
/*
		if ((bsi->uport != NULL) &&
			(gpio_get_value(bsi->host_wake) == bsi->host_wake_assert)) {
			bsi->uport->ops->set_mctrl(bsi->uport, TIOCM_RTS);
		}
*/
		clear_bit(BT_SUSPEND, &flags);
	}
	return 0;
}

static int bluesleep_suspend(struct platform_device *pdev, pm_message_t state)
{
	BT_ERR("bluesleep_suspend.\n");

/* TODO: enable irq. */

	set_bit(BT_SUSPEND, &flags);
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

/**
 * Initializes the module.
 * @return On success, 0. On error, -1, and <code>errno</code> is set
 * appropriately.
 */
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

#if BT_BLUEDROID_SUPPORT
	/* read/write proc entries */
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
#endif

	flags = 0; /* clear all status bits */

	/* Initialize spinlock. */
	spin_lock_init(&rw_lock);

	/* initialize host wake tasklet */
	tasklet_init(&hostwake_task, bluesleep_hostwake_task, 0);

	return 0;

fail:
#if BT_BLUEDROID_SUPPORT
	remove_proc_entry("btwrite", sleep_dir);
	remove_proc_entry("lpm", sleep_dir);
#endif
	remove_proc_entry("sleep", bluetooth_dir);
	remove_proc_entry("bluetooth", 0);
	return retval;
}

/**
 * Cleans up the module.
 */
static void __exit bluesleep_exit(void)
{
	platform_driver_unregister(&bluesleep_driver);

#if BT_BLUEDROID_SUPPORT
	remove_proc_entry("btwrite", sleep_dir);
	remove_proc_entry("lpm", sleep_dir);
#endif
	remove_proc_entry("sleep", bluetooth_dir);
	remove_proc_entry("bluetooth", 0);
}

module_init(bluesleep_init);
module_exit(bluesleep_exit);

MODULE_DESCRIPTION("Bluetooth Sleep Mode Driver ver %s " VERSION);
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
