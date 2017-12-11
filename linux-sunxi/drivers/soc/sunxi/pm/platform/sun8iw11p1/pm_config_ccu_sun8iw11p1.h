/*
 * drivers/soc/sunxi/pm/platform/sun8iw11p1/pm_config_ccu_sun8iw11p1.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef __PM_CONFIG_CCU_SUN8W11P1_H__
#define __PM_CONFIG_CCU_SUN8W11P1_H__

#include <linux/types.h>

union __ccmu_pll1_reg0000_t {
	__u32 dwval;
	struct {
		__u32 FactorM:2;	/*bit0,  PLL1 Factor M */
		__u32 reserved0:2;	/*bit2,  reserved */
		__u32 FactorK:2;	/*bit4,  PLL1 factor K */
		__u32 reserved1:2;	/*bit6,  reserved */
		__u32 FactorN:5;	/*bit8,  PLL1 Factor N */
		__u32 reserved2:3;	/*bit13, reserved */
		__u32 FactorP:2;	/*bit16, PLL1 Factor P */
		__u32 reserved3:6;	/*bit18, reserved */
		__u32 SigmaEn:1;	/*bit24, sigma delta enbale */
		__u32 reserved4:3;	/*bit25, reserved */
		__u32 Lock:1;		/*bit28, 1-pll has stabled */
		__u32 reserved5:2;	/*bit29, reserved */
		__u32 PLLEn:1;		/*bit31, (24Mhz*N*K)/(M*P) */
	} bits;
};

struct __ccmu_pll2_reg0008_t {
	__u32 FactorM:5;	/*bit0,  PLL2 prev division M */
	__u32 reserved0:3;	/*bit5,  reserved */
	__u32 FactorN:7;	/*bit8,  PLL2 factor N */
	__u32 reserved1:1;	/*bit15, reserved */
	__u32 FactorP:4;	/*bit16, PLL2 factor P */
	__u32 reserved2:4;	/*bit20, reserved */
	__u32 SdmEn:1;		/*bit24, sdm enable */
	__u32 reserved3:3;	/*bit25, reserved */
	__u32 Lock:1;		/*bit28, pll stable flag */
	__u32 reserved4:2;	/*bit29, reserved */
	__u32 PLLEn:1;		/*bit31, PLL2 enable */
};

struct __ccmu_media_pll_t {
	__u32 FactorM:4;	/*bit0,  PLL3 FactorM */
	__u32 reserved0:4;	/*bit4,  reserved */
	__u32 FactorN:7;	/*bit8,  PLL factor N */
	__u32 reserved1:5;	/*bit15, reserved */
	__u32 SdmEn:1;		/*bit20, sdm enable */
	__u32 reserved2:3;	/*bit21, reserved */
	__u32 ModeSel:1;	/*bit24, PLL mode select */
	__u32 FracMod:1;	/*bit25, PLL out is 0:270Mhz, 1:297Mhz */
	__u32 reserved3:2;	/*bit26, reserved */
	__u32 Lock:1;		/*bit28, lock flag */
	__u32 reserved4:1;	/*bit29, reserved */
	__u32 CtlMode:1;	/*bit30, 0-control by cpu, 1-control by DE */
	__u32 PLLEn:1;		/*bit31, PLL3 enable */
};

struct __ccmu_pll5_reg0020_t {
	__u32 FactorM:2;	/*bit0,  PLL5 factor M */
	__u32 reserved0:2;	/*bit2,  reserved */
	__u32 FactorK:2;	/*bit4,  PLL1 factor K */
	__u32 reserved1:2;	/*bit6,  reserved */
	__u32 FactorN:5;	/*bit8,  PLL5 factor N */
	__u32 reserved2:7;	/*bit13, reserved */
	__u32 PllUpdate:1;	/*bit20, set to 1 to validate the pll */
	__u32 reserved3:3;	/*bit21, reserved */
	__u32 SdmEn:1;		/*bit24, sdm enable */
	__u32 reserved4:3;	/*bit25, reserved */
	__u32 Lock:1;		/*bit28, lock flag,pll stable flag */
	__u32 reserved5:2;	/*bit29, reserved */
	__u32 PLLEn:1;		/*bit31, PLL5 Enable */
};

