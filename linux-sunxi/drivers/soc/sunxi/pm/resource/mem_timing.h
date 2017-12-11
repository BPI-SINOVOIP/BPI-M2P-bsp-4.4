/*
 * drivers/soc/sunxi/pm/resource/mem_timing.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef __MEM_TIMING_H__
#define __MEM_TIMING_H__

#include <linux/types.h>

enum counter_type_e {
	I_CACHE_MISS      = 0X01,
	I_TLB_MISS        = 0X02,
	D_CACHE_MISS      = 0X03,
	D_CACHE_ACCESS    = 0X04,
	D_TLB_MISS        = 0X05,
	BR_PRED           = 0X12,
	MEM_ACCESS        = 0X13,
	D_CACHE_EVICT     = 0X15,
	L2_CACHE_REFILL   = 0X17,
	BUS_ACCESS        = 0X19,
	INST_SPEC         = 0X1c,
	PREFETCH_LINEFILL = 0Xc2,
};

#define typecheck(type, x) \
({	type __dummy; \
	typeof(x) __dummy2; \
		(void)(&__dummy == &__dummy2); \
			1; \
})

#define counter_after(a, b) \
(typecheck(__u32, a) && \
typecheck(__u32, b) && \
((__s32)(b) - (__s32)(a) < 0))
#define counter_before(a, b) counter_after(b, a)

#define counter_after_eq(a, b) \
(typecheck(__u32, a) && \
typecheck(__u32, b) && \
((__s32)(a) - (__s32)(b) >= 0))
#define counter_before_eq(a, b) counter_after_eq(b, a)

__u32 get_cyclecount(void);
void backup_perfcounter(void);
void init_perfcounters(__u32 do_reset, __u32 enable_divider);
void restore_perfcounter(void);
void reset_counter(void);
void change_runtime_env(void);
void delay_us(__u32 us);
void delay_ms(__u32 ms);

void init_event_counter(__u32 do_reset, __u32 enable_divider);
void set_event_counter(enum counter_type_e type);
int get_event_counter(enum counter_type_e type);

extern __u32 raw_lib_udiv(__u32 dividend, __u32 divisior);

#endif /* __MEM_TIMING_H__ */
