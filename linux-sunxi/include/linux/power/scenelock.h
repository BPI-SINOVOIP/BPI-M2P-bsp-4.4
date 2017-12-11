/*
 * include/linux/power/scenelock.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Pannan <pannan@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef _LINUX_SCENELOCK_H
#define _LINUX_SCENELOCK_H

#include <linux/list.h>
#include <linux/ktime.h>
#include <linux/rbtree.h>
#include <linux/power/aw_pm.h>

enum SUNXI_POWER_SCENE {
	SCENE_TALKING_STANDBY = 1,
	SCENE_USB_STANDBY,
	SCENE_MP3_STANDBY,
	SCENE_BOOT_FAST,
	SCENE_SUPER_STANDBY,
	SCENE_GPIO_STANDBY,
	SCENE_GPIO_HOLD_STANDBY,
	SCENE_NORMAL_STANDBY,
	SCENE_MISC_STANDBY,
	SCENE_MISC1_STANDBY,
	SCENE_DYNAMIC_STANDBY,
	SCENE_USB_OHCI_STANDBY,
	SCENE_USB_EHCI_STANDBY,
	SCENE_WLAN_STANDBY,
	SCENE_MAX
};

enum POWER_SCENE_FLAGS {
	TALKING_STANDBY_FLAG   = (1<<0x0),
	USB_STANDBY_FLAG       = (1<<0x1),
	MP3_STANDBY_FLAG       = (1<<0x2),
	SUPER_STANDBY_FLAG     = (1<<0x3),
	NORMAL_STANDBY_FLAG    = (1<<0x4),
	GPIO_STANDBY_FLAG      = (1<<0x5),
	MISC_STANDBY_FLAG      = (1<<0x6),
	BOOT_FAST_STANDBY_FLAG = (1<<0x7),
	MISC1_STANDBY_FLAG     = (1<<0x8),
	GPIO_HOLD_STANDBY_FLAG = (1<<0x9),
	DYNAMIC_STANDBY_FLAG   = (1<<0xa),
	USB_OHCI_STANDBY_FLAG  = (1<<0xb),
	USB_EHCI_STANDBY_FLAG  = (1<<0xc),
	WLAN_STANDBY_FLAG      = (1<<0xd)
};

struct scene_lock {
	struct list_head  link;
	int               flags;
	int               count;
	const char       *name;
};

enum CPU_WAKEUP_SRC {
	/* the wakeup source of main cpu: cpu0, only used in normal standby */
	CPU0_MSGBOX_SRC    = CPU0_WAKEUP_MSGBOX,  /* external interrupt, pmu event for ex. */
	CPU0_KEY_SRC       = CPU0_WAKEUP_KEY,     /* key event, volume home menu enter */
	CPU0_EXINT_SRC     = CPU0_WAKEUP_EXINT,
	CPU0_IR_SRC        = CPU0_WAKEUP_IR,
	CPU0_ALARM_SRC     = CPU0_WAKEUP_ALARM,
	CPU0_USB_SRC       = CPU0_WAKEUP_USB,
	CPU0_TIMEOUT_SRC   = CPU0_WAKEUP_TIMEOUT,

	/* the wakeup source of assistant cpu: cpus */
	CPUS_LOWBATT_SRC   = CPUS_WAKEUP_LOWBATT,  /* low battery event */
	CPUS_USB_SRC       = CPUS_WAKEUP_USB,      /* usb insert event */
	CPUS_AC_SRC        = CPUS_WAKEUP_AC,       /* charger insert event */
	CPUS_ASCEND_SRC    = CPUS_WAKEUP_ASCEND,   /* power key ascend event */
	CPUS_DESCEND_SRC   = CPUS_WAKEUP_DESCEND,  /* power key descend event */
	CPUS_SHORT_KEY_SRC = CPUS_WAKEUP_SHORT_KEY,/* power key short press event */
	CPUS_LONG_KEY_SRC  = CPUS_WAKEUP_LONG_KEY, /* power key long press event */
	CPUS_IR_SRC        = CPUS_WAKEUP_IR,       /* IR key event */
	CPUS_ALM0_SRC      = CPUS_WAKEUP_ALM0,     /* alarm0 event */
	CPUS_ALM1_SRC      = CPUS_WAKEUP_ALM1,     /* alarm1 event */
	CPUS_TIMEOUT_SRC   = CPUS_WAKEUP_TIMEOUT,  /* debug test event */
	CPUS_GPIO_SRC      = CPUS_WAKEUP_GPIO,     /* GPIO interrupt event, only used in extended standby */
	CPUS_USBMOUSE_SRC  = CPUS_WAKEUP_USBMOUSE, /* USBMOUSE key event, only used in extended standby */
	CPUS_LRADC_SRC     = CPUS_WAKEUP_LRADC,    /* key event, volume home menu enter, only used in extended standby */
	CPUS_WLAN_SRC      = CPUS_WAKEUP_WLAN,     /* WLAN interrupt */
	CPUS_CODEC_SRC     = CPUS_WAKEUP_CODEC,    /* codec irq, only used in extended standby*/
	CPUS_BAT_TEMP_SRC  = CPUS_WAKEUP_BAT_TEMP, /* baterry temperature low and high */
	CPUS_FULLBATT_SRC  = CPUS_WAKEUP_FULLBATT, /* baterry full */
	CPUS_HMIC_SRC      = CPUS_WAKEUP_HMIC,     /* earphone insert/pull event, only used in extended standby */
	CPUS_KEY_SL_SRC    = CPUS_WAKEUP_KEY       /* power key short and long press event */
};

