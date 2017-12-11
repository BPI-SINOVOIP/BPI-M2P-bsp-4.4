/*
 * drivers/soc/sunxi/pm/standby/standby_twi.c
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Yanggq <yanggq@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include "standby.h"
#include "standby_twi.h"
#include "standby_printk.h"
#include "../pm_config.h"
#include "../resource/mem_twi.h"

#define TWI_CHECK_TIMEOUT       (0x4ffff)

static struct __twic_reg_t *TWI_REG_BASE[3] = {
	(struct __twic_reg_t *) IO_ADDRESS(AW_TWI0_BASE),
	(struct __twic_reg_t *) IO_ADDRESS(AW_TWI1_BASE),
	(struct __twic_reg_t *) IO_ADDRESS(AW_TWI2_BASE)
};

static __u32 TwiClkRegBak;
static __u32 TwiCtlRegBak;
static struct __twic_reg_t *twi_reg = (struct __twic_reg_t *) (0);

/*
 * init twi transfer when apb2 clk == 24M.
 */
__s32 standby_twi_init(int group)
{
	twi_reg = TWI_REG_BASE[group];
	TwiClkRegBak = twi_reg->reg_clkr;
	/* backup INT_EN; no need for BUS_EN(0xc0)  */
	TwiCtlRegBak = 0x80 & twi_reg->reg_ctl;
	/*twi_reg->reg_clkr = (2<<3)|3; //100k */
	/* M = 5, N = 0 : -> 24M/(2^N*(M+1)*10) = 24M/60=400k */
	twi_reg->reg_clkr = (5 << 3) | 0;

	twi_reg->reg_reset |= 0x1;
	while (twi_reg->reg_reset & 0x1)
		;

	return 0;
}

/*
 * init twi transfer when apb2 clk == 32K.
 */
__s32 standby_twi_init_losc(int group)
{
	/* M = 0, N = 0 : -> 24M/(2^N*(M+1)*10) = 32k/10=3.2k */
	twi_reg->reg_clkr = (5 << 3) | 0;
}

/*
 * exit twi transfer.
 */
__s32 standby_twi_exit(void)
{
	/* softreset twi module */
	twi_reg->reg_reset |= 0x1;

	/* delay */
	/*change_runtime_env(); */
	/*delay_ms(10); */

	/* restore clock division */
	twi_reg->reg_clkr = TwiClkRegBak;
	/* restore INT_EN */
	twi_reg->reg_ctl |= TwiCtlRegBak;

	return 0;
}

/*
 * stop current twi transfer.
 */
static int _standby_twi_stop(void)
{
	unsigned int nop_read;
	unsigned int timeout = TWI_CHECK_TIMEOUT;

	/* set stop+clear int flag */
	twi_reg->reg_ctl = (twi_reg->reg_ctl & 0xc0) | 0x10 | 0x08;

	nop_read = twi_reg->reg_ctl;
	nop_read = nop_read;

	/* 1. stop bit is zero. */
	while ((twi_reg->reg_ctl & 0x10) && (--timeout))
		;

	if (timeout == 0)
		return -1;

	/* 2. twi fsm is idle(0xf8). */
	timeout = TWI_CHECK_TIMEOUT;
	while ((twi_reg->reg_status != 0xf8) && (--timeout))
		;

	if (timeout == 0)
		return -1;

	/* 3. twi scl & sda must high level. */
	timeout = TWI_CHECK_TIMEOUT;
	while ((twi_reg->reg_lctl != 0x3a) && (--timeout))
		;

	if (timeout == 0)
		return -1;

	return 0;
}

/*
 * twi byte read and write.
 *
 *Arguments  : op        operation read or write;
 *             saddr     slave address;
 *             baddr     byte address;
 *             data      pointer to the data to be read or write;
 *
 *Return     :
 *             EPDK_OK,      byte read or write successed;
 *             EPDK_FAIL,    btye read or write failed!
 */
