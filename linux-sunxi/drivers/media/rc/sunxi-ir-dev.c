/* Copyright (C) 2014 ALLWINNERTECH
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
#include <linux/clk.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/irq.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <media/rc-core.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/arisc/arisc.h>

#include "sunxi-ir-rx.h"
#include "rc-core-priv.h"

#define SUNXI_IR_DRIVER_NAME	"sunxi-rc-recv"
#define SUNXI_IR_DEVICE_NAME	"sunxi-ir"

#define RC5_UNIT		889000  /* ns */
#define NEC_UNIT		562500
#define NEC_BOOT_CODE		(16 * NEC_UNIT)
#define NEC			0x0
#define RC5			0x1
#define RC5ANDNEC		0x2
DEFINE_IR_RAW_EVENT(rawir);
static struct rc_dev *sunxi_rcdev;
static u32 is_receiving;
static u32 threshold_low = RC5_UNIT + RC5_UNIT/2;
static u32 threshold_high = 2*RC5_UNIT + RC5_UNIT/2;
static bool pluse_pre;
static char protocol;
static bool boot_code;
static struct proc_dir_entry *sunxi_ir_protocol;
static int sunxi_ir_protocols;

static inline u8 ir_get_data(void __iomem *reg_base)
{
	return (u8)(readl(reg_base + IR_RXDAT_REG) & 0xff);
}

static inline u32 ir_get_intsta(void __iomem *reg_base)
{
	return readl(reg_base + IR_RXINTS_REG);
}

static inline void ir_clr_intsta(void __iomem *reg_base, u32 bitmap)
{
	u32 tmp = readl(reg_base + IR_RXINTS_REG);

	tmp &= ~0xff;
	tmp |= bitmap&0xff;
	writel(tmp, reg_base + IR_RXINTS_REG);
}

#ifdef CONFIG_OF
/* Translate OpenFirmware node properties into platform_data */
static struct of_device_id const sunxi_ir_recv_of_match[] = {
	{ .compatible = "allwinner,s_cir", },
	{ .compatible = "allwinner,ir", },
	{ },
};
MODULE_DEVICE_TABLE(of, sunxi_ir_recv_of_match);
#else /* !CONFIG_OF */
#endif
static void sunxi_ir_recv(u32 reg_data, struct sunxi_ir_data *ir_data)
{
	bool pluse_now = 0;
	u32 ir_duration = 0;

	pluse_now = reg_data >> 7; /* get the polarity */
	ir_duration = reg_data & 0x7f; /* get duration, number of clocks */

	if (pluse_pre == pluse_now) {
		/* the signal sunperposition */
		rawir.duration += ir_duration;
		pr_debug("raw: polar=%d; dur=%d\n", pluse_now, ir_duration);
	} else {
		if (is_receiving) {
			rawir.duration *= IR_SIMPLE_UNIT;
			pr_debug("pusle :polar=%d, dur: %u ns\n",
					rawir.pulse, rawir.duration);
			if (boot_code == 0) {
				boot_code = 1;
				if (eq_margin(rawir.duration, NEC_BOOT_CODE, NEC_UNIT*2)) {
					protocol = NEC;
					ir_raw_event_store(ir_data->rcdev, &rawir);
				} else {
					protocol = RC5;
					ir_raw_event_store(ir_data->rcdev, &rawir);
				}
			} else {
				if (((rawir.duration > threshold_low) &&
						(rawir.duration < threshold_high)) &&
						(protocol == RC5)) {
					rawir.duration = rawir.duration/2;
					ir_raw_event_store(ir_data->rcdev, &rawir);
					ir_raw_event_store(ir_data->rcdev, &rawir);
				} else
					ir_raw_event_store(ir_data->rcdev, &rawir);
			}
				rawir.pulse = pluse_now;
				rawir.duration = ir_duration;
				pr_debug("raw: polar=%d; dur=%d\n",
							pluse_now, ir_duration);
		} else {
				/* get the first pluse signal */
				rawir.pulse = pluse_now;
				rawir.duration = ir_duration;
				is_receiving = 1;
				pr_debug("get frist pulse,add head !!\n");
				pr_debug("raw: polar=%d; dur=%d\n", pluse_now, ir_duration);
		}
		pluse_pre = pluse_now;
	}
}
static irqreturn_t sunxi_ir_recv_irq(int irq, void *dev_id)
{
	struct sunxi_ir_data *ir_data = (struct sunxi_ir_data *)dev_id;
	u32 intsta, dcnt;
	u32 i = 0;
	u32 reg_data;

	pr_debug("IR RX IRQ Serve\n");

	intsta = ir_get_intsta(ir_data->reg_base);
	ir_clr_intsta(ir_data->reg_base, intsta);

	/* get the count of signal */
	dcnt =	(intsta>>8) & 0x7f;
	pr_debug("receive cnt :%d\n", dcnt);
	/* Read FIFO and fill the raw event */
	for (i = 0; i < dcnt; i++) {
		/* get the data from fifo */
		reg_data = ir_get_data(ir_data->reg_base);
		/* Byte in FIFO format YXXXXXXX(B)	Y:polarity(0:low level, 1:high level)  X:Number of clocks */
		sunxi_ir_recv(reg_data, ir_data);
	}

	if (intsta & IR_RXINTS_RXPE) {
		/* The last pulse can not call ir_raw_event_store() since miss invert level in above, manu call */
		if (rawir.duration) {
			rawir.duration *= IR_SIMPLE_UNIT;
			pr_debug("pusle :polar=%d, dur: %u ns\n",
						rawir.pulse, rawir.duration);
			ir_raw_event_store(ir_data->rcdev, &rawir);
		}
		pr_debug("handle raw data.\n");
		/* handle ther decoder theread */
		ir_raw_event_handle(ir_data->rcdev);
		is_receiving = 0;
		boot_code = 0;
		pluse_pre = false;
	}

	if (intsta & IR_RXINTS_RXOF) {
		/* FIFO Overflow */
		pr_err("ir_rx_irq_service: Rx FIFO Overflow!!\n");
		is_receiving = 0;
		boot_code = 0;
		pluse_pre = false;
	}

	return IRQ_HANDLED;
}


