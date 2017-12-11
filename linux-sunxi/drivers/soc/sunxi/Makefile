obj-$(CONFIG_SUNXI_SRAM) +=	sunxi_sram.o
obj-y +=	sunxi-sid.o
ifdef CONFIG_ARCH_SUN8IW7
obj-$(CONFIG_SUNXI_SUSPEND) +=	power/
else
obj-$(CONFIG_SUNXI_SUSPEND) +=	pm/
endif
obj-$(CONFIG_SUNXI_ARISC)       += arisc/
