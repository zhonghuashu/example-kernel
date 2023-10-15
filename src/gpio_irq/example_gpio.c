/**
 * @file example_gpio.c
 * @brief A example GPIO interrupt driver.
 *        Test with Raspberry Pi 3B+ board.
 * @author zhonghuashu (77599567@qq.com)
 * @date 2023-09-24
 */
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#define EN_DEBOUNCE

#ifdef EN_DEBOUNCE
#include <linux/jiffies.h>

extern unsigned long volatile jiffies;
unsigned long old_jiffie = 0;
#endif

// Output LED is connected to this GPIO
#define GPIO_21_OUT (21)

// LED is connected to this GPIO
#define GPIO_25_IN (24)

// GPIO_25_IN value toggle
unsigned int led_toggle = 0;

// This used for storing the IRQ number for the GPIO
unsigned int GPIO_irqNumber;

// Interrupt handler for GPIO 25. This will be called whenever there is a raising edge detected.
static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
    static unsigned long flags = 0;

#ifdef EN_DEBOUNCE
    unsigned long diff = jiffies - old_jiffie;
    if (diff < 20) {
        return IRQ_HANDLED;
    }

    old_jiffie = jiffies;
#endif

    local_irq_save(flags);
    led_toggle = (0x01 ^ led_toggle);        // toggle the old value
    gpio_set_value(GPIO_21_OUT, led_toggle); // toggle the GPIO_21_OUT
    pr_info("Interrupt Occurred : GPIO_21_OUT : %d ", gpio_get_value(GPIO_21_OUT));
    local_irq_restore(flags);
    return IRQ_HANDLED;
}

dev_t dev = 0;
static struct class *dev_class;
static struct cdev example_gpio_cdev;

static int __init example_gpio_init(void);
static void __exit example_gpio_exit(void);

/*************** Driver functions **********************/
static int example_gpio_open(struct inode *inode, struct file *file);
static int example_gpio_release(struct inode *inode, struct file *file);
static ssize_t example_gpio_read(struct file *filp, char __user *buf, size_t len, loff_t *off);
static ssize_t example_gpio_write(struct file *filp, const char *buf, size_t len, loff_t *off);
/******************************************************/

// File operation structure
static struct file_operations fops =
    {
        .owner = THIS_MODULE,
        .read = example_gpio_read,
        .write = example_gpio_write,
        .open = example_gpio_open,
        .release = example_gpio_release,
};

static int example_gpio_open(struct inode *inode, struct file *file)
{
    pr_info("Device file opened\n");
    return 0;
}

static int example_gpio_release(struct inode *inode, struct file *file)
{
    pr_info("Device file closed\n");
    return 0;
}

static ssize_t example_gpio_read(struct file *filp,
                        char __user *buf, size_t len, loff_t *off)
{
    uint8_t gpio_state = 0;

    // reading GPIO value
    gpio_state = gpio_get_value(GPIO_21_OUT);

    // write to user
    len = 1;
    if (copy_to_user(buf, &gpio_state, len) > 0) {
        pr_err("ERROR: Not all the bytes have been copied to user\n");
    }

    pr_info("Read function : GPIO_21 = %d \n", gpio_state);

    return 0;
}

/*
** This function will be called when we write the Device file
*/
static ssize_t example_gpio_write(struct file *filp,
                         const char __user *buf, size_t len, loff_t *off)
{
    uint8_t rec_buf[10] = {0};

    if (copy_from_user(rec_buf, buf, len) > 0) {
        pr_err("ERROR: Not all the bytes have been copied from user\n");
    }

    pr_info("Write Function : GPIO_21 Set = %c\n", rec_buf[0]);

    if (rec_buf[0] == '1') {
        // set the GPIO value to HIGH
        gpio_set_value(GPIO_21_OUT, 1);
    } else if (rec_buf[0] == '0') {
        // set the GPIO value to LOW
        gpio_set_value(GPIO_21_OUT, 0);
    } else {
        pr_err("Unknown command : Please provide either 1 or 0 \n");
    }

    return len;
}

