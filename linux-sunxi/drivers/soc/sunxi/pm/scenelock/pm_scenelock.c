/*
 * drivers/soc/sunxi/pm/scenelock/pm_scenelock.c
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Pannan <pannan@allwinnertech.com>
 *
 * scenelock APIs for sunxi suspend
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/power/scenelock.h>
#include "pm_scenelock_config.h"
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif

enum {
	DEBUG_EXIT_SUSPEND = 1U << 0,
	DEBUG_SHOW_NAME    = 1U << 1,
	DEBUG_SUSPEND      = 1U << 2,
	DEBUG_EXPIRE       = 1U << 3,
	DEBUG_SCENE_LOCK   = 1U << 4,
};

int scenelock_debug_mask = 0xf0;

#define SCENE_LOCK_TYPE_MASK              (0x0f)
#define SCENE_LOCK_INITIALIZED            (1U << 8)
#define SCENE_LOCK_ACTIVE                 (1U << 9)

static DEFINE_SPINLOCK(list_lock);
static LIST_HEAD(inactive_locks);
static struct list_head active_scene_locks[SCENE_MAX];
static DEFINE_SPINLOCK(extended_standby_list_lock);
static LIST_HEAD(extended_standby_list);
int extended_standby_cnt = ARRAY_SIZE(extended_standby);

#ifdef CONFIG_HAS_WAKELOCK
static struct wake_lock mp3_extended_standby_wake_lock;
#endif

/* Caller must acquire the list_lock spinlock */
static void print_active_locks(enum SUNXI_POWER_SCENE type)
{
	struct scene_lock *lock;

	if (type >= SCENE_MAX) {
		pr_err("type is not valid =%d\n", type);
		return;
	}

	list_for_each_entry(lock, &active_scene_locks[type], link) {
		pr_info("active scene lock %s\n", lock->name);
	}
}

static int has_scene_lock_locked(enum SUNXI_POWER_SCENE type)
{
	struct scene_lock *lock, *n;

	if (type >= SCENE_MAX) {
		pr_err("type is not valid =%d\n", type);
		return -1;
	}

	list_for_each_entry_safe(lock, n, &active_scene_locks[type], link) {
		if (lock->flags & SCENE_LOCK_ACTIVE)
			return 0;
	}

	return -1;
}

static int has_scene_lock(enum SUNXI_POWER_SCENE type)
{
	int ret;
	unsigned long irqflags;

	spin_lock_irqsave(&list_lock, irqflags);

	ret = has_scene_lock_locked(type);
	if ((!ret) && (scenelock_debug_mask & DEBUG_SHOW_NAME))
		print_active_locks(type);

	spin_unlock_irqrestore(&list_lock, irqflags);

	return ret;
}

void scene_lock_init(struct scene_lock *lock, enum SUNXI_POWER_SCENE type,
				const char *name)
{
	unsigned long irqflags = 0;

	if (name)
		lock->name = name;

	if (!lock->name) {
		pr_err("lock name is NULL\n");
		return;
	}

	if (scenelock_debug_mask & DEBUG_SCENE_LOCK)
		pr_info("%s name=%s\n", __func__, lock->name);

	lock->count = 0;
	lock->flags = (type & SCENE_LOCK_TYPE_MASK) | SCENE_LOCK_INITIALIZED;

	INIT_LIST_HEAD(&lock->link);

	spin_lock_irqsave(&list_lock, irqflags);
	list_add(&lock->link, &inactive_locks);
	spin_unlock_irqrestore(&list_lock, irqflags);
}
EXPORT_SYMBOL(scene_lock_init);

void scene_lock_destroy(struct scene_lock *lock)
{
	unsigned long irqflags;

	if (scenelock_debug_mask & DEBUG_SCENE_LOCK) {
		if (lock->name != NULL)
			pr_info("%s name=%s\n", __func__, lock->name);
	}

	spin_lock_irqsave(&list_lock, irqflags);
	lock->flags &= ~SCENE_LOCK_INITIALIZED;
	list_del(&lock->link);
	spin_unlock_irqrestore(&list_lock, irqflags);
}
EXPORT_SYMBOL(scene_lock_destroy);

