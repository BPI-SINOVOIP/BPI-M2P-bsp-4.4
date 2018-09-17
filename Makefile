.PHONY: all clean help
.PHONY: u-boot kernel kernel-config
.PHONY: linux pack

include chosen_board.mk

SUDO=sudo
U_CROSS_COMPILE=$(U_COMPILE_TOOL)/arm-linux-gnueabi-
K_CROSS_COMPILE=$(K_COMPILE_TOOL)/arm-linux-gnueabihf-

LICHEE_KDIR=$(CURDIR)/linux-sunxi
LICHEE_MOD_DIR=$(LICHEE_KDIR)/output/lib/modules/4.4.55-BPI-M2P-Kernel
ROOTFS=$(CURDIR)/rootfs/linux/default_linux_rootfs.tar.gz

Q=
J=$(shell expr `grep ^processor /proc/cpuinfo  | wc -l` \* 2)

all: bsp

## clean all
clean: u-boot-clean kernel-clean
	rm -f chosen_board.mk

## pack
pack: sunxi-pack
	$(Q)scripts/mk_pack.sh

u-boot:
	$(Q)$(MAKE) -C u-boot-sunxi $(MACH)_config CROSS_COMPILE=$(U_CROSS_COMPILE) -j$J
	$(Q)$(MAKE) -C u-boot-sunxi all CROSS_COMPILE=$(U_CROSS_COMPILE) -j$J

u-boot-clean:
	$(Q)$(MAKE) -C u-boot-sunxi CROSS_COMPILE=$(U_CROSS_COMPILE) -j$J distclean

## linux
$(K_DOT_CONFIG): linux-sunxi
	mkdir -p linux-sunxi/output/lib/firmware

kernel:
	$(Q)$(MAKE) -C linux-sunxi ARCH=arm $(KERNEL_CONFIG)
	$(Q)$(MAKE) -C linux-sunxi ARCH=arm CROSS_COMPILE=${K_CROSS_COMPILE} -j$J INSTALL_MOD_PATH=output uImage dtbs modules
	$(Q)$(MAKE) -C linux-sunxi ARCH=arm CROSS_COMPILE=${K_CROSS_COMPILE} -j$J INSTALL_MOD_PATH=output modules_install
	$(Q)$(MAKE) -C linux-sunxi/modules/gpu CROSS_COMPILE=$(K_CROSS_COMPILE) ARCH=arm LICHEE_MOD_DIR=${LICHEE_MOD_DIR} LICHEE_KDIR=${LICHEE_KDIR}
	$(Q)$(MAKE) -C linux-sunxi/modules/gpu CROSS_COMPILE=$(K_CROSS_COMPILE) ARCH=arm LICHEE_MOD_DIR=${LICHEE_MOD_DIR} LICHEE_KDIR=${LICHEE_KDIR} install
	$(Q)scripts/install_kernel_headers.sh $(K_CROSS_COMPILE)

kernel-clean:
	$(Q)$(MAKE) -C linux-sunxi/modules/gpu CROSS_COMPILE=$(K_CROSS_COMPILE) ARCH=arm LICHEE_MOD_DIR=${LICHEE_MOD_DIR} LICHEE_KDIR=${LICHEE_KDIR} clean
	$(Q)$(MAKE) -C linux-sunxi ARCH=arm CROSS_COMPILE=${K_CROSS_COMPILE} -j$J distclean
	rm -rf linux-sunxi/output/
	rm -f linux-sunxi/bImage

kernel-config:
	$(Q)$(MAKE) -C linux-sunxi ARCH=arm $(KERNEL_CONFIG)
	$(Q)$(MAKE) -C linux-sunxi ARCH=arm CROSS_COMPILE=${K_CROSS_COMPILE} -j$J menuconfig
	cp linux-sunxi/.config linux-sunxi/arch/arm/configs/$(KERNEL_CONFIG)

## bsp
bsp: u-boot kernel

## linux
linux: 
	$(Q)scripts/mk_linux.sh $(ROOTFS)

help:
	@echo ""
	@echo "Usage:"
	@echo "  make bsp             - Default 'make'"
	@echo "  make linux         - Build target for linux platform, as ubuntu, need permisstion confirm during the build process"
	@echo "   Arguments:"
	@echo "    ROOTFS=            - Source rootfs (ie. rootfs.tar.gz with absolute path)"
	@echo ""
	@echo "  make pack            - pack the images and rootfs to a PhenixCard download image."
	@echo "  make clean"
	@echo ""
	@echo "Optional targets:"
	@echo "  make kernel           - Builds linux kernel"
	@echo "  make kernel-config    - Menuconfig"
	@echo "  make u-boot          - Builds u-boot"
	@echo ""

