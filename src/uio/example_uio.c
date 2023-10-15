/**
 * @file example_uio.c
 * @brief Example UIO driver from:
 * https://blog.csdn.net/weixin_38452632/article/details/130947993
 * @author zhonghuashu (77599567@qq.com)
 * @date 2023-09-30
 */
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uio_driver.h>
#include <linux/vmalloc.h>

#define DRV_NAME "uio_test"
#define MEM_SIZE 0x1000

static struct uio_info uio_test = {
    .name = "uio_device",
    .version = "0.0.1",
    .irq = UIO_IRQ_NONE,
};

static void uio_release(struct device *dev)
{
    struct uio_device *uio_dev = dev_get_drvdata(dev);
    uio_unregister_device(uio_dev->info);
    kfree(uio_dev);
}

static int uio_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct uio_info *info = filp->private_data;
    /*virt_to_page()将虚拟地址转换为一个指向相应页面描述符的指针，并使用page_to_pfn()获取该页面描述符对应的页框号*/
    unsigned long pfn = page_to_pfn(virt_to_page(info->mem[0].addr));
    /*PFN_PHYS()将页框号转换为相应的物理地址*/
    unsigned long phys = PFN_PHYS(pfn);
    /*uio_info结构体中第一个内存区域的大小*/
    unsigned long size = info->mem[0].size;

    if (remap_pfn_range(vma, vma->vm_start, phys >> PAGE_SHIFT, size, vma->vm_page_prot)) {
        return -EAGAIN;
    }

    return 0;
}

static const struct file_operations uio_fops = {
    .owner = THIS_MODULE,
    .mmap = uio_mmap,
};

char test_arr[PAGE_SIZE] = {0};

static ssize_t get_uio_info(struct device *dev, struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%s\n", test_arr);
}

static ssize_t set_uio_info(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
    snprintf(test_arr, PAGE_SIZE, "%s\n", buf);
    return count;
}

static DEVICE_ATTR(uio_info, 0600, get_uio_info, set_uio_info);

static struct attribute *uio_sysfs_attrs[] = {
    &dev_attr_uio_info.attr,
    NULL,
};

static struct attribute_group uio_attr_group = {
    .attrs = uio_sysfs_attrs,
};

static int uio_probe(struct platform_device *pdev)
{
    struct uio_device *uio_dev;
    int err;
    void *p;

    uio_dev = kzalloc(sizeof(struct uio_device), GFP_KERNEL);
    if (uio_dev == NULL) {
        return -ENOMEM;
    }

    p = kmalloc(MEM_SIZE, GFP_KERNEL);
    strcpy(p, "123456");
    uio_test.mem[0].name = "uio_mem",
    uio_test.mem[0].addr = (unsigned long)p;
    uio_test.mem[0].memtype = UIO_MEM_LOGICAL;
    uio_test.mem[0].size = MEM_SIZE;
    uio_dev->info = &uio_test;
    uio_dev->dev.parent = &pdev->dev;

    err = uio_register_device(&pdev->dev, uio_dev->info);
    if (err) {
        kfree(uio_dev);
        return err;
    }
    if (sysfs_create_group(&pdev->dev.kobj, &uio_attr_group)) {
        printk(KERN_ERR "Cannot create sysfs for system uio\n");
        return err;
    }

    // dev_set_drvdata(pdev, uio_dev);

    return 0;
}

static int uio_remove(struct platform_device *pdev)
{
    struct uio_device *uio_dev = platform_get_drvdata(pdev);

    sysfs_remove_group(&uio_dev->dev.kobj, &uio_attr_group);
    uio_unregister_device(uio_dev->info);
    // dev_set_drvdata(uio_dev, NULL);
    kfree(uio_dev);

    return 0;
}

static struct platform_device *uio_test_dev;
static struct platform_driver uio_driver = {
    .probe = uio_probe,
    .remove = uio_remove,
    .driver = {
        .name = DRV_NAME,
    },

};

static int __init uio_init(void)
{
    uio_test_dev = platform_device_register_simple(DRV_NAME, -1, NULL, 0);
    return platform_driver_register(&uio_driver);
}

static void __exit uio_exit(void)
{
    platform_device_unregister(uio_test_dev);
    platform_driver_unregister(&uio_driver);
}

module_init(uio_init);
module_exit(uio_exit);

MODULE_AUTHOR("zhonghuashu <77599567@qq.com>");
MODULE_LICENSE("GPL");