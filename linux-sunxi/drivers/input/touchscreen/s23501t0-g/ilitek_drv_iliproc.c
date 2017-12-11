#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>

#include "ilitek_drv_common.h"
#include "ilitek_drv_iliproc.h"

/* definitions*/
#define ILITEK_FILE_DRIVER_NAME			"ilitek_file"

/*driver information*/
#define DERVER_VERSION_MAJOR 		2
#define DERVER_VERSION_MINOR 		0
#define CUSTOMER_ID 				0
#define MODULE_ID					0
#define PLATFORM_ID					0
#define PLATFORM_MODULE				0
#define ENGINEER_ID					0

/*define the application command*/
#define ILITEK_IOCTL_BASE                       100
#define ILITEK_IOCTL_I2C_WRITE_DATA             _IOWR(ILITEK_IOCTL_BASE, 0, unsigned char*)
#define ILITEK_IOCTL_I2C_WRITE_LENGTH           _IOWR(ILITEK_IOCTL_BASE, 1, int)
#define ILITEK_IOCTL_I2C_READ_DATA              _IOWR(ILITEK_IOCTL_BASE, 2, unsigned char*)
#define ILITEK_IOCTL_I2C_READ_LENGTH            _IOWR(ILITEK_IOCTL_BASE, 3, int)
#define ILITEK_IOCTL_USB_WRITE_DATA             _IOWR(ILITEK_IOCTL_BASE, 4, unsigned char*)
#define ILITEK_IOCTL_USB_WRITE_LENGTH           _IOWR(ILITEK_IOCTL_BASE, 5, int)
#define ILITEK_IOCTL_USB_READ_DATA              _IOWR(ILITEK_IOCTL_BASE, 6, unsigned char*)
#define ILITEK_IOCTL_USB_READ_LENGTH            _IOWR(ILITEK_IOCTL_BASE, 7, int)
#define ILITEK_IOCTL_DRIVER_INFORMATION		    _IOWR(ILITEK_IOCTL_BASE, 8, int)
#define ILITEK_IOCTL_USB_UPDATE_RESOLUTION      _IOWR(ILITEK_IOCTL_BASE, 9, int)
#define ILITEK_IOCTL_I2C_INT_FLAG	            _IOWR(ILITEK_IOCTL_BASE, 10, int)
#define ILITEK_IOCTL_I2C_UPDATE                 _IOWR(ILITEK_IOCTL_BASE, 11, int)
#define ILITEK_IOCTL_STOP_READ_DATA             _IOWR(ILITEK_IOCTL_BASE, 12, int)
#define ILITEK_IOCTL_START_READ_DATA            _IOWR(ILITEK_IOCTL_BASE, 13, int)
#define ILITEK_IOCTL_GET_INTERFANCE				_IOWR(ILITEK_IOCTL_BASE, 14, int)/*default setting is i2c interface*/
#define ILITEK_IOCTL_I2C_SWITCH_IRQ				_IOWR(ILITEK_IOCTL_BASE, 15, int)
#define ILITEK_IOCTL_UPDATE_FLAG				_IOWR(ILITEK_IOCTL_BASE, 16, int)
#define ILITEK_IOCTL_I2C_UPDATE_FW				_IOWR(ILITEK_IOCTL_BASE, 18, int)
#define ILITEK_IOCTL_RESET						_IOWR(ILITEK_IOCTL_BASE, 19, int)
#define ILITEK_IOCTL_INT_STATUS					_IOWR(ILITEK_IOCTL_BASE, 20, int)

#ifdef DEBUG_NETLINK
#define ILITEK_IOCTL_DEBUG_SWITCH				_IOWR(ILITEK_IOCTL_BASE, 21, int)
#endif /*DEBUG_NETLINK*/

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
#ifdef CONFIG_ENABLE_TOUCH_PIN_CONTROL
/*extern int MS_TS_MSG_IC_GPIO_RST;*/
extern int MS_TS_MSG_IC_GPIO_INT;
#endif /*CONFIG_ENABLE_TOUCH_PIN_CONTROL*/

