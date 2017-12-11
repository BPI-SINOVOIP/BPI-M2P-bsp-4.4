/*
 * Allwinner SoCs display driver.
 *
 * Copyright (C) 2016 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include "panels.h"

struct sunxi_lcd_drv g_lcd_drv;

struct __lcd_panel *panel_array[] = {
	&default_eink,
	&default_panel,
#ifdef CONFIG_LCD_SUPPORT_LT070ME05000
	&lt070me05000_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_WTQ05027D01
	&wtq05027d01_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_T27P06
	&t27p06_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_DX0960BE40A1
	&dx0960be40a1_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_TFT720x1280
	&tft720x1280_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_S6D7AA0X01
	&S6D7AA0X01_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_CPU_GG1P4062UTSW
	&gg1p4062utsw_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_LS029B3SX02
	&ls029b3sx02_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_HE0801A068
	&he0801a068_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_ILI9341
	&ili9341_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_FD055HD003S
	&fd055hd003s_panel,
#endif
	/* add new panel below */
	NULL,
};

static void lcd_set_panel_funs(void)
{
	int i;

	for (i = 0; panel_array[i] != NULL; i++) {
		sunxi_lcd_set_panel_funs(panel_array[i]->name,
					 &panel_array[i]->func);
	}
}

int lcd_init(void)
{
	sunxi_disp_get_source_ops(&g_lcd_drv.src_ops);
	lcd_set_panel_funs();

	return 0;
}

