#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

// Creating the dev with our custom major and minor number.
// dev_t dev = MKDEV(229, 0);

// Automatic creation of device files can be handled with udev.
static dev_t dev_no = 0;
static struct class *dev_class;

// Kernel module arguments.
int hello_value, arr_hello_value[4];
char *hello_name;
int cb_hello_value = 0;

module_param(hello_value, int, S_IRUSR | S_IWUSR);                 // integer value
module_param(hello_name, charp, S_IRUSR | S_IWUSR);                // String
module_param_array(arr_hello_value, int, NULL, S_IRUSR | S_IWUSR); // Array of integers

/*----------------------Module_param_cb()--------------------------------*/
int notify_param(const char *val, const struct kernel_param *kp)
{
    int res = param_set_int(val, kp); // Use helper for write variable
    if (res == 0) {
        pr_info("Call back function called...\n");

        pr_info("New value of cb_hello_value = %d\n", cb_hello_value);
        return 0;
    }
    return -1;
}

const struct kernel_param_ops my_param_ops =
    {
        .set = &notify_param,  // Use our setter ...
        .get = &param_get_int, // .. and standard getter
};

module_param_cb(cb_hello_value, &my_param_ops, &cb_hello_value, S_IRUGO | S_IWUSR);

static int __init hello_init(void)
{
    /*
    Use kernel module arguments.
    */
    int i;
    pr_info("hello_value = %d  \n", hello_value);
    pr_info("cb_hello_value = %d  \n", cb_hello_value);
    pr_info("hello_name = %s \n", hello_name);
    for (i = 0; i < (sizeof arr_hello_value / sizeof(int)); i++) {
        pr_info("array_value[%d] = %d\n", i, arr_hello_value[i]);
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

    printk(KERN_INFO "Hello World enter\n");
    return 0;

r_device:
    class_destroy(dev_class);
r_class:
    unregister_chrdev_region(dev_no, 1);
    return -1;
}
module_init(hello_init);

static void __exit hello_exit(void)
{
    device_destroy(dev_class, dev_no);
    class_destroy(dev_class);
    unregister_chrdev_region(dev_no, 1);
    printk(KERN_INFO "Hello World exit\n");
}
module_exit(hello_exit);

MODULE_AUTHOR("zhonghuashu <77599567@qq.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A simple Hello World Module");
MODULE_ALIAS("a simplest module");
