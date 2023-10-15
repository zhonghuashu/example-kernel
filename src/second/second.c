#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

// LINUX_WSL: device number 248 used by WSL, changed 248 -> 232
#define SECOND_MAJOR 232
#define LINUX_WSL

static int second_major = SECOND_MAJOR;
module_param(second_major, int, S_IRUGO);

struct second_dev {
    struct cdev cdev;
    atomic_t counter;
    struct timer_list s_timer;
};

static struct second_dev *second_devp;

// LINUX_WSL: unsigned long arg => struct timer_list * arg
static void second_timer_handler(struct timer_list *arg)
{
    // Trigger next timeout.
    mod_timer(&second_devp->s_timer, jiffies + HZ);
    // Increase second number.
    atomic_inc(&second_devp->counter);

    printk(KERN_INFO "current jiffies is %ld\n", jiffies);
}

static int second_open(struct inode *inode, struct file *filp)
{
#ifdef LINUX_WSL
    // Setup your timer to call callback handler.
    timer_setup(&second_devp->s_timer, second_timer_handler, 0);
    // Modify a timer's timeout interval to 1 second (1 HZ = msecs_to_jiffies(1000)).
    mod_timer(&second_devp->s_timer, jiffies + HZ);
#else
    init_timer(&second_devp->s_timer);
    second_devp->s_timer.function = &second_timer_handler;
    second_devp->s_timer.expires = jiffies + HZ;

    add_timer(&second_devp->s_timer);
#endif
    // Intialize second to zero.
    atomic_set(&second_devp->counter, 0);

    return 0;
}

static int second_release(struct inode *inode, struct file *filp)
{
    del_timer(&second_devp->s_timer);

    return 0;
}

static ssize_t second_read(struct file *filp, char __user *buf, size_t count,
                           loff_t *ppos)
{
    int counter;

    counter = atomic_read(&second_devp->counter);
    // Copy simple variable counter to user space.
    if (put_user(counter, (int *)buf))
        return -EFAULT;
    else
        return sizeof(unsigned int);
}

static const struct file_operations second_fops = {
    .owner = THIS_MODULE,
    .open = second_open,
    .release = second_release,
    .read = second_read,
};

static void second_setup_cdev(struct second_dev *dev, int index)
{
    int err, devno = MKDEV(second_major, index);

    cdev_init(&dev->cdev, &second_fops);
    dev->cdev.owner = THIS_MODULE;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
        printk(KERN_ERR "Failed to add second device\n");
}

static int __init second_init(void)
{
    int ret;
    dev_t devno = MKDEV(second_major, 0);

    if (second_major)
        ret = register_chrdev_region(devno, 1, "second");
    else {
        ret = alloc_chrdev_region(&devno, 0, 1, "second");
        second_major = MAJOR(devno);
    }
    if (ret < 0)
        return ret;

    second_devp = kzalloc(sizeof(*second_devp), GFP_KERNEL);
    if (!second_devp) {
        ret = -ENOMEM;
        goto fail_malloc;
    }

    second_setup_cdev(second_devp, 0);
    printk(KERN_INFO "Initialize second device");
    return 0;

fail_malloc:
    unregister_chrdev_region(devno, 1);
    return ret;
}
module_init(second_init);

static void __exit second_exit(void)
{
    cdev_del(&second_devp->cdev);
    kfree(second_devp);
    unregister_chrdev_region(MKDEV(second_major, 0), 1);
}
module_exit(second_exit);

MODULE_AUTHOR("zhonghuashu <77599567@qq.com>");
MODULE_LICENSE("GPL");
