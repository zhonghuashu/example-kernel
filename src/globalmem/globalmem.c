/*
 * a simple char device driver: globalmem mutex
 */

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/err.h>

#define GLOBALMEM_SIZE 0x1000
#define GLOBALMEM_MAGIC 'g'

// An ioctl with write parameters (copy_from_user)
#define MEM_CLEAR _IOW(GLOBALMEM_MAGIC, 0x01, int32_t *)

// Automatic creation of device files can be handled with udev.
static dev_t dev_no = 0;
static struct class *dev_class;

/*************** Kernel module arguments **********************/
static int notify_param(const char *val, const struct kernel_param *kp);
int globalmem_value = 0;
int globalmem_arr_value[4];
char *globalmem_hello_name;
int globalmem_cb_value = 0;

module_param(globalmem_value, int, S_IRUSR | S_IWUSR);                 // integer value
module_param(globalmem_hello_name, charp, S_IRUSR | S_IWUSR);          // String
module_param_array(globalmem_arr_value, int, NULL, S_IRUSR | S_IWUSR); // Array of integers

const struct kernel_param_ops my_param_ops = {
    .set = &notify_param,  // Use our setter ...
    .get = &param_get_int, // .. and standard getter
};

module_param_cb(globalmem_cb_value, &my_param_ops, &globalmem_cb_value, S_IRUGO | S_IWUSR);

/*************** device struct**********************/
// Common character device struct and encapsulated memory buffer mem[]
struct globalmem_dev {
    struct cdev cdev;
    unsigned char mem[GLOBALMEM_SIZE];
    /*
    Concurrent access control using mutex.
    - spin lock can't use because copy_xxx_user might block the process.
    - mutex used for demo purpose, not all places are added.
    */
    struct mutex mutex;
};

struct globalmem_dev *globalmem_devp;

/*************** Driver Functions **********************/
static int globalmem_open(struct inode *inode, struct file *filp);
static int globalmem_release(struct inode *inode, struct file *filp);
static long globalmem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static ssize_t globalmem_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos);
static ssize_t globalmem_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos);
static loff_t globalmem_llseek(struct file *filp, loff_t offset, int orig);

static const struct file_operations globalmem_fops = {
    .owner = THIS_MODULE,
    .llseek = globalmem_llseek, // Change current position.
    .read = globalmem_read,
    .write = globalmem_write,
    .unlocked_ioctl = globalmem_ioctl, // Realize device control command, map to user space fcntl() / ioctl().
    .open = globalmem_open,
    .release = globalmem_release};

/***************** Procfs *******************/
char globalmem_proc_array[20] = "try proc array\n";
int globalmem_proc_len = 1;
static struct proc_dir_entry *proc_parent;
static int open_proc(struct inode *inode, struct file *file);
static int release_proc(struct inode *inode, struct file *file);
static ssize_t read_proc(struct file *filp, char __user *buffer, size_t length, loff_t *offset);
static ssize_t write_proc(struct file *filp, const char *buff, size_t len, loff_t *off);

static struct proc_ops proc_fops = {
    .proc_open = open_proc,
    .proc_read = read_proc,
    .proc_write = write_proc,
    .proc_release = release_proc};

/*************** Sysfs **********************/
static ssize_t sysfs_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t sysfs_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
struct kobject *kobj_ref;
volatile int sysfs_value = 0;
struct kobj_attribute globalmem_attr = __ATTR(sysfs_value, 0660, sysfs_show, sysfs_store);

/***************** Wait queue *******************/
wait_queue_head_t wait_queue_exit;
int wait_queue_flag = 0;
static struct task_struct *wait_thread;

/***************** Interrupt *******************/
// Interrupt Request number.
#define IRQ_NO 11
// Interrupt handler for IRQ 11.
static irqreturn_t irq_handler(int irq, void *dev_id)
{
    pr_info("Shared IRQ: Interrupt Occurred");
    return IRQ_HANDLED;
}

/**
 * This function will be called when we read the sysfs file.
 */
static ssize_t sysfs_show(struct kobject *kobj,
                          struct kobj_attribute *attr, char *buf)
{
    pr_info("Sysfs - Read!!!\n");
    pr_info("Raise interrupt IRQ 11\n");
    asm("int $0x3B");  // Corresponding to irq 11

    return sprintf(buf, "%d", sysfs_value);
}

/**
 * This function will be called when we write the sysfsfs file.
 */
static ssize_t sysfs_store(struct kobject *kobj,
                           struct kobj_attribute *attr, const char *buf, size_t count)
{
    pr_info("Sysfs - Write!!!\n");
    sscanf(buf, "%d", &sysfs_value);
    return count;
}

/**
 * Thread function.
 */
static int wait_function(void *unused)
{

    while (1) {
        pr_info("Waiting For Event...\n");
        wait_event_interruptible(wait_queue_exit, wait_queue_flag != 0);
        if (wait_queue_flag == 2) {
            pr_info("Event Came From Exit Function\n");
            return 0;
        } else {
            pr_info("Event Came From %d\n", wait_queue_flag);
        }
        wait_queue_flag = 0;
    }
    return 0;
}

