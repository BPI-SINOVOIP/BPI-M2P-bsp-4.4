/*
 * drivers/staging/android/ion/sunxi/sunxi_ion.c
 *
 * Copyright(c) 2015-2020 Allwinnertech Co., Ltd.
 *      http://www.allwinnertech.com
 *
 * Author: Wim Hwang <huangwei@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/of.h>

#include "../ion.h"
#include "../ion_priv.h"
#include "../../uapi/ion_sunxi.h"
#include "ion_sunxi.h"

static unsigned int  ion_sunxi_drm_phy_addr, ion_sunxi_drm_tee_addr;
static struct ion_device *idev;
struct ion_handle *ion_handle_get_by_id(struct ion_client *client, int id);
int ion_handle_put(struct ion_handle *handle);


void sunxi_ion_probe_drm_info(uint32_t *drm_phy_addr, uint32_t *drm_tee_addr)
{
	*drm_phy_addr = ion_sunxi_drm_phy_addr;
	*drm_tee_addr = ion_sunxi_drm_tee_addr;
}
EXPORT_SYMBOL(sunxi_ion_probe_drm_info);

struct ion_client *sunxi_ion_client_create(const char *name)
{
	/*
	 * The assumption is that if there is a NULL device, the ion
	 * driver has not yet probed.
	 */
	if (IS_ERR_OR_NULL(idev))
		return ERR_PTR(-EPROBE_DEFER);

	return ion_client_create(idev, name);
}
EXPORT_SYMBOL(sunxi_ion_client_create);

long sunxi_ion_ioctl(struct ion_client *client, unsigned int cmd,
		     unsigned long arg)
{
	long ret = 0;

	switch (cmd) {
	case ION_IOC_SUNXI_PHYS_ADDR:
	{
		struct sunxi_phys_data data;
		struct ion_handle *handle;

		if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
			return -EFAULT;

		handle = ion_handle_get_by_id(client, data.handle);
		if (IS_ERR(handle))
			return PTR_ERR(handle);

		data.size = 0;
		ret = ion_phys(client, handle,
			       (ion_phys_addr_t *)&data.phys_addr,
			       (size_t *)&data.size);
		ion_handle_put(handle);
		if (ret)
			return -EINVAL;

		if (copy_to_user((void __user *)arg, &data, sizeof(data)))
			return -EFAULT;
		break;
	}
	case ION_IOC_SUNXI_TEE_ADDR:
	{
		struct sunxi_phys_data data;
		struct ion_handle *handle;
		if (copy_from_user(&data, (void __user *)arg,
			sizeof(struct sunxi_phys_data)))
			return -EFAULT;

		handle = ion_handle_get_by_id(client, data.handle);
		if (IS_ERR(handle))
			return PTR_ERR(handle);

		data.size = 0xffff;
		ret = ion_phys(client, handle,
			(ion_phys_addr_t *)&data.phys_addr,
			(size_t *)&data.size);
		ion_handle_put(handle);
		if (ret)
			return -EINVAL;

		if (copy_to_user((void __user *)arg, &data, sizeof(data)))
			return -EFAULT;
		break;
	}
	default:
		pr_err("%s(%d) err: cmd not support!\n", __func__, __LINE__);
		return -ENOTTY;
	}

	return ret;
}

static int sunxi_ion_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct ion_platform_heap heaps_desc;
	struct device_node *heap_node = NULL;

	idev = ion_device_create(sunxi_ion_ioctl);
	if (IS_ERR_OR_NULL(idev))
		return PTR_ERR(idev);

	do {
		u32 type = -1;
		struct ion_heap *pheap = NULL;

		/*loop all the child node */
		heap_node = of_get_next_child(np, heap_node);
		if (!heap_node)
			break;
		memset(&heaps_desc, 0, sizeof(heaps_desc));

		/* get the properties "name","type" for common ion heap	*/
		if (of_property_read_u32(heap_node, "type", &type)) {
			pr_err("You need config the heap node 'type'\n");
			continue;
		}
		heaps_desc.type = type;
		heaps_desc.id = type;

		if (of_property_read_string(heap_node, "name",
					    &heaps_desc.name)) {
			pr_err("You need config the heap node 'name'\n");
			continue;
		}

		/*for specail heaps, need extra argument to config */
		switch (heaps_desc.type) {
		case ION_HEAP_TYPE_CARVEOUT:
		{
			u32 base = 0, size = 0;

			if (of_property_read_u32(heap_node, "base", &base))
				pr_err("You need config the carvout 'base'\n");
			heaps_desc.base = base;

			if (of_property_read_u32(heap_node, "size", &size))
				pr_err("You need config the carvout 'size'\n");
			heaps_desc.size = size;
			break;
		}
		case ION_HEAP_TYPE_DMA:
		{
			heaps_desc.priv = &pdev->dev;
			break;
		}
#ifdef CONFIG_TEE
		case ION_HEAP_TYPE_SECURE:
		{
			ulong tee_base;

			optee_probe_drm_configure(&heaps_desc.base,
				&heaps_desc.size, &tee_base);
			ion_sunxi_drm_phy_addr = heaps_desc.base;
			ion_sunxi_drm_tee_addr = tee_base;
			break;
		}
#endif
		default:
			continue;
		}

		/* now we can create a heap & add it to the ion device*/
		pheap = ion_heap_create(&heaps_desc);
		if (IS_ERR_OR_NULL(pheap)) {
			pr_err("ion_heap_create '%s' failed!\n", heaps_desc.name);
			continue;
		}

		ion_device_add_heap(idev, pheap);
	} while (1);

	return 0;
}

static const struct of_device_id sunxi_ion_dt_ids[] = {
	{ .compatible = "allwinner,sunxi-ion" },
	{ /* sentinel */ }
};

static struct platform_driver sunxi_ion_driver = {
	.driver = {
		.name = "sunxi-ion",
		.of_match_table = sunxi_ion_dt_ids,
	},
	.probe = sunxi_ion_probe,
};
module_platform_driver(sunxi_ion_driver);