char *show_scene_state(char *s, char *end)
{
	int i = 0;

	for (i = 0; i < extended_standby_cnt; i++) {
		if (extended_standby[i].scene_type) {
			if (!check_scene_locked(extended_standby[i].scene_type))
				s += scnprintf(s, end - s, "[%s] ",
						extended_standby[i].name);
			else
				s += scnprintf(s, end - s, "%s ",
						extended_standby[i].name);
		}
	}

	s += scnprintf(s, end - s, "\n");

	return s;
}

void scene_lock(struct scene_lock *lock)
{
	enum SUNXI_POWER_SCENE type;
	unsigned long irqflags;
	bool err = false;
	struct scene_extended_standby_t *local_standby;

	spin_lock_irqsave(&list_lock, irqflags);

	type = lock->flags & SCENE_LOCK_TYPE_MASK;

	if (type >= SCENE_MAX) {
		pr_err("type is not valid =%d\n", type);
		return;
	}

	if (!(lock->flags & SCENE_LOCK_INITIALIZED)) {
		pr_err("lock is not initialized\n");
		return;
	}


	if (!(lock->flags & SCENE_LOCK_ACTIVE))
		lock->flags |= SCENE_LOCK_ACTIVE;

	lock->count += 1;
	list_del(&lock->link);

	if (scenelock_debug_mask & DEBUG_SCENE_LOCK)
		pr_info("%s: %s, type %d, count %d\n", __func__, lock->name,
				type, lock->count);

	list_add(&lock->link, &active_scene_locks[type]);

	spin_unlock_irqrestore(&list_lock, irqflags);

	spin_lock_irqsave(&extended_standby_list_lock, irqflags);
	list_for_each_entry(local_standby, &extended_standby_list, list) {
		if (type == local_standby->scene_type) {
			err = set_extended_standby_manager(local_standby);
			if (!err)
				pr_err("set extended standby manager err\n");
		}
	}
	spin_unlock_irqrestore(&extended_standby_list_lock, irqflags);

#ifdef CONFIG_HAS_WAKELOCK
	if (type == SCENE_MP3_STANDBY)
		wake_lock(&mp3_extended_standby_wake_lock);
#endif
}
EXPORT_SYMBOL(scene_lock);

void scene_unlock(struct scene_lock *lock)
{
	enum SUNXI_POWER_SCENE type;
	unsigned long irqflags;
	bool err = false;
	struct scene_extended_standby_t *local_standby;

	spin_lock_irqsave(&list_lock, irqflags);

	type = lock->flags & SCENE_LOCK_TYPE_MASK;

	if (scenelock_debug_mask & DEBUG_SCENE_LOCK)
		pr_info("%s: %s, count: %d\n", __func__,
				lock->name, lock->count);

	if (lock->count > 1) {
		lock->count -= 1;
		spin_unlock_irqrestore(&list_lock, irqflags);
	} else if (lock->count == 1) {
		lock->count -= 1;
		lock->flags &= ~(SCENE_LOCK_ACTIVE);
		list_del(&lock->link);
		list_add(&lock->link, &inactive_locks);

		spin_unlock_irqrestore(&list_lock, irqflags);

		err = set_extended_standby_manager(NULL);
		if (!err)
			pr_err("clear extended standby manager err\n");

		spin_lock_irqsave(&extended_standby_list_lock, irqflags);
		list_for_each_entry(local_standby,
				&extended_standby_list, list) {
			if (!check_scene_locked(local_standby->scene_type)) {
				err = set_extended_standby_manager(local_standby);
				if (!err)
					pr_err("set extended standby manager err\n");
			}
		}
		spin_unlock_irqrestore(&extended_standby_list_lock, irqflags);
	} else {
		spin_unlock_irqrestore(&list_lock, irqflags);
	}

#ifdef CONFIG_HAS_WAKELOCK
	if (type == SCENE_MP3_STANDBY)
		wake_unlock(&mp3_extended_standby_wake_lock);
#endif
}
EXPORT_SYMBOL(scene_unlock);