static void ir_mode_set(void __iomem *reg_base, enum ir_mode set_mode)
{
	u32 ctrl_reg = 0;

	switch (set_mode) {
	case CIR_MODE_ENABLE:
		ctrl_reg = readl(reg_base + IR_CTRL_REG);
		ctrl_reg |= IR_CIR_MODE;
		break;
	case IR_MODULE_ENABLE:
		ctrl_reg = readl(reg_base + IR_CTRL_REG);
		ctrl_reg |= IR_ENTIRE_ENABLE;
		break;
	case IR_BOTH_PULSE_MODE:
		ctrl_reg = readl(reg_base + IR_CTRL_REG);
		ctrl_reg |= IR_BOTH_PULSE;
		break;
	case IR_LOW_PULSE_MODE:
		ctrl_reg = readl(reg_base + IR_CTRL_REG);
		ctrl_reg |= IR_LOW_PULSE;
		break;
	case IR_HIGH_PULSE_MODE:
		ctrl_reg = readl(reg_base + IR_CTRL_REG);
		ctrl_reg |= IR_HIGH_PULSE;
		break;
	default:
		pr_err("ir_mode_set error!!\n");
		return;
	}
	writel(ctrl_reg, reg_base + IR_CTRL_REG);
}

static void ir_sample_config(void __iomem *reg_base,
					enum ir_sample_config set_sample)
{
	u32 sample_reg = 0;

	sample_reg = readl(reg_base + IR_SPLCFG_REG);

