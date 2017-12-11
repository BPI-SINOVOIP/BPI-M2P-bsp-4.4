#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>

static struct class *sunxi_class;
static struct device *sunxi_sram_dev;

struct sram_dev {
	struct device *dev;
	size_t size;
	void __iomem *virbase;
	struct resource *res;
	struct cdev cdev;
	dev_t devid;
};

static int sunxi_sram_open(struct inode *inode, struct file *file)
{
	struct sram_dev *devp;

	devp = container_of(inode->i_cdev, struct sram_dev, cdev);
	file->private_data = devp;

	return 0;
}

static int sunxi_sram_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static int sunxi_sram_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct sram_dev *devp;
	unsigned long size = vma->vm_end - vma->vm_start;

	devp = (struct sram_dev *)filp->private_data;

	if (size > devp->size) {
		pr_err("mmap size is bigger than sram size\n");
		return -ENXIO;
	}

	vma->vm_flags |= VM_IO;
	vma->vm_flags |= VM_LOCKED;
	if (remap_pfn_range(vma, vma->vm_start,
		virt_to_phys(devp->virbase) >> PAGE_SHIFT,
			size, vma->vm_page_prot)) {
		pr_err("remap page range failed\n");
		return -ENXIO;
	}

	return 0;
}

static const struct file_operations sunxi_sram_fops = {
	.owner = THIS_MODULE,
	.open = sunxi_sram_open,
	.release = sunxi_sram_close,
	.mmap = sunxi_sram_mmap,
};

static int sunxi_sram_probe(struct platform_device *pdev)
{
	struct sram_dev *sram_dev;
	struct resource *res;
	int ret;
	dev_t devid;

	sram_dev = devm_kzalloc(&pdev->dev, sizeof(*sram_dev), GFP_KERNEL);
	if (!sram_dev)
		return -ENOMEM;

	sram_dev->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(sram_dev->dev, "found no memory resource\n");
		ret = -EINVAL;
		goto err_get_resource;
	}

	sram_dev->size = resource_size(res);
	sram_dev->res = res;

	sram_dev->virbase = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(sram_dev->virbase)) {
		ret = PTR_ERR(sram_dev->virbase);
		goto err_get_resource;
	}


	ret = alloc_chrdev_region(&devid, 0, 1, "sunxi_sram_a1");
	if (ret < 0) {
		dev_err(&pdev->dev,
				"sunxi_sram_a1 chrdev_region err: %d\n", ret);
		goto err_chr_reg;
	}

	sram_dev->devid = devid;
	cdev_init(&sram_dev->cdev, &sunxi_sram_fops);
	sram_dev->cdev.owner = THIS_MODULE;
	ret = cdev_add(&sram_dev->cdev, devid, 1);
	if (ret)
		goto err_cdev;

	sunxi_class = class_create(THIS_MODULE, "sram");
	if (IS_ERR(sunxi_class)) {
		pr_err("Error creating sram class.\n");
		ret = PTR_ERR(sunxi_class);
		goto err_crete_c;
	}

	sunxi_sram_dev =
		device_create(sunxi_class, NULL, devid, NULL, "sram_a1");
	if (IS_ERR(sunxi_sram_dev)) {
		pr_err("Error creating sram device.\n");
		ret = PTR_ERR(sunxi_sram_dev);
		goto err_dev_create;
	}

	platform_set_drvdata(pdev, sram_dev);

	return 0;

err_dev_create:
	class_destroy(sunxi_class);
err_crete_c:
	cdev_del(&sram_dev->cdev);
err_cdev:
	unregister_chrdev_region(devid, 1);
err_chr_reg:
	devm_iounmap(&pdev->dev, sram_dev->virbase);
	devm_release_mem_region(&pdev->dev, res->start, sram_dev->size);
err_get_resource:
	devm_kfree(&pdev->dev, sram_dev);
	return ret;
}

static int sunxi_sram_remove(struct platform_device *pdev)
{
	struct sram_dev *sram_dev;

	sram_dev = platform_get_drvdata(pdev);
	device_destroy(sunxi_class, sram_dev->devid);
	class_destroy(sunxi_class);
	cdev_del(&sram_dev->cdev);
	unregister_chrdev_region(sram_dev->devid, 1);
	devm_iounmap(&pdev->dev, sram_dev->virbase);
	devm_release_mem_region(&pdev->dev,
			sram_dev->res->start, sram_dev->size);
	devm_kfree(&pdev->dev, sram_dev);

	return 0;
}

static const struct of_device_id sram_dt_ids[] = {
	{ .compatible = "allwinner,sram_a1" },
	{}
};

static struct platform_driver sram_driver = {
	.driver = {
		.name = "sram",
		.of_match_table = of_match_ptr(sram_dt_ids),
	},
	.probe = sunxi_sram_probe,
	.remove = sunxi_sram_remove,
};

static int __init sram_init(void)
{
	return platform_driver_register(&sram_driver);
}

static void __exit sram_exit(void)
{
	return platform_driver_unregister(&sram_driver);
}

postcore_initcall(sram_init);
module_exit(sram_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhuxianbin");