/**
 * Module_param_cb
 */
static int notify_param(const char *val, const struct kernel_param *kp)
{
    int res = param_set_int(val, kp); // Use helper for write variable
    if (res == 0) {
        pr_info("Call back function called...\n");

        pr_info("New value of globalmem_cb_value = %d\n", globalmem_cb_value);
        return 0;
    }
    return -1;
}

/**
 * This function will be called when we open the procfs file.
 */
static int open_proc(struct inode *inode, struct file *file)
{
    pr_info("proc file opend.....\t");
    return 0;
}

/**
 * This function will be called when we close the procfs file.
 */
static int release_proc(struct inode *inode, struct file *file)
{
    pr_info("proc file released.....\n");
    return 0;
}

/**
 * This function will be called when we read the procfs file.
 */
static ssize_t read_proc(struct file *filp, char __user *buffer, size_t length, loff_t *offset)
{
    pr_info("proc file read.....\n");

    // Read / copy only once.
    if (globalmem_proc_len)
        globalmem_proc_len = 0;
    else {
        globalmem_proc_len = 1;
        return 0;
    }

    if (copy_to_user(buffer, globalmem_proc_array, 20)) {
        pr_err("Data Send : Err!\n");
    }

    return length;
}

/**
 * This function will be called when we write the procfs file.
 */
static ssize_t write_proc(struct file *filp, const char *buff, size_t len, loff_t *off)
{
    pr_info("proc file wrote.....\n");

    if (copy_from_user(globalmem_proc_array, buff, len)) {
        pr_err("Data Write : Err!\n");
    }

    return len;
}

static int globalmem_open(struct inode *inode, struct file *filp)
{
    // Pointer private_data to device struct.
    filp->private_data = globalmem_devp;
    return 0;
}