	switch (set_sample) {
	case IR_SAMPLE_REG_CLEAR:
		sample_reg = 0;
		break;
	case IR_CLK_SAMPLE:
		sample_reg |= IR_SAMPLE_DEV;
		break;
	case IR_FILTER_TH_NEC:
		sample_reg |= IR_RXFILT_VAL;
		break;
	case IR_FILTER_TH_RC5:
		sample_reg |= IR_RXFILT_VAL_RC5;
		break;
	case IR_IDLE_TH:
		sample_reg |= IR_RXIDLE_VAL;
		break;
	case IR_ACTIVE_TH:
		sample_reg |= IR_ACTIVE_T;
		sample_reg |= IR_ACTIVE_T_C;
		break;
	case IR_ACTIVE_TH_SAMPLE:
		sample_reg |= IR_ACTIVE_T_SAMPLE;
		sample_reg &= ~IR_ACTIVE_T_C;
		break;
	default:
		return;
	}
	writel(sample_reg, reg_base + IR_SPLCFG_REG);
}

static void ir_signal_invert(void __iomem *reg_base)
{
	u32 reg_value;
	reg_value = 0x1<<2;
	writel(reg_value, reg_base + IR_RXCFG_REG);
}

static void ir_irq_config(void __iomem *reg_base, enum ir_irq_config set_irq)
{
	u32 irq_reg = 0;

	switch (set_irq) {
	case IR_IRQ_STATUS_CLEAR:
		writel(0xef, reg_base + IR_RXINTS_REG);
		return;
	case IR_IRQ_ENABLE:
		irq_reg = readl(reg_base + IR_RXINTE_REG);
		irq_reg |= IR_IRQ_STATUS;
		break;
	case IR_IRQ_FIFO_SIZE:
		irq_reg = readl(reg_base + IR_RXINTE_REG);
		irq_reg |= IR_FIFO_20;
		break;
	default:
		return;
	}
	writel(irq_reg, reg_base + IR_RXINTE_REG);
}

static void ir_reg_cfg(void __iomem *reg_base)
{
	/* Enable IR Mode */
	ir_mode_set(reg_base, CIR_MODE_ENABLE);
	/* Config IR Smaple Register */
	ir_sample_config(reg_base, IR_SAMPLE_REG_CLEAR);
	ir_sample_config(reg_base, IR_CLK_SAMPLE);
	ir_sample_config(reg_base, IR_IDLE_TH); /* Set Idle Threshold */

	ir_sample_config(reg_base, IR_ACTIVE_TH_SAMPLE);         /* rc5 Set Active Threshold */
	ir_sample_config(reg_base, IR_FILTER_TH_RC5);		/* Set Filter Threshold */
	ir_signal_invert(reg_base);
	/* Clear All Rx Interrupt Status */
	ir_irq_config(reg_base, IR_IRQ_STATUS_CLEAR);
	/* Set Rx Interrupt Enable */
	ir_irq_config(reg_base, IR_IRQ_ENABLE);
	ir_irq_config(reg_base, IR_IRQ_FIFO_SIZE);	/* Rx FIFO Threshold = FIFOsz/2; */
	/* for NEC decode which start with high level in the header so should
	 * use IR_HIGH_PULSE_MODE mode, but some ICs don't support this function
	 * therefor use IR_BOTH_PULSE_MODE mode as default
	 */
	ir_mode_set(reg_base, IR_BOTH_PULSE_MODE);
	/* Enable IR Module */
	ir_mode_set(reg_base, IR_MODULE_ENABLE);
}

static void ir_clk_cfg(struct sunxi_ir_data *ir_data)
{

	unsigned long rate = 0;

	rate = clk_get_rate(ir_data->pclk);
	pr_debug("%s: get ir parent rate %dHZ\n", __func__, (__u32)rate);

	if (clk_set_parent(ir_data->mclk, ir_data->pclk))
		pr_err("%s: set ir_clk parent failed!\n", __func__);

	if (clk_set_rate(ir_data->mclk, IR_CLK))
		pr_err("set ir clock freq to %d failed!\n", IR_CLK);

	rate = clk_get_rate(ir_data->mclk);
	pr_debug("%s: get ir_clk rate %dHZ\n", __func__, (__u32)rate);

	if (clk_prepare_enable(ir_data->mclk))
		pr_err("try to enable ir_clk failed!\n");
}

