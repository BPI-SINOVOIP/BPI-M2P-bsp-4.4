/* Sunxi Remote Controller
 *
 * keymap imported from ir-keymaps.c
 *
 * Copyright (c) 2014 by allwinnertech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <media/rc-map.h>
#include "sunxi-ir-rx.h"

static struct rc_map_table sunxi_nec_scan[] = {
	{ KEY_ESC, KEY_ESC },
};

static struct rc_map_list sunxi_map = {
	.map = {
		.scan    = sunxi_nec_scan,
		.size    = ARRAY_SIZE(sunxi_nec_scan),
		.rc_type = RC_TYPE_NEC,	/* Legacy IR type */
		.name    = RC_MAP_SUNXI,
	}
};

int init_sunxi_ir_map(void)
{
	return rc_map_register(&sunxi_map);
}

void exit_sunxi_ir_map(void)
{
	rc_map_unregister(&sunxi_map);
}