struct extended_standby_manager_t {
	extended_standby_t *pextended_standby;
	unsigned long event;
	unsigned long wakeup_gpio_map;
	unsigned long wakeup_gpio_group;
};

#ifndef CONFIG_ARCH_SUN8IW7P1
struct scene_extended_standby_t {
	/*
	 * scene type of extended standby
	 */
	enum SUNXI_POWER_SCENE scene_type; /* for scene_lock implement convenient; */
	char *name; /* for user convenient; */
	soc_pwr_dep_t soc_pwr_dep;
	struct list_head list; /* list of all extended standby */
};

int extended_standby_set_pmu_id(unsigned int num, unsigned int pmu_id);
#else
struct scene_extended_standby_t {
	extended_standby_t extended_standby_data;

	/*
	 * scene type of extended standby
	 */
	enum SUNXI_POWER_SCENE scene_type;
	char *name;

	struct list_head list; /* list of all extended standby */
};
#endif
extern struct scene_extended_standby_t extended_standby[];
extern int extended_standby_cnt;

const struct  extended_standby_manager_t *get_extended_standby_manager(void);
bool set_extended_standby_manager(struct scene_extended_standby_t *local_standby);
int extended_standby_enable_wakeup_src(enum CPU_WAKEUP_SRC src, int para);
int extended_standby_disable_wakeup_src(enum CPU_WAKEUP_SRC src, int para);
int extended_standby_check_wakeup_state(enum CPU_WAKEUP_SRC src, int para);
int extended_standby_show_state(void);

struct user_scenelock {
	struct rb_node    node;
	struct scene_lock scene_lock;
	char              name[0];
};

extern struct user_scenelock *lookup_scene_lock_name(const char *buf,
				int allocate);
extern char *show_scene_state(char *s, char *end);
extern int scene_lock_state(const char *buf, size_t count);
extern int scene_unlock_state(const char *buf, size_t count);

void scene_lock_init(struct scene_lock *lock, enum SUNXI_POWER_SCENE type,
				const char *name);
extern void scene_lock_destroy(struct scene_lock *lock);
extern void scene_lock(struct scene_lock *lock);
extern void scene_unlock(struct scene_lock *lock);
extern int check_scene_locked(enum SUNXI_POWER_SCENE type);
extern int scene_lock_active(struct scene_lock *lock);

extern int enable_wakeup_src(enum CPU_WAKEUP_SRC src, int para);
extern int disable_wakeup_src(enum CPU_WAKEUP_SRC src, int para);
extern int check_wakeup_state(enum CPU_WAKEUP_SRC src, int para);
extern int standby_show_state(void);
extern int scene_set_volt(enum SUNXI_POWER_SCENE scene_type,
				unsigned int bitmap, unsigned int volt_value);

#endif