static void ir_clk_uncfg(struct sunxi_ir_data *ir_data)
{

	if (IS_ERR(ir_data->mclk) || ir_data->mclk == NULL)
		pr_err("ir_clk handle is invalid, just return!\n");
	else {
		clk_disable_unprepare(ir_data->mclk);
		clk_put(ir_data->mclk);
		ir_data->mclk = NULL;
	}

	if (IS_ERR(ir_data->pclk) || ir_data->pclk == NULL)
		pr_err("ir_clk_source handle is invalid, just return!\n");
	else {
		clk_put(ir_data->pclk);
		ir_data->pclk = NULL;
	}
}

static void ir_setup(struct sunxi_ir_data *ir_data)
{
	pr_debug("ir_rx_setup: ir setup start!!\n");

	ir_clk_cfg(ir_data);
	ir_reg_cfg(ir_data->reg_base);

	pr_debug("ir_rx_setup: ir setup end!!\n");
}

static int sunxi_ir_startup(struct platform_device *pdev,
				struct sunxi_ir_data *ir_data)
{
	struct device_node *np = NULL;
	int ret = 0, i = 0;
	const char *name = NULL;
	char addr_name[32];

	np = pdev->dev.of_node;

	ir_data->reg_base = of_iomap(np, 0);
	if (ir_data->reg_base == NULL) {
		pr_err("%s:Failed to ioremap() io memory region.\n", __func__);
		ret = -EBUSY;
	} else
		pr_debug("ir base: %p !\n", ir_data->reg_base);

	ir_data->irq_num = irq_of_parse_and_map(np, 0);
	if (ir_data->irq_num == 0) {
		pr_err("%s:Failed to map irq.\n", __func__);
		ret = -EBUSY;
	} else
		pr_debug("ir irq num: %d !\n", ir_data->irq_num);

	ir_data->pclk = of_clk_get(np, 0);
	ir_data->mclk = of_clk_get(np, 1);
	if (IS_ERR(ir_data->pclk) || ir_data->pclk == NULL ||
		IS_ERR(ir_data->mclk) || ir_data->mclk == NULL) {
		pr_err("%s:Failed to get clk.\n", __func__);
		ret = -EBUSY;
	}
	if (of_property_read_u32(np, "ir_protocol_used", &ir_data->ir_protocols)) {
		pr_err("%s: get ir protocol failed", __func__);
		ir_data->ir_protocols = 0x0;
	}
	sunxi_ir_protocols = ir_data->ir_protocols;
	if (of_property_read_u32(np, "supply_vol", &ir_data->suply_vol))
		pr_err("%s: get cir supply_vol failed", __func__);

	if (of_property_read_string(np, "supply", &name))
		pr_err("%s: cir have no power supply\n", __func__);
	else {
		ir_data->suply = regulator_get(NULL, name);
		if (IS_ERR(ir_data->suply)) {
			pr_err("%s: cir get supply err\n", __func__);
			ir_data->suply = NULL;
		}
	}

	if (ir_data->ir_protocols == NEC) {
		for (i = 0; i < MAX_ADDR_NUM; i++) {
			sprintf(addr_name, "ir_addr_code%d", i);
			if (of_property_read_u32(np, (const char *)&addr_name,
						&ir_data->ir_addr[i])) {
				pr_err("node %s get failed!\n", addr_name);
				break;
			}
		}
		ir_data->ir_addr_cnt = i;
		for (i = 0; i < ir_data->ir_addr_cnt; i++) {
			sprintf(addr_name, "ir_power_key_code%d", i);
			if (of_property_read_u32(np, (const char *)&addr_name,
						&ir_data->ir_powerkey[i])) {
				pr_err("node %s get failed!\n", addr_name);
				break;
			}
		}
	} else if (ir_data->ir_protocols ==  RC5) {
		for (i = 0; i < MAX_ADDR_NUM; i++) {
			sprintf(addr_name, "rc5_ir_addr_code%d", i);
			if (of_property_read_u32(np, (const char *)&addr_name,
						&ir_data->ir_addr[i])) {
				pr_err("node %s get failed!\n", addr_name);
				break;
			}
		}
		ir_data->ir_addr_cnt = i;
		for (i = 0; i < ir_data->ir_addr_cnt; i++) {
			sprintf(addr_name, "rc5_ir_power_key_code%d", i);
			if (of_property_read_u32(np, (const char *)&addr_name,
						&ir_data->ir_powerkey[i])) {
				pr_err("node %s get failed!\n", addr_name);
				break;
			}
		}
	}

