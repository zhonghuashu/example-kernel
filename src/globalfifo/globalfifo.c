/*
 * a simple char device driver: globalfifo
 * - Block read until FIFO not empty.
 * - Block write until FIFO not full.
 * - Send async SIGIO signal after writing data to FIFO.
 */

#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>

#define GLOBALFIFO_SIZE 0x1000
#define FIFO_CLEAR 0x1
#define GLOBALFIFO_MAJOR 231

static int globalfifo_major = GLOBALFIFO_MAJOR;
module_param(globalfifo_major, int, S_IRUGO);

struct globalfifo_dev {
    struct cdev cdev;
    unsigned int current_len;
    unsigned char mem[GLOBALFIFO_SIZE];
    struct mutex mutex;                // Access protect to critical area FIFO and wait queue.
    wait_queue_head_t r_wait;          // Wait queue to wakeup blocking read process.
    wait_queue_head_t w_wait;          // Wait queue to wakeup blocking write process.
    struct fasync_struct *async_queue; // Async access I/O queue.
};

struct globalfifo_dev *globalfifo_devp;

static int globalfifo_fasync(int fd, struct file *filp, int mode)
{
    // Invoked when FASYNC flag set via F_SETFL.
    struct globalfifo_dev *dev = filp->private_data;
    return fasync_helper(fd, filp, mode, &dev->async_queue);
}

static int globalfifo_open(struct inode *inode, struct file *filp)
{
    filp->private_data = globalfifo_devp;
    return 0;
}

static int globalfifo_release(struct inode *inode, struct file *filp)
{
    // Remove file from async notify table.
    globalfifo_fasync(-1, filp, 0);
    return 0;
}

static long globalfifo_ioctl(struct file *filp, unsigned int cmd,
                             unsigned long arg)
{
    struct globalfifo_dev *dev = filp->private_data;

    switch (cmd) {
    case FIFO_CLEAR:
        mutex_lock(&dev->mutex);
        dev->current_len = 0;
        memset(dev->mem, 0, GLOBALFIFO_SIZE);
        mutex_unlock(&dev->mutex);

        printk(KERN_INFO "globalfifo is set to zero\n");
        break;

    default:
        return -EINVAL;
    }
    return 0;
}

static unsigned int globalfifo_poll(struct file *filp, poll_table *wait)
{
    unsigned int mask = 0;
    struct globalfifo_dev *dev = filp->private_data;

    mutex_lock(&dev->mutex);

    // Add read / write wait queue into poll_table.
    poll_wait(filp, &dev->r_wait, wait);
    poll_wait(filp, &dev->w_wait, wait);

    // Mark data can be read (not Empty).
    if (dev->current_len != 0) {
        mask |= POLLIN | POLLRDNORM;
    }

    // Mark data can be write (not FULL)
    if (dev->current_len != GLOBALFIFO_SIZE) {
        mask |= POLLOUT | POLLWRNORM;
    }

    mutex_unlock(&dev->mutex);
    return mask;
}

static ssize_t globalfifo_read(struct file *filp, char __user *buf,
                               size_t count, loff_t *ppos)
{
    int ret;
    struct globalfifo_dev *dev = filp->private_data;
    // Declare wait queue.
    DECLARE_WAITQUEUE(wait, current);

    mutex_lock(&dev->mutex);
    // Add current task struct to wait queue.
    add_wait_queue(&dev->r_wait, &wait);

    // Check if possible to read.
    while (dev->current_len == 0) {
        // If non-block flag set, return error Try Again.
        if (filp->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            goto out;
        }
        // Change process state to INTERRUPTIBLE (light sleep) and could be wakeup by signal.
        __set_current_state(TASK_INTERRUPTIBLE);
        mutex_unlock(&dev->mutex);

        // Switch to other process running.
        schedule();
        if (signal_pending(current)) {
            // Return Restart error if wakeup by signal.
            ret = -ERESTARTSYS;
            goto out2;
        }

        mutex_lock(&dev->mutex);
    }

    if (count > dev->current_len)
        count = dev->current_len;

    if (copy_to_user(buf, dev->mem, count)) {
        ret = -EFAULT;
        goto out;
    } else {
        memcpy(dev->mem, dev->mem + count, dev->current_len - count);
        dev->current_len -= count;
        printk(KERN_INFO "read %d bytes(s),current_len:%d\n", count,
               dev->current_len);

        // Wakeup possible write blocking process after read out data.
        wake_up_interruptible(&dev->w_wait);

        ret = count;
    }
out:
    mutex_unlock(&dev->mutex);
out2:
    // Remove from wait queue.
    remove_wait_queue(&dev->r_wait, &wait);
    // Set process tate to RUNNING.
    set_current_state(TASK_RUNNING);
    return ret;
}