static int __init example_gpio_init(void)
{
    /*Allocating Major number*/
    if ((alloc_chrdev_region(&dev, 0, 1, "example_gpio_Dev")) < 0) {
        pr_err("Cannot allocate major number\n");
        goto r_unreg;
    }
    pr_info("Major = %d Minor = %d \n", MAJOR(dev), MINOR(dev));

    /*Creating cdev structure*/
    cdev_init(&example_gpio_cdev, &fops);

    /*Adding character device to the system*/
    if ((cdev_add(&example_gpio_cdev, dev, 1)) < 0) {
        pr_err("Cannot add the device to the system\n");
        goto r_del;
    }

    /*Creating struct class*/
    if (IS_ERR(dev_class = class_create(THIS_MODULE, "example_gpio_class"))) {
        pr_err("Cannot create the struct class\n");
        goto r_class;
    }

    /*Creating device*/
    if (IS_ERR(device_create(dev_class, NULL, dev, NULL, "example_gpio_device"))) {
        pr_err("Cannot create the Device \n");
        goto r_device;
    }

    // Output GPIO configuration
    // Checking the GPIO is valid or not
    if (gpio_is_valid(GPIO_21_OUT) == false) {
        pr_err("GPIO %d is not valid\n", GPIO_21_OUT);
        goto r_device;
    }

    // Requesting the GPIO
    if (gpio_request(GPIO_21_OUT, "GPIO_21_OUT") < 0) {
        pr_err("ERROR: GPIO %d request\n", GPIO_21_OUT);
        goto r_gpio_out;
    }

    // configure the GPIO as output
    gpio_direction_output(GPIO_21_OUT, 0);

    // Input GPIO configuratioin
    // Checking the GPIO is valid or not
    if (gpio_is_valid(GPIO_25_IN) == false) {
        pr_err("GPIO %d is not valid\n", GPIO_25_IN);
        goto r_gpio_in;
    }

    // Requesting the GPIO
    if (gpio_request(GPIO_25_IN, "GPIO_25_IN") < 0) {
        pr_err("ERROR: GPIO %d request\n", GPIO_25_IN);
        goto r_gpio_in;
    }

    // configure the GPIO as input
    gpio_direction_input(GPIO_25_IN);

    /*
    ** I have commented the below few lines, as gpio_set_debounce is not supported
    ** in the Raspberry pi. So we are using EN_DEBOUNCE to handle this in this driver.
    */
#ifndef EN_DEBOUNCE
    // Debounce the button with a delay of 200ms
    if (gpio_set_debounce(GPIO_25_IN, 200) < 0) {
        pr_err("ERROR: gpio_set_debounce - %d\n", GPIO_25_IN);
        // goto r_gpio_in;
    }
#endif

    // Get the IRQ number for our GPIO
    GPIO_irqNumber = gpio_to_irq(GPIO_25_IN);
    pr_info("GPIO_irqNumber = %d\n", GPIO_irqNumber);

    if (request_irq(GPIO_irqNumber,           // IRQ number
                    (void *)gpio_irq_handler, // IRQ handler
                    IRQF_TRIGGER_RISING,      // Handler will be called in raising edge
                    "example_gpio_device",             // used to identify the device name using this IRQ
                    NULL)) {                  // device id for shared IRQ
        pr_err("my_device: cannot register IRQ ");
        goto r_gpio_in;
    }

    pr_info("Example gpio driver enter\n");
    return 0;

r_gpio_in:
    gpio_free(GPIO_25_IN);
r_gpio_out:
    gpio_free(GPIO_21_OUT);
r_device:
    device_destroy(dev_class, dev);
r_class:
    class_destroy(dev_class);
r_del:
    cdev_del(&example_gpio_cdev);
r_unreg:
    unregister_chrdev_region(dev, 1);

    return -1;
}

static void __exit example_gpio_exit(void)
{
    free_irq(GPIO_irqNumber, NULL);
    gpio_free(GPIO_25_IN);
    gpio_free(GPIO_21_OUT);
    device_destroy(dev_class, dev);
    class_destroy(dev_class);
    cdev_del(&example_gpio_cdev);
    unregister_chrdev_region(dev, 1);
    pr_info("Example gpio driver exit\n");
}

module_init(example_gpio_init);
module_exit(example_gpio_exit);

MODULE_AUTHOR("zhonghuashu <77599567@qq.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A example GPIO Module");
