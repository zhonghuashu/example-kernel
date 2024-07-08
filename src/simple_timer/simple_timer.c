#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

/***************** kernel timer *******************/
#define TIMEOUT 10    // milliseconds
static struct timer_list simple_timer;
static unsigned int timer_counter = 0;
void timer_callback(struct timer_list * data);

/***************** htrimer *******************/
#define TIMEOUT_NSEC   ( TIMEOUT * 1000 * 1000L )      // nano seconds
#define TIMEOUT_SEC    ( 0 )                // seconds
static struct hrtimer simple_timer_hr_timer;
static unsigned int hrtimer_counter = 0;
enum hrtimer_restart hr_timer_callback(struct hrtimer *timer);

/*************** Sysfs **********************/
static ssize_t sysfs_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t sysfs_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
struct kobject *kobj_ref;
struct kobj_attribute simple_timer_attr = __ATTR(sysfs_value, 0660, sysfs_show, sysfs_store);


// Creating the dev with our custom major and minor number.
// dev_t dev = MKDEV(229, 0);

// Automatic creation of device files can be handled with udev.
static dev_t dev_no = 0;
static struct class *dev_class;

/**
 * This function will be called when we read the sysfs file.
 */
static ssize_t sysfs_show(struct kobject *kobj,
                          struct kobj_attribute *attr, char *buf)
{
    pr_info("Sysfs - read\n");

    return sprintf(buf, "jiffies timer: %d\n, hr timer: %d", timer_counter, hrtimer_counter);
}

/**
 * This function will be called when we write the sysfsfs file.
 */
static ssize_t sysfs_store(struct kobject *kobj,
                           struct kobj_attribute *attr, const char *buf, size_t count)
{
    pr_info("Sysfs - write %s\n", buf);

    return 0;
}

/**
 * Timer Callback function. This will be called when timer expires.
 */
void timer_callback(struct timer_list * data)
{
    /* do your timer stuff here */
    if (++timer_counter == 1) {
        pr_info("Kernel timer callback function called [%d] / [%d]\n", timer_counter, (int)msecs_to_jiffies(TIMEOUT));
        pr_info("mses_to_jiff: 10ms = %d, 20ms = %d, 1000ms = %d, TIMEOUT = %d",
            (int)msecs_to_jiffies(10),
            (int)msecs_to_jiffies(20),
            (int)msecs_to_jiffies(1000),
            (int)msecs_to_jiffies(TIMEOUT));
    }

    /*
       Re-enable timer. Because this function will be called only first time.
       If we re-enable this will work like periodic timer.
    */
    mod_timer(&simple_timer, jiffies + msecs_to_jiffies(TIMEOUT) - 1);
}

/**
 * Timer Callback function. This will be called when timer expires
 */
enum hrtimer_restart hr_timer_callback(struct hrtimer *timer)
{
    /* do your timer stuff here */
    if (++hrtimer_counter == 1) {
        pr_info("hrtimer callback function called [%d]\n", hrtimer_counter++);
    }

    hrtimer_forward_now(timer, ktime_set(TIMEOUT_SEC, TIMEOUT_NSEC));

    return HRTIMER_RESTART;
}


static int __init simple_timer_init(void)
{
    ktime_t ktime;

    // Setup your timer to call my_timer_callback.
    // If you face some issues and using older kernel version, then you can try setup_timer API(Change Callback function's argument to unsingned long instead of struct timer_list *.
    timer_setup(&simple_timer, timer_callback, 0);
    // Setup timer interval to based on TIMEOUT Macro.
    mod_timer(&simple_timer, jiffies + msecs_to_jiffies(TIMEOUT));

    // Setup hrtimer to call my_timer_callback.
    ktime = ktime_set(TIMEOUT_SEC, TIMEOUT_NSEC);
    hrtimer_init(&simple_timer_hr_timer, CLOCK_MONOTONIC /* Always to move forward in time */, HRTIMER_MODE_REL /* Relative */);
    simple_timer_hr_timer.function = &hr_timer_callback;
    hrtimer_start( &simple_timer_hr_timer, ktime, HRTIMER_MODE_REL);

    // Creating a directory in /sys/kernel/
    kobj_ref = kobject_create_and_add("example_sysfs", kernel_kobj);

    // Creating sysfs file for globalmem_value.
    if (sysfs_create_file(kobj_ref, &simple_timer_attr.attr)) {
        pr_err("Cannot create sysfs file......\n");
        goto r_sysfs;
    }

    /*
    Automatic create device file.
    */
    // Allocating Major number, see /proc/devices
    if ((alloc_chrdev_region(&dev_no, 0, 1, "hello")) < 0) {
        pr_err("Cannot allocate major number for device\n");
        return -1;
    }
    pr_info("Major = %d Minor = %d \n", MAJOR(dev_no), MINOR(dev_no));

    // Creating struct class structure under/sys/class/hello.
    dev_class = class_create(THIS_MODULE, "hello");
    if (IS_ERR(dev_class)) {
        pr_err("Cannot create the struct class for device\n");
        goto r_class;
    }

    // Creating device, A “dev” file will be created: /dev/hello
    if (IS_ERR(device_create(dev_class, NULL, dev_no, NULL, "hello"))) {
        pr_err("Cannot create the Device\n");
        goto r_device;
    }

    printk(KERN_INFO "Simple timer enter\n");
    return 0;

r_sysfs:
    kobject_put(kobj_ref);
    sysfs_remove_file(kernel_kobj, &simple_timer_attr.attr);
r_device:
    class_destroy(dev_class);
r_class:
    unregister_chrdev_region(dev_no, 1);
    return -1;
}
module_init(simple_timer_init);

static void __exit simple_timer_exit(void)
{
    kobject_put(kobj_ref);
    sysfs_remove_file(kernel_kobj, &simple_timer_attr.attr);

    del_timer(&simple_timer);
    hrtimer_cancel(&simple_timer_hr_timer);

    device_destroy(dev_class, dev_no);
    class_destroy(dev_class);
    unregister_chrdev_region(dev_no, 1);
    printk(KERN_INFO "Simple timer exit\n");
}
module_exit(simple_timer_exit);

MODULE_AUTHOR("zhonghuashu <77599567@qq.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A simple Hello World Module");
MODULE_ALIAS("a simplest module");