struct __ccmu_pll6_reg0028_t {
	__u32 FactorM:2;	/*bit0,  PLL6 factor M */
	__u32 reserved0:2;	/*bit2,  reserved */
	__u32 FactorK:2;	/*bit4,  PLL6 factor K */
	__u32 reserved1:2;	/*bit6,  reserved */
	__u32 FactorN:5;	/*bit8,  PLL6 factor N */
	__u32 reserved2:3;	/*bit13, reserved */
	__u32 Pll24MPdiv:2;	/*bit16, PLL 24M output clock post divider */
	__u32 Pll24MOutEn:1;	/*bit18, PLL 24M output enable */
	__u32 reserved3:5;	/*bit19, reserved */
	__u32 PllClkOutEn:1;	/*bit24, pll clock output enable */
	__u32 PLLBypass:1;	/*bit25, PLL6 output bypass enable */
	__u32 reserved4:2;	/*bit26, reserved */
	__u32 Lock:1;		/*bit28, lock flag */
	__u32 reserved5:2;	/*bit29, reserved */
	__u32 PLLEn:1;		/*bit31, PLL6 enable */
};

#define AC327_CLKSRC_LOSC   (0)
#define AC327_CLKSRC_HOSC   (1)
#define AC327_CLKSRC_PLL1   (2)

union __ccmu_sysclk_ratio_reg0050_t {
	__u32 dwval;
	struct {
		__u32 AXIClkDiv:2;	/*bit0,  00-1, 01-2, 10-3, 11-4 */
		__u32 reserved0:14;	/*bit2,  reserved */
		__u32 CpuClkSrc:2;	/*bit16, 00-LOSC, 01-HOSC, 10/11-PLL1 */
		__u32 reserved2:14;	/*bit18, reserved */
	} bits;
};

#define AHB1_CLKSRC_LOSC    (0)
#define AHB1_CLKSRC_HOSC    (1)
#define AHB1_CLKSRC_AXI     (2)
#define AHB1_CLKSRC_PLL6    (3)

union __ccmu_ahb1_ratio_reg0054_t {
	__u32 dwval;
	struct {
		__u32 reserved0:4;	/*bit0,  reserved */
		__u32 Ahb1Div:2;	/*bit4,  1/2/4/8 */
		__u32 Ahb1PreDiv:2;	/*bit6,  ratio 1/2/3/4 */
		__u32 Apb1Div:2;	/*bit8,  2/2/4/8, source is ahb1 */
		__u32 reserved1:2;	/*bit10, reserved */
		__u32 Ahb1ClkSrc:2;	/*bit12, 00-LO,01-24M,10-AXI,11-PLL6 */
		__u32 reserved2:18;	/*bit26, reserved */
	} bits;
};

#define APB2_CLKSRC_LOSC    (0)
#define APB2_CLKSRC_HOSC    (1)
#define APB2_CLKSRC_PLL24M  (2)
#define APB2_CLKSRC_PLL6    (3)

union __ccmu_apb2_ratio_reg0058_t {
	__u32 dwval;
	struct {
		__u32 DivM:5;		/*bit0,  clock divide ratio m */
		__u32 reserved:11;	/*bit5,  reserved */
		__u32 DivN:2;		/*bit16, clock pre-div ratio 1/2/4/8 */
		__u32 reserved1:6;	/*bit18, reserved */
		__u32 ClkSrc:2;		/*bit24, 00-LO,01-24M,10/11-PLL6 */
		__u32 reserved2:6;	/*bit26, reserved */
	} bits;
};

