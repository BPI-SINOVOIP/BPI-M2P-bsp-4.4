#makefile for standby.bin
always  := standby.code
targets := standby.elf

STANDBY_XN_FILES := standby.xn

GCOV_PROFILE=no

#use "-Os" flags.
#Don't use "-O2" flags.
KBUILD_CFLAGS := -g -c -nostdlib -march=armv7-a -D__LINUX_ARM_ARCH__=7 -marm -fno-unwind-tables -fno-jump-tables -fno-asynchronous-unwind-tables -mlittle-endian -O2 -mno-unaligned-access

#Include the cur dir.
KBUILD_CPPFLAGS += -I.

LD_FLAGS = -static
LIBS =

INCLUDE   = -I.                              \
            -I$(KDIR)/include                \
            -I$(KDIR)/drivers/soc/sunxi/pm

standby-y := standby.o                       \
	     standby_common.o                \
	     standby_twi.o                   \
	     standby_clock.o                 \
	     standby_serial.o                \
	     standby_printk.o                \
	     standby_mmu_pc_div_asm.o        \
	     axp/axp_core.o                  \
	     axp/axp15.o                     \
	     axp/axp20.o                     \
	     axp/axp22.o                     \
	     ../resource/mem_clk.o           \
	     ../resource/mem_timing.o

ifeq ($(CONFIG_ARCH_SUN8IW11P1),y)
standby-y += dram/sun8iw11p1/mctl_standby-sun8iw11.o
endif

standby-y := $(addprefix $(obj)/,$(standby-y))

$(obj)/standby.code: $(obj)/standby.elf FORCE
	$(Q)$(CROSS_COMPILE)objcopy -O binary $(obj)/standby.elf $(obj)/standby.code
#$(call if_changed,objcopy)
	rm -rf *.o $(obj)/./*.o


$(obj)/standby.elf: $(obj)/$(STANDBY_XN_FILES) $(standby-y)
	$(Q)$(CROSS_COMPILE)ld -T $(obj)/$(STANDBY_XN_FILES) $(LD_FLAGS) $(LIBS) -EL  $(standby-y) -o $(obj)/standby.elf -Map $(obj)/standby.map
	$(Q)$(CROSS_COMPILE)objdump -D $(obj)/standby.elf > $(obj)/standby.lst

#$(call if_changed,ld)

clean:
	-rm $(obj) *.code *.map *.lst *.elf