__s32 twi_byte_rw(enum twi_op_type_e op, __u8 saddr, __u8 baddr, __u8 *data)
{
	unsigned char state_tmp;
	unsigned int timeout;
	int ret = -1;
	unsigned int ctrl;

	_standby_twi_stop();

	standby_printk("op = 0x%x, saddr = 0x%x, baddr = 0x%x, data = 0x%x\n",
		op, saddr, baddr, *data);

	twi_reg->reg_efr = 0;

	state_tmp = twi_reg->reg_status;
	if (state_tmp != 0xf8) {
		ret = 0xf8;
		goto stop_out;
	}

	/* control registser bitmap
	 *    7      6       5     4       3       2    1    0
	 * INT_EN  BUS_EN  START  STOP  INT_FLAG  ACK  NOT  NOT
	 */

	/*1.Send Start */
	twi_reg->reg_ctl |= 0x20;
	timeout = TWI_CHECK_TIMEOUT;
	while ((!(twi_reg->reg_ctl & 0x08)) && (--timeout))
		;

	if (timeout == 0) {
		ret = 0xff;
		goto stop_out;
	}

	state_tmp = twi_reg->reg_status;
	if (state_tmp != 0x08) {
		ret = 0x08;
		goto stop_out;
	}

	/*2.Send Slave Address, slave address + write */
	twi_reg->reg_data = ((saddr << 1) & 0xfe) | 0;
	twi_reg->reg_ctl &= 0xCF;
	timeout = TWI_CHECK_TIMEOUT;
	while ((!(twi_reg->reg_ctl & 0x08)) && (--timeout))
		;

	if (timeout == 0) {
		ret = 0xfe;
		goto stop_out;
	}

	state_tmp = twi_reg->reg_status;
	if (state_tmp != 0x18) {
		ret = 0x18;
		goto stop_out;
	}

	/*3.Send Byte Address */
	twi_reg->reg_data = baddr;
	twi_reg->reg_ctl &= 0xCF;
	timeout = TWI_CHECK_TIMEOUT;
	while ((!(twi_reg->reg_ctl & 0x08)) && (--timeout))
		;

	if (timeout == 0) {
		ret = 0xfd;
		goto stop_out;
	}

	state_tmp = twi_reg->reg_status;
	if (state_tmp != 0x28) {
		ret = 0x28;
		goto stop_out;
	}

	if (op == TWI_OP_WR) {
		/*4.Send Data to be write */
		twi_reg->reg_data = *data;
		twi_reg->reg_ctl &= 0xCF;
		timeout = TWI_CHECK_TIMEOUT;
		while ((!(twi_reg->reg_ctl & 0x08)) && (--timeout))
			;

		if (timeout == 0) {
			ret = 0xfc;
			goto stop_out;
		}

		state_tmp = twi_reg->reg_status;
		if (state_tmp != 0x28) {
			ret = 0x28;
			goto stop_out;
		}
	} else {
		/* 4. Send restart for read, set start+clear int flag */
		twi_reg->reg_ctl = (twi_reg->reg_ctl & 0xc0) | 0x20 | 0x08;
		timeout = TWI_CHECK_TIMEOUT;
		while ((!(twi_reg->reg_ctl & 0x08)) && (--timeout))
			;

		if (timeout == 0) {
			ret = 0xfb;
			goto stop_out;
		}

		state_tmp = twi_reg->reg_status;
		if (state_tmp != 0x10) {
			ret = 0x10;
			goto stop_out;
		}

		/*5.Send Slave Address, slave address+ read */
		twi_reg->reg_data = (saddr << 1) | 1;
		/* clear int flag then 0x40 come in */
		twi_reg->reg_ctl &= 0xCF;
		timeout = TWI_CHECK_TIMEOUT;
		while ((!(twi_reg->reg_ctl & 0x08)) && (--timeout))
			;

		if (timeout == 0) {
			ret = 0xfa;
			goto stop_out;
		}

		state_tmp = twi_reg->reg_status;
		if (state_tmp != 0x40) {
			ret = 0x40;
			goto stop_out;
		}

		/*6.Get data, clear int flag then data come in */
		twi_reg->reg_ctl &= 0xCF;
		timeout = TWI_CHECK_TIMEOUT;
		while ((!(twi_reg->reg_ctl & 0x08)) && (--timeout))
			;

		if (timeout == 0) {
			ret = 0xf9;
			goto stop_out;
		}

		*data = twi_reg->reg_data;
		state_tmp = twi_reg->reg_status;
		if (state_tmp != 0x58) {
			ret = 0x58;
			goto stop_out;
		}
	}

	ret = 0;

stop_out:
	/* WRITE: step 5; READ: step 7 */
	/* Send Stop */
	if (ret)
		standby_printk("NOTICE: state_tmp=0x%x, ret=0x%x\n",
				state_tmp, ret);

	_standby_twi_stop();

	if (op == 0)
		standby_printk("after read: data=0x%x\n", *data);

	return ret;
}
