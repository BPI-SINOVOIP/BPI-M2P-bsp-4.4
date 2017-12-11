	.globl  arisc_binary_start
	.globl  arisc_binary_end
arisc_binary_start:
#if defined CONFIG_ARCH_SUN8IW12P1
	.incbin "drivers/soc/sunxi/arisc/binary/arisc_sun8iw12p1.bin"
#elif defined CONFIG_ARCH_SUN8IW7P1
	.incbin "drivers/soc/sunxi/arisc/binary/arisc_sun8iw7p1.bin"
#else
#error "please select a platform\n"
#endif
arisc_binary_end:
	.align  2

