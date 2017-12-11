/*
 *    Filename: cve_api_include.h
 *     Version: 1.0
 * Description: CVE driver CMD, Don't modify it in user space.
 *     License: GPLv2
 *
 *		Author  : gaofeng <xiongyi@allwinnertech.com>
 *		Date    : 2017/04/07
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
#ifndef CVE_API_INCLUDE_H
#define CVE_API_INCLUDE_H

#define CVE_MAGIC 'x'

#define CVE_READ_RESNUM			_IO(CVE_MAGIC, 0)
#define CVE_WRITE_REGISTER		_IO(CVE_MAGIC, 1)
#define CVE_READ_REGISTER		_IO(CVE_MAGIC, 2)
#define CVE_OPEN_CLK			_IO(CVE_MAGIC, 3)
#define CVE_CLOSE_CLK			_IO(CVE_MAGIC, 4)
#define CVE_PLL_SET				_IO(CVE_MAGIC, 5)
#define CVE_SYS_RESET			_IO(CVE_MAGIC, 6)
#define CVE_MOD_RESET			_IO(CVE_MAGIC, 7)
#define IOCTL_WAIT_CVE			_IO(CVE_MAGIC, 8)

struct cve_register{
	unsigned long addr;
	unsigned long value;
};

#endif