#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
/*extern int MS_TS_MSG_IC_GPIO_RST;*/
extern int MS_TS_MSG_IC_GPIO_INT;
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD*/
#endif

extern u32 SLAVE_I2C_ID_DWI2C;
extern struct i2c_client *g_I2cClient;
extern u8 TOUCH_DRIVER_DEBUG_LOG_LEVEL;
extern bool g_GestureState;
extern u8 g_IsDisableFingerTouch;

/* device data*/
struct dev_data {
	/* device number*/
	dev_t devno;
	/* character device*/
	struct cdev cdev;
	/* class device*/
	struct class *class;
};
static struct dev_data dev;

/* declare file operations*/
struct file_operations ilitek_fops = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	.unlocked_ioctl = ilitek_file_ioctl,
#else
	.ioctl = ilitek_file_ioctl,
#endif
	.read = ilitek_file_read,
	.write = ilitek_file_write,
	.open = ilitek_file_open,
	.release = ilitek_file_close,
};


u32 driver_information[] = {DERVER_VERSION_MAJOR, DERVER_VERSION_MINOR, CUSTOMER_ID, MODULE_ID, PLATFORM_ID, PLATFORM_MODULE, ENGINEER_ID};

#ifdef DEBUG_NETLINK
static struct sock *netlink_sock;
bool debug_flag;
void udp_reply(int pid, int seq, void *payload, int size)
{
	struct sk_buff	*skb;
	struct nlmsghdr	*nlh;
	int		len = NLMSG_SPACE(size);
	void		*data;
	int ret;
	u8 buf[64] = {0};
	memcpy(buf, payload, 53);

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return;
	nlh = nlmsg_put(skb, pid, seq, 0, size, 0);
	nlh->nlmsg_flags = 0;
	data = NLMSG_DATA(nlh);
	memcpy(data, payload, size);
	NETLINK_CB(skb).portid = 0;         /* from kernel */
	NETLINK_CB(skb).dst_group = 0;  /* unicast */
	ret = netlink_unicast(netlink_sock, skb, pid, MSG_DONTWAIT);
	if (ret < 0) {
		printk("ilitek send failed\n");
		return;
	}
	return;
}

