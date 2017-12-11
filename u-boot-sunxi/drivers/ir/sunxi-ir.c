/*
 * (C) Copyright 2007-2016
 * Allwinnertech Technology Co., Ltd <www.allwinnertech.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/arch/platform.h>
#include <asm/arch/intc.h>
#include <sys_config.h>
#include <fdt_support.h>

#include "sunxi-ir.h"
#include "asm/arch/timer.h"
#include <sys_config_old.h>


DECLARE_GLOBAL_DATA_PTR;

DEFINE_IR_RAW_EVENT(rawir);

struct ir_raw_buffer sunxi_ir_raw;

static bool pluse_pre = 0;
static u32 is_receiving = 0;

extern struct timer_list ir_timer_t;

#define ONEKEY_USB_RECOVERY_MODE			(0x01)
#define ONEKEY_SPRITE_RECOVERY_MODE			(0x02)
#define USB_RECOVERY_KEY_VALUE				(0x81)
#define SPRITE_RECOVERY_KEY_VALUE			(0X82)
#define IR_BOOT_RECOVERY_VALUE				(0x88)


static inline void ir_reset_rawbuffer(void)
{
	sunxi_ir_raw.dcnt = 0;
}

static inline u8 ir_get_data(void)
{
	return (u8)(readl(IR_BASE + IR_RXDAT_REG));
}

static inline u32 ir_get_intsta(void)
{
	return (readl(IR_BASE + IR_RXINTS_REG));
}

static inline void ir_clr_intsta(u32 bitmap)
{
	u32 tmp = readl(IR_BASE + IR_RXINTS_REG);

	tmp &= ~0xff;
	tmp |= bitmap&0xff;
	writel(tmp, IR_BASE + IR_RXINTS_REG);
}

static void ir_mode_set(enum ir_mode set_mode)
{
	u32 ctrl_reg = 0;

	switch (set_mode) {
		case CIR_MODE_ENABLE:
			ctrl_reg = readl(IR_BASE + IR_CTRL_REG);
			ctrl_reg |= IR_CIR_MODE; //(0x3<<4)
			break;

		case IR_MODULE_ENABLE:
			ctrl_reg = readl(IR_BASE + IR_CTRL_REG);
			ctrl_reg |= IR_ENTIRE_ENABLE; //(0x3<<0)
			break;

		default:
			return;
	}

	writel(ctrl_reg, IR_BASE + IR_CTRL_REG);
}

static void ir_sample_config(enum ir_sample_config set_sample)
{
	u32 sample_reg = 0;

	sample_reg = readl(IR_BASE + IR_SPLCFG_REG);

	switch (set_sample) {
		case IR_SAMPLE_REG_CLEAR:
			sample_reg = 0;
			break;
		case IR_CLK_SAMPLE:
			sample_reg |= IR_SAMPLE_DEV; //(0x3<<0)
			break;
		case IR_FILTER_TH:
			sample_reg |= IR_RXFILT_VAL;
			break;
		case IR_IDLE_TH:
			sample_reg |= IR_RXIDLE_VAL;
			break;
		case IR_ACTIVE_TH:
			sample_reg |= IR_ACTIVE_T;
			sample_reg |= IR_ACTIVE_T_C;
			break;
		default:
			return;
	}

	writel(sample_reg, IR_BASE+IR_SPLCFG_REG);
}

static void ir_signal_invert(void)
{
	u32 reg_value;

	reg_value = 0x1<<2;
	writel(reg_value, IR_BASE+IR_RXCFG_REG);
}

static void ir_irq_config(enum ir_irq_config set_irq)
{
	u32 irq_reg = 0;

	switch (set_irq) {
		case IR_IRQ_STATUS_CLEAR:
			writel(0xef, IR_BASE + IR_RXINTS_REG);
			return;
		case IR_IRQ_ENABLE:
			irq_reg = readl(IR_BASE + IR_RXINTE_REG);
			irq_reg |= IR_IRQ_STATUS;
			break;
		case IR_IRQ_FIFO_SIZE:
			irq_reg = readl(IR_BASE + IR_RXINTE_REG);
			irq_reg |= IR_FIFO_32;
			break;
		default:
			return;
	}

	writel(irq_reg, IR_BASE + IR_RXINTE_REG);
}

static void ir_reg_cfg(void)
{
	/* Enable IR Mode */
	ir_mode_set(CIR_MODE_ENABLE);
	/* Config IR Smaple Register */
	ir_sample_config(IR_SAMPLE_REG_CLEAR);
	ir_sample_config(IR_CLK_SAMPLE);
	ir_sample_config(IR_FILTER_TH);		/* Set Filter Threshold */
	ir_sample_config(IR_IDLE_TH); 		/* Set Idle Threshold */
	ir_sample_config(IR_ACTIVE_TH);         /* Set Active Threshold */
	/* Invert Input Signal */
	ir_signal_invert();
	/* Clear All Rx Interrupt Status */
	ir_irq_config(IR_IRQ_STATUS_CLEAR);
	/* Set Rx Interrupt Enable */
	ir_irq_config(IR_IRQ_ENABLE);
	ir_irq_config(IR_IRQ_FIFO_SIZE);	/* Rx FIFO Threshold = FIFOsz/2; */
	/* Enable IR Module */
	ir_mode_set(IR_MODULE_ENABLE);
}