struct  __ccmu_reg_list_t {
	volatile union __ccmu_pll1_reg0000_t Pll1Ctl;	/* PLL1 ctrl, cpux */
	volatile __u32 reserved0;	/* 0x004, reserved */
	volatile struct __ccmu_pll2_reg0008_t Pll2Ctl;	/* PLL2 ctrl, audio */
	volatile __u32 reserved1;	/* 0x00c, reserved */
	volatile __u32 Pll3Ctl;		/* 0x010, PLL3 control, video0 */
	volatile __u32 reserved2;	/* 0x014, reserved */
	volatile __u32 Pll4Ctl;		/* 0x018, PLL4 control, ve */
	volatile __u32 reserved3;	/* 0x01c, reserved */
	volatile struct __ccmu_pll5_reg0020_t Pll5Ctl;	/* PLL5 ctrl, ddr0 */
	volatile __u32 reserved4;	/* 0x024, reserved */
	volatile struct __ccmu_pll6_reg0028_t Pll6Ctl;	/* PLL6 ctrl, periph0 */
	volatile __u32 Pll7Ctl;		/* 0x02c, PLL7 ctrl, periph1 */
	volatile __u32 PllVideo1;	/* 0x030, Pll videio1 reg */
	volatile __u32 PllSata;		/* 0x034, sata ctrl reg */
	volatile __u32 Pll8Ctl;		/* 0x038, PLL8 control, gpu */
	volatile __u32 reserved5;	/* 0x03c, reserved */
	volatile __u32 PllMipi;		/* 0x040, MIPI PLL control */
	volatile __u32 reserved6;	/* 0x044, reserved */
	volatile __u32 Pll10Ctl;	/* 0x048, PLL10 control, de */
	volatile __u32 PllDdr1Ctl;	/* 0x04c, pll ddr1 ctrl reg */
	/* cpux/axi clock divide ratio */
	volatile union __ccmu_sysclk_ratio_reg0050_t SysClkDiv;
	/* ahb1/apb1 clock divide ratio */
	volatile union __ccmu_ahb1_ratio_reg0054_t Ahb1Div;
	/* apb2 clock divide ratio */
	volatile union __ccmu_apb2_ratio_reg0058_t Apb2Div;
	volatile __u32 reserved7;	/* 0x05c, reserved */
	volatile __u32 AhbGate0;	/* 0x060, bus gating reg0 */
	volatile __u32 AhbGate1;	/* 0x064, bus gating reg1 */
	volatile __u32 Apb1Gate;	/* 0x068, bus gating reg2 */
	volatile __u32 Apb2Gate0;	/* 0x06c, bus gating reg3 */
	volatile __u32 Apb2Gate1;	/* 0x070, bus gating reg4 */
	volatile __u32 Ths;		/* 0x074, ths clk reg */
	volatile __u32 reserved8[2];	/* 0x078, reseved */
	volatile __u32 Nand0;		/* 0x080, nand clock */
	volatile __u32 reserved9;	/* 0x084, reserved */
	volatile __u32 Sd0;		/* 0x088, sd/mmc0 clock */
	volatile __u32 Sd1;		/* 0x08c, sd/mmc1 clock */
	volatile __u32 Sd2;		/* 0x090, sd/mmc2 clock */
	volatile __u32 Sd3;		/* 0x094, sd/mmc3 clock */
	volatile __u32 Ts;		/* 0x098, ts clk reg */
	volatile __u32 Ce;		/* 0x09c, ce clk reg */
	volatile __u32 Spi0;		/* 0x0a0, spi controller 0 clock */
	volatile __u32 Spi1;		/* 0x0a4, spi controller 1 clock */
	volatile __u32 Spi2;		/* 0x0a8, spi controller 2 clock */
	volatile __u32 Spi3;		/* 0x0ac, spi controller 3 clock */
	volatile __u32 I2s0;		/* 0x0b0, daudio-0 clock */
	volatile __u32 I2s1;		/* 0x0b4, daudio-1 clock */
	volatile __u32 I2s2;		/* 0x0b8, daudio-2 clock */
	volatile __u32 Ac97;		/* 0x0bc, ac97 clock */
	volatile __u32 SpdifClk;	/* 0x0c0, spdif clock */
	volatile __u32 KeyPadClk;	/* 0x0c4, keypad clock */
	volatile __u32 Sata0Clk;	/* 0x0c8, sata clock */
	volatile __u32 UsbClk;		/* 0x0cc, usb phy clock */
	volatile __u32 Ir0Clk;		/* 0x0d0, ir0 clock */
	volatile __u32 Ir1Clk;		/* 0x0d4, ir1 clock */
	volatile __u32 reserved10[6];	/* 0x0d8, reserved*/
	volatile __u32 PllDdrAuxiliary;	/* 0x0f0, pll ddr auxiloary reg */
	volatile __u32 DramCfg;		/* 0x0f4, dram configuration clock */
	volatile __u32 PllDdr1Cfg;	/* 0x0f8, pll ddr1 config reg */
	volatile __u32 MbusResetReg;	/* 0x0fc, mbus reset reg */
	volatile __u32 DramGate;	/* 0x100, dram module clock */
	volatile __u32 DeClk;		/* 0x104, DE clock reg */
	volatile __u32 DeMpClk;		/* 0x108, DE_MP clock reg */
	volatile __u32 reserved11;	/* 0x10c, de_mp clock */
	volatile __u32 Lcd0Ch0;		/* 0x110, tcon lcd0 clock */
	volatile __u32 Lcd1Ch0;		/* 0x114, tcon lcd1 clock */
	volatile __u32 Tv0Ch0;		/* 0x118, tcon tv0 clock */
	volatile __u32 Tv1Ch0;		/* 0x11c, tcon tv1 clock */
	volatile __u32 reserved12;	/* 0x120, reserved */
	volatile __u32 DeinterlaceClk;	/* 0x124, deinterlace clock */
	volatile __u32 reserved13[2];	/* 0x128, reserved */
	volatile __u32 CsiMisc;		/* 0x130, reserved */
	volatile __u32 Csi0;		/* 0x134, csi0 module clock */
	volatile __u32 reserved14;	/* 0x138, reserved */
	volatile __u32 Ve;		/* 0x13c, Ve clock reg */
	volatile __u32 Adda;		/* 0x140, ac digital clock register */
	volatile __u32 Avs;		/* 0x144, avs module clock */
	volatile __u32 reserved15[2];	/* 0x148, reserved */
	volatile __u32 HdmiClk;		/* 0x150, HDMI clock reg */
	volatile __u32 HdmiSlowClk;	/* 0x154, HDMI slow clock */
	volatile __u32 reserved16;	/* 0x158, reserved */
	volatile __u32 MBus0;		/* 0x15C, MBUS controller 0 clock */
	volatile __u32 reserved17;	/* 0x160, reserved clock */
	volatile __u32 Gmac;		/* 0x164, gmac clock */
	volatile __u32 MipiDsiClk;	/* 0x168, MIPI_DSI clock */
	volatile __u32 reserved18[5];	/* 0x16c-0x017c, reserved */
	volatile __u32 TvE0Clk;		/* 0x180, TVE0 clock */
	volatile __u32 TvE1Clk;		/* 0x184, TVE1 clock */
	volatile __u32 TvD0Clk;		/* 0x188, TVD0 clock */
	volatile __u32 TvD1Clk;		/* 0x18c, TVD1 clock */
	volatile __u32 TvD2Clk;		/* 0x190, TVD2 clock */
	volatile __u32 TvD3Clk;		/* 0x194, TVD3 clock */
	volatile __u32 reserved19[2];	/* 0x198, reserved */
	volatile __u32 GpuClk;		/* 0x1a0, GPU clock */
	volatile __u32 reserved20[19];	/* 0x1a4 ~ 0x01ec, reserved */
	volatile __u32 OutAClk;		/* 0x1f0, clock output A reg */
	volatile __u32 OutBClk;		/* 0x1f4, clock output B reg */
	volatile __u32 reserved21[8];	/* 0x01f8 ~ 0x0214, reserved */
	volatile __u32 PllSataBias;	/* 0x218*/
	volatile __u32 PllPeriph1Bias;	/* 0x21c, periph1 hsic  bias reg */
	volatile __u32 PllxBias[1];	/* 0x220, pll cpux  bias reg */
	volatile __u32 PllAudioBias;	/* 0x224, pll audio bias reg */
	volatile __u32 PllVideo0Bias;	/* 0x228, pll vedio bias reg */
	volatile __u32 PllVeBias;	/* 0x22c, pll ve    bias reg */
	volatile __u32 PllDram0Bias;	/* 0x230, pll dram0 bias reg */
	volatile __u32 PllPeriph0Bias;	/* 0x234, pll periph0 bias reg */
	volatile __u32 PllVideo1Bias;	/* 0x238, pll video1 bias reg */
	volatile __u32 PllGpuBias;	/* 0x23c, pll gpu bias reg */
	volatile __u32 reserved22[2];	/* 0x240, reserved */
	volatile __u32 PllDeBias;	/* 0x248, pll de bias reg */
	volatile __u32 PllDram1BiasReg;	/* 0x24c, pll dram1 bias */
	volatile __u32 Pll1Tun;		/* 0x250, pll1 tun,cpux tuning reg */
	volatile __u32 reserved23[3];	/* 0x254-0x25c, reserved */
	volatile __u32 PllDdr0Tun;	/* 0x260, pll ddr0 tuning */
	volatile __u32 reserved24[3];	/* 0x264-0x26c, reserved */
	volatile __u32 pllMipiTun;	/* 0x70, mipi tuning reg*/
	volatile __u32 reserved32[2];	/* 0x274-0x278, reserved */
	volatile __u32 PllPeriph1Pattern;/* 0x27c, pll periph1 pattern reg */
	volatile __u32 Pll1Pattern;	/* 0x280, pll cpux  pattern reg */
	volatile __u32 PllAudioPattern;	/* 0x284, pll audio pattern reg */
	volatile __u32 PllVedio0Pattern;/* 0x288, pll vedio pattern reg */
	volatile __u32 PllVePattern;	/* 0x28c, pll ve pattern reg */
	volatile __u32 PllDdr0Pattern;	/* 0x290, pll ddr0 pattern reg */
	volatile __u32 reserved25;	/* 0x294, reserved */
	volatile __u32 PllVedio1Pattern;/* 0x298, pll video1 pattern reg */
	volatile __u32 PllGpuPattern;	/* 0x29c, pll gpu pattern reg */
	volatile __u32 reserved26[2];	/* 0x2a0, reserved */
	volatile __u32 PllDePattern;	/* 0x2a8, pll de pattern reg */
	volatile __u32 PllDram1PatternReg0;/* 0x2ac, pll dram1 pattern reg0 */
	volatile __u32 PllDram1PatternReg1;/* 0x2b0, pll dram1 pattern reg1 */
	volatile __u32 reserved27[3];	/* 0x2b4, reserved */
	volatile __u32 AhbReset0;	/* 0x2c0, bus soft reset register 0 */
	volatile __u32 AhbReset1;	/* 0x2c4, bus soft reset register 1 */
	volatile __u32 AhbReset2;	/* 0x2c8, bus soft reset register 2 */
	volatile __u32 reserved28;	/* 0x2cc, reserved */
	volatile __u32 Apb1Reset;	/* 0x2d0, bus soft reset register 3 */
	volatile __u32 reserved29;	/* 0x2d4, reserved */
	volatile __u32 Apb2Reset;	/* 0x2d8, bus soft reset register 4 */
	volatile __u32 reserved30[9];	/* 0x2dc-0x2fc, reserved */
	volatile __u32 PsCtrl;		/* 0x300, PS control register */
	volatile __u32 PsCnt;		/* 0x304, PS counter register */
	volatile __u32 reserved31[2];	/* 0x308,0x30c reserved */
	volatile __u32 Sys32kClk;	/* 0x310, system 32k clk reg */
	volatile __u32 reserved33[3];	/* 0x314-0x31c, reserved */
	volatile __u32 PllLockCtrl;	/* 0x320, Pll Lock Ctrl register */
};

#endif /* __PM_CONFIG_CCU_SUN8W11P1_H__ */
