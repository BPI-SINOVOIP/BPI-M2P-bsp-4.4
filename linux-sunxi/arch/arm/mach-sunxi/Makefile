obj-$(CONFIG_ARCH_SUNXI)	+= sunxi.o

ifneq ($(CONFIG_ARM_PSCI), y)
obj-$(CONFIG_SMP)		+= headsmp.o platsmp.o
obj-$(CONFIG_HOTPLUG_CPU)	+= hotplug.o
endif