static int ir_raw_event_store(struct ir_raw_buffer *ir_raw, struct ir_raw_event *ev)
{
	if (ir_raw->dcnt < IR_RAW_BUF_SIZE)
		memcpy(&(ir_raw->raw[ir_raw->dcnt++]), ev, sizeof(*ev));
	else
		printf("raw event store full\n");

	return 0;
}

extern int ir_boot_recovery_mode_detect(void);
extern int ir_nec_decode(struct ir_raw_buffer *ir_raw, struct ir_raw_event ev);
unsigned long ir_packet_handle(struct ir_raw_buffer *ir_raw)
{
	int i = 0, ret = 0,mode=1;
	for (i = 0; i < ir_raw->dcnt; i++)
	{
		ir_nec_decode(ir_raw, ir_raw->raw[i]);
	}

	ret = ir_boot_recovery_mode_detect();
	if (!ret)
	{
		del_timer(&ir_timer_t);
		ir_disable();
		gd->ir_detect_status = IR_DETECT_OK;
		ret=script_parser_fetch("ir_boot_recovery", "mode", (int *)&mode, sizeof(int) / 4);
		if(ret)
		{
			printf("ir boot recovery use default mode 1\n");
			mode=1;
		}
		if (mode == ONEKEY_USB_RECOVERY_MODE)
		{
			gd->key_pressd_value = IR_BOOT_RECOVERY_VALUE;
			printf("ir boot recovery use mode 1\n");
		}
		else if (mode == ONEKEY_SPRITE_RECOVERY_MODE)
		{
			gd->key_pressd_value = SPRITE_RECOVERY_KEY_VALUE;
			printf("ir boot recovery use mode 2\n");
		}
		printf("ir boot recovery\n");
	}
	return 0;

}

void ir_recv_irq_service(void *data)
{
	u32 intsta, dcnt;
	u32 i = 0;
	bool pluse_now = 0;
	u8 reg_data;

	print_debug("IR RX IRQ Serve\n");

	intsta = ir_get_intsta();
	ir_clr_intsta(intsta);

	dcnt =  (ir_get_intsta() >> 8) & 0x7f;
	print_debug("receive cnt :%d \n", dcnt);

	/* Read FIFO and fill the raw event */
	{
		/* get the data from fifo */
		for (i = 0; i < dcnt; i++) {
			reg_data = ir_get_data();
			pluse_now = (reg_data & 0x80)? true : false;

			if ( pluse_pre == pluse_now){/* the signal maintian */
				/* the pluse or space lasting*/
				rawir.duration += (u32)(reg_data & 0x7f);
				print_debug("raw: %d:%d \n",(reg_data & 0x80)>>7,(reg_data & 0x7f));
			} else {
				if (is_receiving) {
					rawir.duration *= IR_SIMPLE_UNIT;
					print_debug("pusle :%d, dur: %u ns\n",rawir.pulse,rawir.duration );
					ir_raw_event_store(&sunxi_ir_raw, &rawir);
					rawir.pulse = pluse_now;
					rawir.duration = (u32)(reg_data & 0x7f);
					print_debug("raw: %d:%d \n",(reg_data & 0x80)>>7,(reg_data & 0x7f));
				} else {
					/* get the first pluse signal */
					rawir.pulse = pluse_now;
					rawir.duration = (u32)(reg_data & 0x7f);
#ifdef CIR_24M_CLK_USED
					rawir.duration += ((IR_ACTIVE_T>>16)+1) * ((IR_ACTIVE_T_C>>23 )? 128:1);
					print_debug("get frist pulse,add head %d !!\n",((IR_ACTIVE_T>>16)+1) * ((IR_ACTIVE_T_C>>23 )? 128:1));
#endif
					is_receiving = 1;
					print_debug("raw: %d:%d \n",(reg_data & 0x80)>>7,(reg_data & 0x7f));
				}
				pluse_pre = pluse_now;
			}
		}
	}

	if (intsta & IR_RXINTS_RXPE) {	 /* Packet End */
		if (rawir.duration) {
			rawir.duration *= IR_SIMPLE_UNIT;
			print_debug("pusle :%d, dur: %u ns\n",rawir.pulse,rawir.duration );
			ir_raw_event_store(&sunxi_ir_raw, &rawir);
		}

		print_debug("handle raw data.\n");
		/* handle ther decoder theread */
		ir_packet_handle(&sunxi_ir_raw);
		ir_reset_rawbuffer();
		is_receiving = 0;
		pluse_pre = false;
	}

	if (intsta & IR_RXINTS_RXOF) { /* FIFO Overflow */
		/* flush rew buffer */
		is_receiving = 0;
		pluse_pre = false;
		ir_reset_rawbuffer();
		printf("ir_irq_service: Rx FIFO Overflow\n");
	}

	print_debug("ir_irq_service: end\n");
	return ;
}

void rc_keydown(struct ir_raw_buffer *ir_raw, u32 scancode, u8 toggle)
{
	ir_raw->scancode = scancode;
}

int ir_setup(void)
{
	if (fdt_set_all_pin("/soc/s_cir", "pinctrl-0")) {
		printf("[ir_boot_para] ir gpio failed\n");
		return -1;
	}

	ir_clk_cfg();
	ir_reg_cfg();

	ir_reset_rawbuffer();
	irq_install_handler(AW_IRQ_CIR, ir_recv_irq_service, NULL);
	irq_enable(AW_IRQ_CIR);

	return 0;
}

void ir_disable(void)
{
	irq_disable(AW_IRQ_CIR);
	irq_free_handler(AW_IRQ_CIR);
}