/* Receive messages from netlink socket. */
uint8_t	pid = 100, seq = 23/*, sid*/;
kuid_t uid;
static void udp_receive(struct sk_buff  *skb)
{
	void			*data;
	uint8_t buf[64] = {0};
	struct nlmsghdr *nlh;

	nlh = (struct nlmsghdr *)skb->data;
	pid  = 100;/*NETLINK_CREDS(skb)->pid;*/
	uid  = NETLINK_CREDS(skb)->uid;
	/*sid  = NETLINK_CB(skb).sid;*/
	seq  = 23;/*nlh->nlmsg_seq;*/
	data = NLMSG_DATA(nlh);
	/*printk("recv skb from user space uid:%d pid:%d seq:%d, sid:%d\n", uid, pid, seq, sid);*/
	if (!strcmp(data, "Hello World!")) {
		printk("data is :%s\n", (char *)data);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
		printk("recv skb from user space uid:%d pid:%d seq:%d\n", uid.val, pid, seq);
#else
		printk("recv skb from user space uid:%d pid:%d seq:%d\n", uid, pid, seq);
#endif
		printk("data is :%s\n", (char *)data);
		udp_reply(pid, seq, data, sizeof("Hello World!"));
		/*udp_reply(pid, seq, data);*/
	} else {
		memcpy(buf, data, 64);
	}
	/*kfree(data);*/
	return ;
}
#endif /*DEBUG_NETLINK*/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
long ilitek_file_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#else
int  ilitek_file_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
#endif
{
	static u8 szBuf[512] = {0};
	static u16 nLen = 0, i;
	s32 rc = 0;

	switch(cmd) {
	case ILITEK_IOCTL_I2C_WRITE_DATA:
		rc = copy_from_user(szBuf, (unsigned char*)arg, nLen);
		if (rc < 0) {
			printk("%s, copy data from user space, failed\n", __func__);
			return -1;
		}
		rc = IicWriteData(SLAVE_I2C_ID_DWI2C, &szBuf[0], nLen);
		if (rc < 0) {
			printk("%s, i2c write, failed\n", __func__);
			return -1;
		}
		break;
	case ILITEK_IOCTL_I2C_READ_DATA:
		rc = IicReadData(SLAVE_I2C_ID_DWI2C, szBuf, nLen);
		if (rc < 0) {
			printk("%s, i2c read, failed\n", __func__);
			return -1;
		}
		rc = copy_to_user((unsigned char*)arg, szBuf, nLen);

		if (rc < 0) {
			printk("%s, copy data to user space, failed\n", __func__);
			return -1;
		}
		break;
	case ILITEK_IOCTL_I2C_WRITE_LENGTH:
	case ILITEK_IOCTL_I2C_READ_LENGTH:
		nLen = arg;
		break;

	case ILITEK_IOCTL_DRIVER_INFORMATION:
		for(i = 0; i < 7; i++) {
			szBuf[i] = driver_information[i];
		}
		rc = copy_to_user((unsigned char*)arg, szBuf, 7);
		break;
	case ILITEK_IOCTL_I2C_SWITCH_IRQ:
		rc = copy_from_user(szBuf, (unsigned char*)arg, 1);
		if (szBuf[0] == 0) {
			DrvDisableFingerTouchReport();
		} else {
			DrvEnableFingerTouchReport();
		}
		break;
	case ILITEK_IOCTL_RESET:
		DrvTouchDeviceHwReset();
		break;
	case ILITEK_IOCTL_UPDATE_FLAG:
		rc = copy_from_user(szBuf, (unsigned char*)arg, 1);
		if (szBuf[0] == 1) {
			/*TBD*/
		} else if (szBuf[0] == 0) {
			/*TBD*/
			DrvTouchDeviceHwReset();

		} else {
			printk("unknow status:0x%02X\n", szBuf[0]);
		}
		break;
	case ILITEK_IOCTL_I2C_INT_FLAG:

		break;
	case ILITEK_IOCTL_START_READ_DATA:
		g_IsDisableFingerTouch = 0;
		printk("g_IsDisableFingerTouch = %d\n", g_IsDisableFingerTouch);
		break;
	case ILITEK_IOCTL_STOP_READ_DATA:
		g_IsDisableFingerTouch = 1;
		printk("g_IsDisableFingerTouch = %d\n", g_IsDisableFingerTouch);
		break;
	case ILITEK_IOCTL_INT_STATUS:
		put_user(gpio_get_value(MS_TS_MSG_IC_GPIO_INT), (int *)arg);
		break;
#ifdef DEBUG_NETLINK
	case ILITEK_IOCTL_DEBUG_SWITCH:
		rc = copy_from_user(szBuf, (unsigned char*)arg, 1);
		printk("ilitek The debug_flag = %d.\n", szBuf[0]);
		if (szBuf[0] == 0) {
			debug_flag = false;
		} else if (szBuf[0] == 1) {
			debug_flag = true;
		}
		break;
#endif /*DEBUG_NETLINK*/

	default:
		return -1;
		break;

	}
	return 0;
}

ssize_t ilitek_file_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{

	return 0;
}