	return ret;
}

static ssize_t sunxi_ir_protocol_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	if (size != sizeof(unsigned int)) {
		pr_err("size != sizeof(unsigned int) \n");
		return -1;
	}
	if (copy_to_user((void __user *)buf, &sunxi_ir_protocols, size))
		return -1;
	return size;
}
static const struct file_operations sunxi_ir_proc_fops = {
		.read           = sunxi_ir_protocol_read,
};
static bool ir_protocol_judge(void)
{
	pr_err("sunxi ir_protocol_judge\n");
	sunxi_ir_protocol = proc_create("sunxi_ir_protocol", S_IRUSR, NULL, &sunxi_ir_proc_fops);
	if (!sunxi_ir_protocol)
		return true;
	return false;
}

static int sunxi_ir_recv_probe(struct platform_device *pdev)
{
	struct sunxi_ir_data *ir_data;
	int rc, ret = 0;
	char const ir_dev_name[] = "s_cir_rx";
	pr_debug("sunxi-ir probe start !\n");
	ir_data = kzalloc(sizeof(*ir_data), GFP_KERNEL);
	if (IS_ERR_OR_NULL(ir_data)) {
		pr_err("ir_data: not enough memory for ir data\n");
		return -ENOMEM;
	}
	if (pdev->dev.of_node)
		/* get dt and sysconfig */
		rc = sunxi_ir_startup(pdev, ir_data);
	else {
		pr_err("sunxi ir device tree err!\n");
		return -EBUSY;
	}

	if (rc < 0)
		goto err_allocate_device;

	sunxi_rcdev = rc_allocate_device();
	if (!sunxi_rcdev) {
		rc = -ENOMEM;
		pr_err("rc dev allocate fail !\n");
		goto err_allocate_device;
	}

	sunxi_rcdev->driver_type = RC_DRIVER_IR_RAW;
	sunxi_rcdev->input_name = SUNXI_IR_DEVICE_NAME;
	sunxi_rcdev->input_phys = SUNXI_IR_DEVICE_NAME "/input0";
	sunxi_rcdev->input_id.bustype = BUS_HOST;
	sunxi_rcdev->input_id.vendor = 0x0001;
	sunxi_rcdev->input_id.product = 0x0001;
	sunxi_rcdev->input_id.version = 0x0100;
	sunxi_rcdev->dev.parent = &pdev->dev;
	sunxi_rcdev->driver_name = SUNXI_IR_DRIVER_NAME;
	if (ir_data->ir_protocols == NEC)
		sunxi_rcdev->allowed_protocols = (u64)RC_BIT_NEC;
	if (ir_data->ir_protocols == RC5)
		sunxi_rcdev->allowed_protocols = (u64)RC_BIT_RC5;
	if (ir_data->ir_protocols == RC5ANDNEC)
		sunxi_rcdev->allowed_protocols = (u64)RC_BIT_RC5 | (u64)RC_BIT_NEC;

	sunxi_rcdev->map_name = RC_MAP_SUNXI;

	init_sunxi_ir_map();
	rc = rc_register_device(sunxi_rcdev);
	if (rc < 0) {
		dev_err(&pdev->dev, "failed to register rc device\n");
		goto err_register_rc_device;
	}
	sunxi_rcdev->enabled_protocols = sunxi_rcdev->allowed_protocols;
	sunxi_rcdev->input_dev->dev.init_name = &ir_dev_name[0];

	ir_data->rcdev = sunxi_rcdev;
	if (ir_data->suply) {
		rc = regulator_set_voltage(ir_data->suply, ir_data->suply_vol, ir_data->suply_vol);
		rc |= regulator_enable(ir_data->suply);
	}
	ir_setup(ir_data);
	platform_set_drvdata(pdev, ir_data);
	if (request_irq(ir_data->irq_num, sunxi_ir_recv_irq, 0, "RemoteIR_RX", ir_data)) {
		pr_err("%s: request irq fail.\n", __func__);
		rc = -EBUSY;
		goto err_request_irq;
	}

	/* Creat file node for android to show which protocol to used. */
	if (ir_protocol_judge()) {
		pr_err("%s: Failed to creat file node for android.\n", __func__);
		ret =  -EBUSY;
	}

	/* enable here */
	pr_debug("ir probe end!\n");

	return 0;

err_request_irq:
	platform_set_drvdata(pdev, NULL);
	rc_unregister_device(sunxi_rcdev);
	sunxi_rcdev = NULL;
	ir_clk_uncfg(ir_data);
	if (ir_data->suply) {
		regulator_disable(ir_data->suply);
		regulator_put(ir_data->suply);
	}
err_register_rc_device:
	exit_sunxi_ir_map();
	rc_free_device(sunxi_rcdev);
err_allocate_device:
	kfree(ir_data);
	return rc;
}

