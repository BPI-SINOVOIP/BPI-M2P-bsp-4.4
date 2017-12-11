
################################################################################
#
# Common Variables that already set:
#     LICHEE_KDIR
#     LICHEE_MOD_DIR
#     CROSS_COMPILE
#     ARCH
#
#################################################################################
PWD=$(shell pwd)

ifneq ($(strip $(CONFIG_ARCH_SUN8IW12P1)),)
obj-$(CONFIG_SUNXI_NAND) += sun8iw12p1/
endif

ifneq ($(strip $(CONFIG_ARCH_SUN8IW7P1)),)
obj-m += sun8iw7p1/
endif

install: build
	@if cat ../../.config|grep "CONFIG_ARCH_SUN8IW7P1=y" >/dev/null 2>&1;then \
	cp sun8iw7p1/nand.ko $(LICHEE_MOD_DIR)/;\
	fi
	@if cat ../../.config|grep "CONFIG_ARCH_SUN8IW12P1=y" >/dev/null 2>&1;then \
	if cat ../../.config|grep "CONFIG_SUNXI_NAND=m" >/dev/null 2>&1;then \
	cp sun8iw12p1/nand.ko $(LICHEE_MOD_DIR)/;\
	else \
	echo "[NAND] CONFIG_SUNXI_NAND is not set";\
	fi \
	fi

build:
	@if cat ../../.config|grep "CONFIG_ARCH_SUN8IW7P1=y" >/dev/null 2>&1;then \
	cp sun8iw7p1/libnand_sun8iw7p1 sun8iw7p1/libnand_sun8iw7p1.a;\
	$(MAKE) -C $(LICHEE_KDIR) M=$(PWD); \
	fi
	@if cat ../../.config|grep "CONFIG_ARCH_SUN8IW12P1=y" >/dev/null 2>&1;then \
	if cat ../../.config|grep "CONFIG_SUNXI_NAND=m" >/dev/null 2>&1;then \
	cp sun8iw12p1/libnand_sun8iw12p1 sun8iw12p1/libnand_sun8iw12p1.a;\
	$(MAKE) -C $(LICHEE_KDIR) M=$(PWD); \
	else \
	echo "[NAND] CONFIG_SUNXI_NAND is not set";\
	fi \
	fi

clean:
	@rm -rf *.o *.ko .*.cmd *.mod.c *.order *.symvers .tmp_versions *~