ssize_t ilitek_file_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	int ret;
	unsigned char buffer[128] = {0};

	/* before sending data to touch device, we need to check whether the device is working or not*/

	/* check the buffer size whether it exceeds the local buffer size or not*/
	if (count > 128) {
		printk("%s, buffer exceed 128 bytes\n", __func__);
		return -1;
	}

	/* copy data from user space*/
	ret = copy_from_user(buffer, buf, count-1);
	if (ret < 0) {
		printk("%s, copy data from user space, failed", __func__);
		return -1;
	}

	/* parsing command*/
	if (strcmp(buffer, "dbg_enable") == 0) {
		TOUCH_DRIVER_DEBUG_LOG_LEVEL = 2;
	} else if (strcmp(buffer, "dbg_disable") == 0) {
		TOUCH_DRIVER_DEBUG_LOG_LEVEL = 0;
	} else if (strcmp(buffer, "irq_status") == 0) {
		printk("%s, gpio_get_value(i2c.irq_gpio) = %d.\n", __func__, gpio_get_value(MS_TS_MSG_IC_GPIO_INT));
	} else if (strcmp(buffer, "enable") == 0) {
		DrvEnableFingerTouchReport();
		printk("enable_irq\n");
	} else if (strcmp(buffer, "disable") == 0) {
		DrvDisableFingerTouchReport();
		printk("disable_irq\n");
	} else if (strcmp(buffer, "enable_report") == 0) {
		g_IsDisableFingerTouch = 0;
		printk("enable report\n");
	} else if (strcmp(buffer, "disable_report") == 0) {
		g_IsDisableFingerTouch = 1;
		printk("disable report\n");
	} else if (strcmp(buffer, "report_status") == 0) {
		printk("g_IsDisableFingerTouch = %d\n", g_IsDisableFingerTouch);
	} else if (strcmp(buffer, "enable_gesture") == 0) {
		g_GestureState = true;
		printk("enable gesture function\n");
	} else if (strcmp(buffer, "disable_gesture") == 0) {
		g_GestureState = false;
		printk("disable gesture function\n");
	}
#ifdef DEBUG_NETLINK
	else if (strcmp(buffer, "dbg_flag") == 0) {
		debug_flag = !debug_flag;
		printk("%s, %s debug_flag message(%X).\n", __func__, debug_flag?"Enabled":"Disabled", debug_flag);
	}
#endif /*DEBUG_NETLINK*/
	else if (strcmp(buffer, "reset") == 0) {
		DrvTouchDeviceHwReset();
		printk("end reset\n");
	}
	return count;
}

int ilitek_file_close(struct inode *inode, struct file *filp)
{
	return 0;
}

int ilitek_file_open(struct inode *inode, struct file *filp)
{
	return 0;
}

int DrvCreateDeviceNode(void)
{
	int ret;
#ifdef DEBUG_NETLINK
	struct netlink_kernel_cfg cfg = {
		.groups = 0,
		.input	= udp_receive,
	};
#endif /*DEBUG_NETLINK*/
	/* allocate character device driver buffer*/
	ret = alloc_chrdev_region(&dev.devno, 0, 1, ILITEK_FILE_DRIVER_NAME);
	if (ret) {
		printk("%s, can't allocate chrdev\n", __func__);
		return ret;
	}
	printk("%s, register chrdev(%d, %d)\n", __func__, MAJOR(dev.devno), MINOR(dev.devno));

	/* initialize character device driver*/
	cdev_init(&dev.cdev, &ilitek_fops);
	dev.cdev.owner = THIS_MODULE;
	ret = cdev_add(&dev.cdev, dev.devno, 1);
	if (ret < 0) {
		printk("%s, add character device error, ret %d\n", __func__, ret);
		return ret;
	}
	dev.class = class_create(THIS_MODULE, ILITEK_FILE_DRIVER_NAME);
	if (IS_ERR(dev.class)) {
		printk("%s, create class, error\n", __func__);
		return ret;
	}
	device_create(dev.class, NULL, dev.devno, NULL, "ilitek_ctrl");
	proc_create("ilitek_ctrl", 0666, NULL, &ilitek_fops);

#ifdef DEBUG_NETLINK
	netlink_sock = netlink_kernel_create(&init_net, 21, &cfg);
#endif /*DEBUG_NETLINK	*/
	return ret;
}