static int sunxi_ir_recv_remove(struct platform_device *pdev)
{
	struct sunxi_ir_data *ir_data = platform_get_drvdata(pdev);

	 proc_remove(sunxi_ir_protocol);

	free_irq(ir_data->irq_num, ir_data->rcdev);
	ir_clk_uncfg(ir_data);
	platform_set_drvdata(pdev, NULL);
	if (ir_data->suply) {
		regulator_disable(ir_data->suply);
		regulator_put(ir_data->suply);
	}
	exit_sunxi_ir_map();
	rc_unregister_device(ir_data->rcdev);
	kfree(ir_data);
	return 0;
}

#ifdef CONFIG_PM
static int sunxi_ir_recv_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sunxi_ir_data *ir_data = platform_get_drvdata(pdev);

	pr_debug("enter: sunxi_ir_rx_suspend.\n");

	disable_irq_nosync(ir_data->irq_num);

	if (NULL == ir_data->mclk || IS_ERR(ir_data->mclk)) {
		pr_err("ir_clk handle is invalid, just return!\n");
		return -1;
	}
	clk_disable_unprepare(ir_data->mclk);

	return 0;
}

static int sunxi_ir_recv_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sunxi_ir_data *ir_data = platform_get_drvdata(pdev);
	unsigned int ir_event = 0;

	pr_debug("enter: sunxi_ir_rx_resume.\n");

	arisc_query_wakeup_source(&ir_event);
	if (CPUS_WAKEUP_IR & ir_event) {
		rc_keydown(sunxi_rcdev, RC_TYPE_NEC, (ir_data->ir_addr[0] << 8) | ir_data->ir_powerkey[0], 0);
		msleep(1);
		rc_keyup(sunxi_rcdev);
	}

	clk_prepare_enable(ir_data->mclk);
	ir_reg_cfg(ir_data->reg_base);
	enable_irq(ir_data->irq_num);


	return 0;
}

static const struct dev_pm_ops sunxi_ir_recv_pm_ops = {
	.suspend        = sunxi_ir_recv_suspend,
	.resume         = sunxi_ir_recv_resume,
};
#endif

static struct platform_driver sunxi_ir_recv_driver = {
	.probe  = sunxi_ir_recv_probe,
	.remove = sunxi_ir_recv_remove,
	.driver = {
		.name   = SUNXI_IR_DRIVER_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(sunxi_ir_recv_of_match),
#ifdef CONFIG_PM
		.pm	= &sunxi_ir_recv_pm_ops,
#endif
	},
};
module_platform_driver(sunxi_ir_recv_driver);
MODULE_DESCRIPTION("SUNXI IR Receiver driver");
MODULE_AUTHOR("QIn");
MODULE_LICENSE("GPL v2");
