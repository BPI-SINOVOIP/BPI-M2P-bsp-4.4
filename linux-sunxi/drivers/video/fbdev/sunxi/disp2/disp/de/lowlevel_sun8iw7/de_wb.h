/*
 * Allwinner SoCs display driver.
 *
 * Copyright (C) 2016 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

/**
 *	All Winner Tech, All Right Reserved. 2014-2015 Copyright (c)
 *
 *	File name   :       de_wb.h
 *
 *	Description :       display engine 2.0 wbc basic function declaration
 *
 *	History     :       2014/03/03   wangxuan   initial version
 *	                    2014/04/02   wangxuan   change the register
 *	                                            operation from bits to word
 */

#ifndef __DE_WB_H__
#define __DE_WB_H__
#include "../disp_private.h"

s32 WB_EBIOS_Set_Reg_Base(u32 sel, u32 base);
u32 WB_EBIOS_Get_Reg_Base(u32 sel);
s32 WB_EBIOS_Init(struct disp_bsp_init_para *para);
s32 WB_EBIOS_Writeback_Enable(u32 sel, bool en);
s32 WB_EBIOS_Set_Para(u32 sel, struct disp_capture_config *cfg);
s32 WB_EBIOS_Apply(u32 sel, struct disp_capture_config *cfg);
s32 WB_EBIOS_Update_Regs(u32 sel);
u32 WB_EBIOS_Get_Status(u32 sel);
s32 WB_EBIOS_EnableINT(u32 sel);
s32 WB_EBIOS_DisableINT(u32 sel);
u32 WB_EBIOS_QueryINT(u32 sel);
u32 WB_EBIOS_ClearINT(u32 sel);

#endif