int check_scene_locked(enum SUNXI_POWER_SCENE type)
{
	return !!has_scene_lock(type);
}
EXPORT_SYMBOL(check_scene_locked);

int scene_lock_active(struct scene_lock *lock)
{
	return !!(lock->flags & SCENE_LOCK_ACTIVE);
}
EXPORT_SYMBOL(scene_lock_active);

int enable_wakeup_src(enum CPU_WAKEUP_SRC src, int para)
{
	return extended_standby_enable_wakeup_src(src, para);
}
EXPORT_SYMBOL(enable_wakeup_src);

int disable_wakeup_src(enum CPU_WAKEUP_SRC src, int para)
{
	return extended_standby_disable_wakeup_src(src, para);
}
EXPORT_SYMBOL(disable_wakeup_src);

int check_wakeup_state(enum CPU_WAKEUP_SRC src, int para)
{
	return extended_standby_check_wakeup_state(src, para);
}
EXPORT_SYMBOL(check_wakeup_state);

int standby_show_state(void)
{
	struct scene_extended_standby_t *local_standby;
	unsigned long irqflags;
	int lock_cnt = 0;

	pr_info("scence_lock: ");

	spin_lock_irqsave(&extended_standby_list_lock, irqflags);
	list_for_each_entry(local_standby, &extended_standby_list, list) {
		if (!check_scene_locked(local_standby->scene_type)) {
			pr_info("%s\n", local_standby->name);
			lock_cnt++;
		}
	}
	spin_unlock_irqrestore(&extended_standby_list_lock, irqflags);

	if (lock_cnt)
		pr_info("\n");
	else
		pr_info("none\n");

	return 0;
}
EXPORT_SYMBOL(standby_show_state);

int scene_set_volt(enum SUNXI_POWER_SCENE scene_type, unsigned int bitmap,
				unsigned int volt_value)
{
	int i;

	for (i = 0; i < extended_standby_cnt; i++) {
		if (extended_standby[i].scene_type == scene_type) {
			extended_standby[i].soc_pwr_dep.soc_pwr_dm_state.volt[bitmap] = volt_value;
			return 0;
		}
	}

	return -1;
}
EXPORT_SYMBOL(scene_set_volt);

static int __init scenelocks_init(void)
{
	int i;
	unsigned long irqflags;

	for (i = 0; i < ARRAY_SIZE(active_scene_locks); i++)
		INIT_LIST_HEAD(&active_scene_locks[i]);

	for (i = 0; i < extended_standby_cnt; i++) {
		spin_lock_irqsave(&extended_standby_list_lock, irqflags);
		list_add(&extended_standby[i].list, &extended_standby_list);
		spin_unlock_irqrestore(&extended_standby_list_lock, irqflags);
	}

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&mp3_extended_standby_wake_lock, WAKE_LOCK_SUSPEND,
				"mp3_extended_standby");
#endif

	return 0;
}
core_initcall(scenelocks_init);

static void  __exit scenelocks_exit(void)
{
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_destroy(&mp3_extended_standby_wake_lock);
#endif
}
module_exit(scenelocks_exit);


static DEFINE_MUTEX(tree_lock);
static struct rb_root user_scene_locks;

