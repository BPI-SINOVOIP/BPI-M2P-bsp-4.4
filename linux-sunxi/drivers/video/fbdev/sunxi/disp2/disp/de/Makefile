MIPI_DSI_IP_VERSION ?= 40

ifeq ($(CONFIG_ARCH_SUN8IW11),y)
sub_dir = lowlevel_v2x
endif

ifeq ($(CONFIG_ARCH_SUN8IW12),y)
sub_dir = lowlevel_v2x
MIPI_DSI_IP_VERSION := 28
endif

ifeq ($(CONFIG_ARCH_SUN8IW7),y)
sub_dir = lowlevel_sun8iw7
endif

-include $(obj)/de/$(sub_dir)/Makefile

