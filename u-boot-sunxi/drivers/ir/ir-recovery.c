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
#include <sys_config_old.h>
#include <fdt_support.h>

#include "sunxi-ir.h"
#include "asm/arch/timer.h"

DECLARE_GLOBAL_DATA_PTR;

struct sunxi_ir_data ir_data;
struct timer_list ir_timer_t;

static int ir_boot_recovery_mode = 0;
extern struct ir_raw_buffer sunxi_ir_raw;
static int ir_detect_time = 1500;

static int ir_sys_cfg(void)
{
	int i, ret;
	int used = 0;
	int value = 0;
	char addr_name[32];
	int detect_time = 0;

	if(uboot_spare_head.boot_data.work_mode != WORK_MODE_BOOT)
		return -1;

	ret = script_parser_fetch("ir_boot_recovery", "ir_boot_recovery_used", (int *)&used, sizeof(int) / 4);
	if(ret || !used)
	{
		printf("ir boot recovery not used\n");
		return -1;
	}

	ret = script_parser_fetch("ir_boot_recovery", "ir_detect_time", (int *)&detect_time, sizeof(int) / 4);
	if(ret)
	{
		printf("use default detect time %d\n", ir_detect_time);
		detect_time = ir_detect_time;
	}

	ir_detect_time = detect_time;

	for (i = 0; i < MAX_IR_ADDR_NUM; i++) {
		sprintf(addr_name, "ir_recovery_key_code%d", i);
		if (script_parser_fetch("ir_boot_recovery", addr_name, (int *)&value, 1)) {
			printf("node %s get failed!\n", addr_name);
			break;
		}
		ir_data.ir_recoverykey[i] = value;
	}
	ir_data.ir_addr_cnt = i;

	for (i = 0; i < ir_data.ir_addr_cnt; i++)
	{
		sprintf(addr_name, "ir_addr_code%d", i);
		if (script_parser_fetch("ir_boot_recovery", addr_name, (int *)&value, 1)) {
			break;
		}
		ir_data.ir_addr[i] = value;
	}
	return 0;
}

static int ir_code_valid(struct sunxi_ir_data *ir_data, u32 code)
{
	u32 i, tmp;
	tmp = (code >> 8) & 0xffff;
	for (i = 0; i < ir_data->ir_addr_cnt; i++) {
		if (ir_data->ir_addr[i] == tmp) {
			if ((code & 0xff) == ir_data->ir_recoverykey[i]) {
				printf("ir boot recovery detect\n");
				return 0;
			}

			printf("ir boot recovery not detect, code=0x%x, coded=0x%x\n", code, ir_data->ir_recoverykey[i]);
			return -1;
		}
	}

	printf("ir boot recovery not detect\n");
	return -1;
}

int ir_boot_recovery_mode_detect(void)
{
	u32 code;
	if (ir_boot_recovery_mode)
	{
		code = sunxi_ir_raw.scancode;

		if((code != 0) && (!ir_code_valid(&ir_data, code)))
			return 0;
	}

	return -1;
}

static void ir_detect_overtime(void *p)
{
	struct timer_list *timer_t;

	timer_t = (struct timer_list *)p;
	ir_disable();
	del_timer(timer_t);

	gd->ir_detect_status = IR_DETECT_END;
}

int check_ir_boot_recovery(void)
{
	if(ir_sys_cfg())
		return -1;

	if (ir_setup())
	return -1;

	gd->ir_detect_status = IR_DETECT_NULL;
	ir_boot_recovery_mode = 1;
	ir_timer_t.data = (unsigned long)&ir_timer_t;
	ir_timer_t.expires = ir_detect_time;
	ir_timer_t.function = ir_detect_overtime;
	init_timer(&ir_timer_t);
	add_timer(&ir_timer_t);

	return 0;
}

