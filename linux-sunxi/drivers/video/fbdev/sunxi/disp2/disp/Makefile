include $(obj)/de/Makefile

obj-$(CONFIG_DISP2_SUNXI) += disp.o

disp-objs :=  dev_disp.o dev_fb.o dev_disp_debugfs.o
disp-objs += disp_sys_intf.o
disp-$(CONFIG_DISP2_SUNXI_COMPOSER)  += dev_composer.o
disp-$(CONFIG_DISP2_SUNXI_COMPOSER2) += dev_composer2.o

disp-objs += de/disp_display.o de/disp_features.o de/disp_device.o \
	     de/disp_lcd.o de/disp_manager.o de/disp_private.o \
	     de/disp_capture.o    de/disp_vdevice.o \
	     de/disp_hdmi.o   de/disp_format_convert.o \
	     de/disp_rotation_sw.o

disp-$(CONFIG_DE_SUPPORT_TV) += de/disp_tv.o
disp-$(CONFIG_DE_SUPPORT_VGA) += de/disp_vga.o
disp-$(CONFIG_DE_SUPPORT_EINK) += de/disp_eink_manager.o de/eink_buffer_manager.o de/eink_pipeline_manager.o
disp-$(CONFIG_DE_SUPPORT_SMBL)	+= de/disp_smart_backlight.o
disp-$(CONFIG_DE_SUPPORT_ENHANCE) += de/disp_enhance.o

disp-objs += lcd/panels.o lcd/lcd_source.o lcd/default_panel.o \
		lcd/default_eink.o

disp-$(CONFIG_LCD_SUPPORT_CPU_GG1P4062UTSW) += lcd/cpu_gg1p4062utsw.o
disp-$(CONFIG_LCD_SUPPORT_DX0960BE40A1) += lcd/dx0960be40a1.o
disp-$(CONFIG_LCD_SUPPORT_FD055HD003S) += lcd/fd055hd003s.o
disp-$(CONFIG_LCD_SUPPORT_HE0801A068) += lcd/he0801a068.o
disp-$(CONFIG_LCD_SUPPORT_ILI9341) += lcd/ili9341.o
disp-$(CONFIG_LCD_SUPPORT_LS029B3SX02) += lcd/ls029b3sx02.o
disp-$(CONFIG_LCD_SUPPORT_LT070ME05000) += lcd/lt070me05000.o
disp-$(CONFIG_LCD_SUPPORT_S6D7AA0X01) += lcd/S6D7AA0X01.o
disp-$(CONFIG_LCD_SUPPORT_T27P06) += lcd/t27p06.o
disp-$(CONFIG_LCD_SUPPORT_TFT720x1280) += lcd/tft720x1280.o
disp-$(CONFIG_LCD_SUPPORT_WTQ05027D01) += lcd/wtq05027d01.o

disp-objs += $(obj_low)
