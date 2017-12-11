#ifndef _MCTL_HAL_H
#define  _MCTL_HAL_H

#include "../../standby_printk.h"

/*#define FPGA_VERIFY*/
/*#define IC_VERIFY*/
#define SYSTEM_VERIFY

#ifdef SYSTEM_VERIFY
/*boot/standby external head file, add by SW*/
#endif

/*#define CPUS_STANDBY_CODE*/
#ifndef CPUS_STANDBY_CODE
/*#define USE_PMU*/
/*#define USE_CHIPID*/
/*#define USE_SPECIAL_DRAM*/
/*#define USE_CHEAP_DRAM*/
#endif

#ifndef CPUS_STANDBY_CODE
/*#define DRAM_AUTO_SCAN*/
#ifdef DRAM_AUTO_SCAN
#define DRAM_TYPE_SCAN
#define DRAM_RANK_SCAN
#define DRAM_SIZE_SCAN
#endif
#endif

#define DEBUG_LEVEL_0
/*#define DEBUG_LEVEL_1*/
/*#define DEBUG_LEVEL_4*/
/*#define DEBUG_LEVEL_8*/
/*#define TIMING_DEBUG*/
/*#define ERROR_DEBUG*/
/*#define AUTO_DEBUG*/

#if defined DEBUG_LEVEL_8
#define dram_dbg_8(fmt, args...)  standby_printk(fmt, ##args)
#define dram_dbg_4(fmt, args...)  standby_printk(fmt, ##args)
#define dram_dbg_1(fmt, args...)  standby_printk(fmt, ##args)
#define dram_dbg_0(fmt, args...)  standby_printk(fmt, ##args)
#elif defined DEBUG_LEVEL_4
#define dram_dbg_8(fmt, args...)
#define dram_dbg_4(fmt, args...)  standby_printk(fmt, ##args)
#define dram_dbg_1(fmt, args...)  standby_printk(fmt, ##args)
#define dram_dbg_0(fmt, args...)  standby_printk(fmt, ##args)
#elif defined DEBUG_LEVEL_1
#define dram_dbg_8(fmt, args...)
#define dram_dbg_4(fmt, args...)
#define dram_dbg_1(fmt, args...)  standby_printk(fmt, ##args)
#define dram_dbg_0(fmt, args...)  standby_printk(fmt, ##args)
#elif defined DEBUG_LEVEL_0
#define dram_dbg_8(fmt, args...)
#define dram_dbg_4(fmt, args...)
#define dram_dbg_1(fmt, args...)
#define dram_dbg_0(fmt, args...)  standby_printk(fmt, ##args)
#else
#define dram_dbg_8(fmt, args...)
#define dram_dbg_4(fmt, args...)
#define dram_dbg_1(fmt, args...)
#define dram_dbg_0(fmt, args...)
#endif

#if defined TIMING_DEBUG
#define dram_dbg_timing(fmt, args...)  standby_printk(fmt, ##args)
#else
#define dram_dbg_timing(fmt, args...)
#endif

#if defined ERROR_DEBUG
#define dram_dbg_error(fmt, args...)  standby_printk(fmt, ##args)
#else
#define dram_dbg_error(fmt, args...)
#endif

#if defined AUTO_DEBUG
#define dram_dbg_auto(fmt, args...)  standby_printk(fmt, ##args)
#else
#define dram_dbg_auto(fmt, args...)
#endif

#endif /*_MCTL_HAL_H*/
