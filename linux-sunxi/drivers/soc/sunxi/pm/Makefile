#use "-Os" flags.
use-flags := -Os
#Don't use "-O2" flags.
not-use-flags := -O2
KBUILD_CFLAGS := $(filter-out $(not-use-flags), $(KBUILD_CFLAGS))
KBUILD_CFLAGS += $(use-flags)
GCOV_PROFILE  := n

obj-y += pm_tmp.o

pm_tmp-y := scenelock/pm_scenelock.o  \
	    debug/pm_debug.o          \
	    debug/pm_errcode.o        \
	    sysfs/pm_sysfs.o          \
	    sysfs/pm_sysfs_parse.o    \
	    resource/mem_int.o        \
	    resource/mem_tmr.o        \
	    resource/mem_usb.o        \
	    resource/mem_ir.o         \
	    resource/mem_key.o        \
	    pm_config.o               \
	    pm_extended_standby.o     \
	    pm.o

ifeq ($(CONFIG_SUNXI_SUSPEND_NONARISC),y)
pm_tmp-y += resume.o                  \
	    resource/mem_gpio.o       \
	    resource/mem_mapping.o    \
	    resource/mem_sram.o       \
	    resource/mem_clk.o        \
	    resource/mem_timing.o     \
	    resource/mem_twi.o        \
	    standby.o

ifeq ($(CONFIG_ARCH_SUN8IW11P1),y)
pm_tmp-y += platform/sun8iw11p1/pm_ccu_sun8iw11p1.o
endif

$(obj)/resource/mem_clk.o       : $(obj)/pm_tmp_x.o
$(obj)/resource/mem_gpio.o      : $(obj)/pm_tmp_x.o
$(obj)/resource/mem_timing.o    : $(obj)/pm_tmp_x.o
$(obj)/resource/mem_int.o       : $(obj)/pm_tmp_x.o

$(obj)/pm_tmp_x.o               : $(obj)/standby/standby.code FORCE
$(obj)/standby.o                : $(obj)/standby/standby.code FORCE

$(obj)/standby/standby.code: FORCE
	$(Q)$(MAKE) $(build)=$(obj)/standby

else
ifneq ($(CONFIG_ARM_PSCI_FW),y)
pm_tmp-y += resume1/resume1.o
pm_tmp-y += resource/mem_serial.o	\
	    resource/mem_clk.o

$(obj)/resume1/resume1.o        : $(obj)/resume1/resume1.code FORCE

$(obj)/resume1/resume1.code: FORCE
	$(Q)$(MAKE) $(build)=$(obj)/resume1
endif
endif
