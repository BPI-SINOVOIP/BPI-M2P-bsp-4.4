/*
 * drivers/soc/sunxi/pm/resource/mem_tmr.c
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <linux/types.h>
#include "mem_tmr.h"
#include "../pm.h"

static struct __mem_tmr_reg_t *TmrReg;
static __u32 WatchDog1_Mod_Reg_Bak;
#ifndef CONFIG_ARCH_SUN8IW11P1
static __u32 WatchDog1_Config_Reg_Bak, WatchDog1_Irq_En_Bak;
#endif

__s32 mem_tmr_save(struct __mem_tmr_reg_t *ptmr_state)
{
	/* backup timer registers */
	WatchDog1_Mod_Reg_Bak = (TmrReg->WDog1_Mode_Reg);

#ifndef CONFIG_ARCH_SUN8IW11P1
	WatchDog1_Config_Reg_Bak = (TmrReg->WDog1_Cfg_Reg);
	WatchDog1_Irq_En_Bak = (TmrReg->WDog1_Irq_En);
#endif

	ptmr_state->IntCtl = TmrReg->IntCtl;
	ptmr_state->Tmr0Ctl = TmrReg->Tmr0Ctl;
	ptmr_state->Tmr0IntVal = TmrReg->Tmr0IntVal;
	ptmr_state->Tmr0CntVal = TmrReg->Tmr0CntVal;
	ptmr_state->Tmr1Ctl = TmrReg->Tmr1Ctl;
	ptmr_state->Tmr1IntVal = TmrReg->Tmr1IntVal;
	ptmr_state->Tmr1CntVal = TmrReg->Tmr1CntVal;

	return 0;
}

__s32 mem_tmr_restore(struct __mem_tmr_reg_t *ptmr_state)
{
	/* restore timer0 parameters */
	TmrReg->Tmr0IntVal = ptmr_state->Tmr0IntVal;
	TmrReg->Tmr0CntVal = ptmr_state->Tmr0CntVal;
	TmrReg->Tmr0Ctl = ptmr_state->Tmr0Ctl;
	TmrReg->Tmr1IntVal = ptmr_state->Tmr1IntVal;
	TmrReg->Tmr1CntVal = ptmr_state->Tmr1CntVal;
	TmrReg->Tmr1Ctl = ptmr_state->Tmr1Ctl;
	TmrReg->IntCtl = ptmr_state->IntCtl;

#ifndef CONFIG_ARCH_SUN8IW11P1
	(TmrReg->WDog1_Cfg_Reg) = WatchDog1_Config_Reg_Bak;
	(TmrReg->WDog1_Irq_En) = WatchDog1_Irq_En_Bak;
#endif

	(TmrReg->WDog1_Mode_Reg) = WatchDog1_Mod_Reg_Bak;

	return 0;
}

__s32 mem_tmr_init(void)
{
	u32 *base = 0;
	u32 len = 0;

	pm_get_dev_info("soc_timer", 0, &base, &len);

	/* set timer register base */
	TmrReg = (struct __mem_tmr_reg_t *)(base);

	return 0;
}

void mem_tmr_enable_watchdog(void)
{
#ifdef CONFIG_ARCH_SUN8IW11P1
	/* set watch-dog reset to whole system */
	(TmrReg->WDog1_Mode_Reg) &= ~(0x3);
	(TmrReg->WDog1_Mode_Reg) |= 0x2;
	/*  timeout is 16 seconds */
	(TmrReg->WDog1_Mode_Reg) &= ~(0xF << 3);
	(TmrReg->WDog1_Mode_Reg) |= (0xb << 3);
	/* enable watch-dog */
	(TmrReg->WDog1_Mode_Reg) |= 0x1;
#else
	/* set watch-dog reset to whole system */
	(TmrReg->WDog1_Cfg_Reg) &= ~(0x3);
	(TmrReg->WDog1_Cfg_Reg) |= 0x1;

	/*  timeout is 16 seconds */
	(TmrReg->WDog1_Mode_Reg) = (0xb << 4);

	/* enable watch-dog interrupt */
	(TmrReg->WDog1_Irq_En) |= (1 << 0);

	/* enable watch-dog */
	(TmrReg->WDog1_Mode_Reg) |= (1 << 0);
#endif
}

void mem_tmr_disable_watchdog(void)
{
#ifdef CONFIG_ARCH_SUN8IW11P1
	(TmrReg->WDog1_Mode_Reg) &= ~(0x3);
#else
	/* disable watch-dog reset: only intterupt */
	(TmrReg->WDog1_Cfg_Reg) &= ~(0x3);
	(TmrReg->WDog1_Cfg_Reg) |= 0x2;

	/* disable watch-dog intterupt */
	(TmrReg->WDog1_Irq_En) &= ~(1 << 0);

	/* disable watch-dog */
	TmrReg->WDog1_Mode_Reg &= ~(1 << 0);
#endif
}

__s32 mem_tmr_set(__u32 timeout_ms)
{
	/* config timer interrupt */
	TmrReg->IntSta = 0x3;
	TmrReg->IntCtl = 0x3;

#if 1
	/* config timer0 for mem */
	TmrReg->Tmr0Ctl = 0;
	TmrReg->Tmr0IntVal = timeout_ms;
	TmrReg->Tmr0Ctl &= ~(0x3 << 2);	/* clk src: 32K */
	TmrReg->Tmr0Ctl = (1 << 7) | (5 << 4);/* single mode | prescale= /32; */
	TmrReg->Tmr0Ctl |= (1 << 1);	/* reload timer 0 interval value; */
	TmrReg->Tmr0Ctl |= (1 << 0);	/* start */
#else
	/* config timer1 for mem */
	TmrReg->Tmr1Ctl = 0;
	TmrReg->Tmr1IntVal = second << 10;
	TmrReg->Tmr1Ctl &= ~(0x3 << 2);	/* clk src: 32K */
	TmrReg->Tmr1Ctl = (1 << 7) | (5 << 4);/* single mode | prescale= /32; */
	TmrReg->Tmr1Ctl |= (1 << 1);	/* reload timer 0 interval value; */
	TmrReg->Tmr1Ctl |= (1 << 0);	/* start */
#endif

	return 0;
}