static ssize_t globalfifo_write(struct file *filp, const char __user *buf,
                                size_t count, loff_t *ppos)
{
    struct globalfifo_dev *dev = filp->private_data;
    int ret;
    DECLARE_WAITQUEUE(wait, current);

    mutex_lock(&dev->mutex);
    add_wait_queue(&dev->w_wait, &wait);

    while (dev->current_len == GLOBALFIFO_SIZE) {
        if (filp->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            goto out;
        }
        __set_current_state(TASK_INTERRUPTIBLE);

        mutex_unlock(&dev->mutex);

        schedule();
        if (signal_pending(current)) {
            ret = -ERESTARTSYS;
            goto out2;
        }

        mutex_lock(&dev->mutex);
    }

    if (count > GLOBALFIFO_SIZE - dev->current_len)
        count = GLOBALFIFO_SIZE - dev->current_len;

    if (copy_from_user(dev->mem + dev->current_len, buf, count)) {
        ret = -EFAULT;
        goto out;
    } else {
        dev->current_len += count;
        printk(KERN_INFO "written %d bytes(s),current_len:%d\n", count,
               dev->current_len);

        wake_up_interruptible(&dev->r_wait);

        // Send SIGIO signal to inform data can be readout.
        if (dev->async_queue) {
            kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
            printk(KERN_DEBUG "%s kill SIGIO\n", __func__);
        }

        ret = count;
    }

out:
    mutex_unlock(&dev->mutex);
out2:
    remove_wait_queue(&dev->w_wait, &wait);
    set_current_state(TASK_RUNNING);
    return ret;
}

static const struct file_operations globalfifo_fops = {
    .owner = THIS_MODULE,
    .read = globalfifo_read,
    .write = globalfifo_write,
    .unlocked_ioctl = globalfifo_ioctl,
    .poll = globalfifo_poll,
    .fasync = globalfifo_fasync,
    .open = globalfifo_open,
    .release = globalfifo_release,
};

static void globalfifo_setup_cdev(struct globalfifo_dev *dev, int index)
{
    int err, devno = MKDEV(globalfifo_major, index);

    cdev_init(&dev->cdev, &globalfifo_fops);
    dev->cdev.owner = THIS_MODULE;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
        printk(KERN_NOTICE "Error %d adding globalfifo%d", err, index);
}

static int __init globalfifo_init(void)
{
    int ret;
    dev_t devno = MKDEV(globalfifo_major, 0);

    if (globalfifo_major)
        ret = register_chrdev_region(devno, 1, "globalfifo");
    else {
        ret = alloc_chrdev_region(&devno, 0, 1, "globalfifo");
        globalfifo_major = MAJOR(devno);
    }
    if (ret < 0)
        return ret;

    globalfifo_devp = kzalloc(sizeof(struct globalfifo_dev), GFP_KERNEL);
    if (!globalfifo_devp) {
        ret = -ENOMEM;
        goto fail_malloc;
    }

    globalfifo_setup_cdev(globalfifo_devp, 0);

    mutex_init(&globalfifo_devp->mutex);
    init_waitqueue_head(&globalfifo_devp->r_wait);
    init_waitqueue_head(&globalfifo_devp->w_wait);

    return 0;

fail_malloc:
    unregister_chrdev_region(devno, 1);
    return ret;
}
module_init(globalfifo_init);

static void __exit globalfifo_exit(void)
{
    cdev_del(&globalfifo_devp->cdev);
    kfree(globalfifo_devp);
    unregister_chrdev_region(MKDEV(globalfifo_major, 0), 1);
}
module_exit(globalfifo_exit);

MODULE_AUTHOR("zhonghuashu <77599567@qq.com>");
MODULE_LICENSE("GPL");