struct user_scenelock *lookup_scene_lock_name(const char *buf,
				int allocate)
{
	struct rb_node **p = &user_scene_locks.rb_node;
	struct rb_node *parent = NULL;
	struct user_scenelock *l;
	int diff, name_len, i;
	const char *arg;
	enum SUNXI_POWER_SCENE type = SCENE_MAX;

	/* Find length of lock name */
	arg = buf;
	while (*arg && !isspace(*arg))
		arg++;

	name_len = arg - buf;
	if (!name_len)
		goto bad_arg;

	while (isspace(*arg))
		arg++;

	/* Lookup scene lock in rbtree */
	while (*p) {
		parent = *p;
		l = rb_entry(parent, struct user_scenelock, node);
		diff = strncmp(buf, l->name, name_len);
		if (!diff && l->name[name_len])
			diff = -1;

		pr_debug("%s: compare %.*s %s %d\n",
				__func__, name_len, buf, l->name, diff);

		if (diff < 0)
			p = &(*p)->rb_left;
		else if (diff > 0)
			p = &(*p)->rb_right;
		else
			return l;
	}

	/* Allocate and add new scenelock to rbtree */
	if (!allocate) {
		pr_err("%s: %.*s not found\n", __func__, name_len, buf);
		return ERR_PTR(-EINVAL);
	}

	l = kzalloc(sizeof(*l) + name_len + 1, GFP_KERNEL);
	if (!l)
		return ERR_PTR(-ENOMEM);

	memcpy(l->name, buf, name_len);

	for (i = 0; i < extended_standby_cnt; i++) {
		if (extended_standby[i].scene_type) {
			if (name_len == strlen(extended_standby[i].name)
				&& !strncmp(l->name, extended_standby[i].name,
					strlen(extended_standby[i].name))) {
				type = extended_standby[i].scene_type;
				break;
			}
		}
	}

	if (type == SCENE_MAX) {
		pr_err("scene name: %s is not supported.\n", l->name);
		return ERR_PTR(-EINVAL);
	}

	pr_info("%s: new scene lock %s\n", __func__, l->name);

	scene_lock_init(&l->scene_lock, type, l->name);
	rb_link_node(&l->node, parent, p);
	rb_insert_color(&l->node, &user_scene_locks);

	return l;

bad_arg:
	pr_err("lookup_wake_lock_name: wake lock, %.*s, bad arg, %s\n",
				name_len, buf, arg);

	return ERR_PTR(-EINVAL);
}

int scene_lock_state(const char *buf, size_t count)
{
	struct user_scenelock *l;

	mutex_lock(&tree_lock);

	l = lookup_scene_lock_name(buf, 1);
	if (IS_ERR(l)) {
		count = PTR_ERR(l);
		goto bad_name;
	}

	scene_lock(&l->scene_lock);

	if (strlen(l->name) == strlen("usb_standby")
		&& !strncmp(l->name, "usb_standby",
			strlen("usb_standby"))) {
		enable_wakeup_src(CPUS_USBMOUSE_SRC, 0);
	} else if (strlen(l->name) == strlen("talking_standby")
		&& !strncmp(l->name, "talking_standby",
			strlen("talking_standby"))) {
		;
	} else if (strlen(l->name) == strlen("mp3_standby")
		&& !strncmp(l->name, "mp3_standby",
			strlen("mp3_standby"))) {
		;
	}

bad_name:
	mutex_unlock(&tree_lock);

	return count;
}

int scene_unlock_state(const char *buf, size_t count)
{
	struct user_scenelock *l;

	mutex_lock(&tree_lock);

	l = lookup_scene_lock_name(buf, 0);
	if (IS_ERR(l)) {
		count = PTR_ERR(l);
		goto not_found;
	}

	pr_info("%s: %s\n", __func__, l->name);

	scene_unlock(&l->scene_lock);

	if (l->scene_lock.count == 0) {
		if (strlen(l->name) == strlen("usb_standby")
			&& !strncmp(l->name, "usb_standby",
				strlen("usb_standby"))) {
			if (check_scene_locked(SCENE_USB_STANDBY))
				disable_wakeup_src(CPUS_USBMOUSE_SRC, 0);
		} else if (strlen(l->name) == strlen("talking_standby")
			&& !strncmp(l->name, "talking_standby",
				strlen("talking_standby"))) {
			;
		} else if (strlen(l->name) == strlen("mp3_standby")
			&& !strncmp(l->name, "mp3_standby",
				strlen("mp3_standby"))) {
			;
		}
	}

not_found:
	mutex_unlock(&tree_lock);

	return count;
}