static int globalmem_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static long globalmem_ioctl(struct file *filp, unsigned int cmd,
                            unsigned long arg)
{
    // Fetch device instance pointer via private_data member.
    struct globalmem_dev *dev = filp->private_data;

    switch (cmd) {
    case MEM_CLEAR: // Clear memory buffer to zero.
        memset(dev->mem, 0, GLOBALMEM_SIZE);
        printk(KERN_INFO "globalmem is set to zero\n");
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

/**
 * Read / write mem[] buffer with user space, change file read position accordingly.
 */
static ssize_t globalmem_read(struct file *filp, char __user *buf, size_t size,
                              loff_t *ppos)
{
    unsigned long p = *ppos;
    unsigned int count = size;
    int ret = 0;
    struct globalmem_dev *dev = filp->private_data;

    if (p >= GLOBALMEM_SIZE)
        return 0;
    if (count > GLOBALMEM_SIZE - p)
        count = GLOBALMEM_SIZE - p;

    // Lock before read shared memory buffer.
    mutex_lock(&dev->mutex);
    // Check if valid user space address: copy_to_user(void __user *to, const void *from, unsigned long count)
    if (copy_to_user(buf, dev->mem + p, count)) {
        ret = -EFAULT;
    } else {
        *ppos += count;
        ret = count;

        printk(KERN_INFO "read %u bytes(s) from %lu\n", count, p);
    }
    mutex_unlock(&dev->mutex);

    return ret;
}

static ssize_t globalmem_write(struct file *filp, const char __user *buf,
                               size_t size, loff_t *ppos)
{
    unsigned long p = *ppos;
    unsigned int count = size;
    int ret = 0;
    struct globalmem_dev *dev = filp->private_data;

    if (p >= GLOBALMEM_SIZE)
        return 0;
    if (count > GLOBALMEM_SIZE - p)
        count = GLOBALMEM_SIZE - p;

    // Lock before write shared memory buffer.
    mutex_lock(&dev->mutex);
    if (copy_from_user(dev->mem + p, buf, count))
        ret = -EFAULT;
    else {
        *ppos += count;
        ret = count;

        printk(KERN_INFO "written %u bytes(s) from %lu\n", count, p);
    }
    mutex_unlock(&dev->mutex);

    return ret;
}

static loff_t globalmem_llseek(struct file *filp, loff_t offset, int orig)
{
    loff_t ret = 0;
    switch (orig) {
    case 0: // SEEK_SET: seek from beginning
        if (offset < 0) {
            ret = -EINVAL;
            break;
        }
        if ((unsigned int)offset > GLOBALMEM_SIZE) {
            ret = -EINVAL;
            break;
        }
        filp->f_pos = (unsigned int)offset;
        ret = filp->f_pos;
        break;
    case 1: // SEEK_CUR: seek from current
        if ((filp->f_pos + offset) > GLOBALMEM_SIZE) {
            ret = -EINVAL;
            break;
        }
        if ((filp->f_pos + offset) < 0) {
            ret = -EINVAL;
            break;
        }
        filp->f_pos += offset;
        ret = filp->f_pos;
        break;
    default:
        ret = -EINVAL;
        break;
    }
    return ret;
}

/**
 * Initialize and add cdev to system.
 */
static void globalmem_setup_cdev(struct globalmem_dev *dev, int index)
{
    int err;
    // Initialize cdev members and set file operation callback functions.
    cdev_init(&dev->cdev, &globalmem_fops);
    dev->cdev.owner = THIS_MODULE;
    // Add a cdev to system.
    err = cdev_add(&dev->cdev, dev_no, 1);
    if (err)
        printk(KERN_NOTICE "Error %d adding globalmem%d", err, index);
}

/**
 * Device driver install function.
 */
static int __init globalmem_init(void)
{
    int ret;

    /*
    Use kernel module arguments.
    */
    pr_info("globalmem_value = %d  \n", globalmem_value);
    pr_info("globalmem_cb_value = %d  \n", globalmem_cb_value);
    pr_info("globalmem_hello_name = %s \n", globalmem_hello_name);
    for (int i = 0; i < (sizeof globalmem_arr_value / sizeof(int)); i++) {
        pr_info("array_value[%d] = %d\n", i, globalmem_arr_value[i]);
    }

    /*
    Automatic create device file.
    */
    // Allocating Major number, see /proc/devices
    if ((alloc_chrdev_region(&dev_no, 0, 1, "globalmem")) < 0) {
        pr_err("Cannot allocate major number for device\n");
        return -1;
    }
    pr_info("Major = %d Minor = %d \n", MAJOR(dev_no), MINOR(dev_no));

    // Creating struct class structure under/sys/class/xxx.
    dev_class = class_create(THIS_MODULE, "globalmem");
    if (IS_ERR(dev_class)) {
        pr_err("Cannot create the struct class for device\n");
        goto r_class;
    }

    // Creating device, A “dev” file will be created: /dev/xxx
    if (IS_ERR(device_create(dev_class, NULL, dev_no, NULL, "globalmem"))) {
        pr_err("Cannot create the Device\n");
        goto r_device;
    }

    // Create proc directory. It will create a directory under "/proc".
    proc_parent = proc_mkdir("example", NULL);
    if (proc_parent == NULL) {
        pr_info("Error creating proc entry");
        goto r_device;
    }
    // Creating Proc entry under "/proc/globalmem/".
    proc_create("globalmem", 0666, proc_parent, &proc_fops);

    // Initialize wait queue.
    init_waitqueue_head(&wait_queue_exit);

    // Creating a directory in /sys/kernel/
    kobj_ref = kobject_create_and_add("example_sysfs", kernel_kobj);

    // Creating sysfs file for globalmem_value.
    if (sysfs_create_file(kobj_ref, &globalmem_attr.attr)) {
        pr_err("Cannot create sysfs file......\n");
        goto r_sysfs;
    }

    // Create the kernel thread with name 'mythread'
    wait_thread = kthread_create(wait_function, NULL, "WaitThread");
    if (wait_thread) {
        pr_info("Thread Created successfully\n");
        wake_up_process(wait_thread);
    } else
        pr_info("Thread creation failed\n");

    // Register an interrupt handler.
    if (request_irq(IRQ_NO, irq_handler, IRQF_SHARED, "globalmem", (void *)(irq_handler))) {
        pr_err("my_device: cannot register IRQ ");
        goto irq;
    }

    globalmem_devp = kzalloc(sizeof(struct globalmem_dev), GFP_KERNEL);
    if (!globalmem_devp) {
        ret = -ENOMEM;
        goto fail_malloc;
    }

    globalmem_setup_cdev(globalmem_devp, 0);
    mutex_init(&globalmem_devp->mutex);

    pr_info("Kernel module inserted successfully...\n");
    return 0;

irq:
    free_irq(IRQ_NO,(void *)(irq_handler));
r_sysfs:
    kobject_put(kobj_ref);
    sysfs_remove_file(kernel_kobj, &globalmem_attr.attr);
r_device:
    class_destroy(dev_class);
r_class:
    unregister_chrdev_region(dev_no, 1);
    return -1;

fail_malloc:
    unregister_chrdev_region(dev_no, 1);
    return ret;
}
module_init(globalmem_init);

/**
 * Device driver uninstall function.
 */
static void __exit globalmem_exit(void)
{
    free_irq(IRQ_NO, (void *)(irq_handler));

    kobject_put(kobj_ref);
    sysfs_remove_file(kernel_kobj, &globalmem_attr.attr);

    // Wakeup wait queue before exit driver.
    wait_queue_flag = 2;
    wake_up_interruptible(&wait_queue_exit);

    // Remove complete /proc/example-kernel.
    proc_remove(proc_parent);
    device_destroy(dev_class, dev_no);
    class_destroy(dev_class);
    // Remove cdev from system.
    cdev_del(&globalmem_devp->cdev);
    kfree(globalmem_devp);
    // Free allocated device number.
    unregister_chrdev_region(dev_no, 1);

    pr_info("Kernel module removed successfully...\n");
}
module_exit(globalmem_exit);

MODULE_AUTHOR("zhonghuashu <77599567@qq.com>");
MODULE_LICENSE("GPL v2");
