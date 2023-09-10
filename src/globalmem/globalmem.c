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
#include <asm/hw_irq.h>
#include <linux/err.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

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
static irqreturn_t irq_handler(int irq, void *dev_id);

/***************** Work queue *******************/
void workqueue_fn(struct work_struct *work);

/* Creating work by Static Method */
DECLARE_WORK(workqueue, workqueue_fn);

/***************** kernel linked list *******************/
struct my_list{
     struct list_head list;     // linux kernel list implementation
     int data;
};

/* Declare and init the head node of the linked list. */
LIST_HEAD(head_node);

/***************** kernel thread *******************/
static struct task_struct *globalmem_thread;
static struct task_struct *globalmem_thread2;
int thread_function(void *pv);
int thread_function2(void *pv);

/***************** tasklet *******************/
void tasklet_fn(unsigned long);
// Tasklet by Dynamic Method.
struct tasklet_struct *tasklet = NULL;

/***************** race condition *******************/
struct mutex globalmem_mutex;
spinlock_t globalmem_spinlock;
rwlock_t globalmem_rwlock;
int globalmem_thread_variable;

/***************** signal *******************/
#define SIGETX 44
#define REG_CURRENT_TASK _IOW(GLOBALMEM_MAGIC, 0x02, int32_t *)
static struct task_struct *sig_task = NULL;
static int sig_num = 0;

/***************** kernel timer *******************/
#define TIMEOUT 5000    // milliseconds
static struct timer_list globalmem_timer;
static unsigned int timer_counter = 0;
void timer_callback(struct timer_list * data);

/***************** htrimer *******************/
#define TIMEOUT_NSEC   ( 1000000000L )      // 1 second in nano seconds
#define TIMEOUT_SEC    ( 4 )                // 4 seconds
static struct hrtimer globalmem_hr_timer;
static unsigned int hrtimer_counter = 0;
enum hrtimer_restart hr_timer_callback(struct hrtimer *timer);

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

/**
 * Timer Callback function. This will be called when timer expires.
 */
void timer_callback(struct timer_list * data)
{
    /* do your timer stuff here */
    if (++timer_counter == 1) {
        pr_info("Kernel timer callback function called [%d]\n", timer_counter);
    }

    /*
       Re-enable timer. Because this function will be called only first time.
       If we re-enable this will work like periodic timer.
    */
    mod_timer(&globalmem_timer, jiffies + msecs_to_jiffies(TIMEOUT));
}

/**
 * Tasklet function.
 */
void tasklet_fn(unsigned long arg)
{
    pr_info("Executing Tasklet Function : arg = %ld\n", arg);
}

/**
 * Kernel thread function.
 */
int thread_function(void *pv)
{
    int i = 0;
    while(!kthread_should_stop()) {
        i++;
        // Mutex example.
        mutex_lock(&globalmem_mutex);
        if (i == 1) {
            pr_info("Mutex is locked in Thread Function 1\n");
        }
        globalmem_thread_variable++;
        mutex_unlock(&globalmem_mutex);

        // Spin lock example.
        spin_lock(&globalmem_spinlock);
        if(i == 1 && spin_is_locked(&globalmem_spinlock)) {
            pr_info("Spinlock is locked in Thread Function 1\n");
        }
        globalmem_thread_variable++;
        spin_unlock(&globalmem_spinlock);

        // Read-write spin lock example.
        read_lock(&globalmem_rwlock);
        if (i == 1) {
            pr_info("Read lock value %d\n", globalmem_thread_variable);
        }
        read_unlock(&globalmem_rwlock);

        if (i == 1) {
            pr_info("In globalmem thread function %d\n", i++);
            pr_info("globalmem_thread_variable: %d\n", globalmem_thread_variable);
        }

        msleep(1000);
    }
    return 0;
}

/**
 * Kernel thread function.
 */
int thread_function2(void *pv)
{
    while(!kthread_should_stop()) {
        mutex_lock(&globalmem_mutex);
        globalmem_thread_variable++;
        mutex_unlock(&globalmem_mutex);

        spin_lock(&globalmem_spinlock);
        globalmem_thread_variable++;
        spin_unlock(&globalmem_spinlock);

        write_lock(&globalmem_rwlock);
        globalmem_thread_variable++;
        write_unlock(&globalmem_rwlock);

        msleep(1000);
    }
    return 0;
}
/**
 * Workqueue Function
 */
void workqueue_fn(struct work_struct *work)
{
    struct my_list *temp_node = NULL;
    struct my_list *temp;
    int count = 0;
    pr_info("Executing Workqueue Function\n");

    // Link list: Creating Node.
    temp_node = kmalloc(sizeof(struct my_list), GFP_KERNEL);
    // Assgin the data that is received.
    temp_node->data = sysfs_value;
    // Init the list within the struct.
    INIT_LIST_HEAD(&temp_node->list);
    // Add Node to Linked List.
    list_add_tail(&temp_node->list, &head_node);
    // Traversing Linked List and Print its Members.
    list_for_each_entry(temp, &head_node, list)
    {
        pr_info("Node %d data = %d\n", count++, temp->data);
    }
    pr_info("Total Nodes = %d\n", count);
}

/**
 * Interrupt handler for IRQ 11.
 */
static irqreturn_t irq_handler(int irq, void *dev_id)
{
    struct kernel_siginfo info;

    // Top half
    pr_info("Shared IRQ: Interrupt Occurred");

    // Bottom half using global workqueue.
    schedule_work(&workqueue);

    // Schedule task to tasklet with normal priority.
    tasklet_schedule(tasklet);

    // Sending signal to the user space app.
    memset(&info, 0, sizeof(struct kernel_siginfo));
    info.si_signo = SIGETX;
    info.si_code = SI_QUEUE;
    info.si_int = 1;

    if (sig_task != NULL) {
        pr_info("Sending signal to app\n");
        if(send_sig_info(SIGETX, &info, sig_task) < 0) {
            pr_info("Unable to send signal\n");
        }
    }

    return IRQ_HANDLED;
}


/**
 * This function will be called when we read the sysfs file.
 */
static ssize_t sysfs_show(struct kobject *kobj,
                          struct kobj_attribute *attr, char *buf)
{
    struct irq_desc *desc;

    pr_info("Sysfs - Read!!!\n");
    pr_info("Raise interrupt IRQ 11\n");

    // Raise IRQ 11.
    desc = irq_to_desc(11);
    if (!desc) {
        return -EINVAL;
    }

    // Corresponding to irq 11: FIRST_EXTERNAL_VECTOR 0x20 + IRQ0_VECTOR 0x10 + IRQ 11 = 0x3B (59)
    __this_cpu_write(vector_irq[59], desc);
    asm("int $0x3B");

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
    pr_info("Device driver file opened\n");
    // Pointer private_data to device struct.
    filp->private_data = globalmem_devp;
    return 0;
}

static int globalmem_release(struct inode *inode, struct file *filp)
{
    struct task_struct *ref_task = get_current();

    pr_info("Device driver file closed\n");

    // Delete registered signal task.
    if (ref_task == sig_task) {
        sig_task = NULL;
    }

    return 0;
}

static long globalmem_ioctl(struct file *filp, unsigned int cmd,
                            unsigned long arg)
{
    // Fetch device instance pointer via private_data member.
    struct globalmem_dev *dev = filp->private_data;

    switch (cmd) {
    case MEM_CLEAR:
        pr_info("Clear memory buffer to zero\n");
        memset(dev->mem, 0, GLOBALMEM_SIZE);
        break;
    case REG_CURRENT_TASK:
        pr_info("Register current task\n");
        sig_task = get_current();
        sig_num = SIGETX;
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
    ktime_t ktime;

    /*
    Use kernel module arguments.
    */
    pr_info("globalmem_value = %d  \n", globalmem_value);
    pr_info("globalmem_cb_value = %d  \n", globalmem_cb_value);
    pr_info("globalmem_hello_name = %s \n", globalmem_hello_name);
    for (int i = 0; i < (sizeof globalmem_arr_value / sizeof(int)); i++) {
        pr_info("array_value[%d] = %d\n", i, globalmem_arr_value[i]);
    }

    globalmem_devp = kzalloc(sizeof(struct globalmem_dev), GFP_KERNEL);
    if (!globalmem_devp) {
        ret = -ENOMEM;
        goto fail_malloc;
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

    // Creating work by Dynamic Method.
    INIT_WORK(&workqueue, workqueue_fn);

    // Creating kernel thread.
    globalmem_thread = kthread_create(thread_function, NULL, "globalmem Thread");
    if (globalmem_thread) {
        pr_info("Kthread Created Successfully...\n");
        wake_up_process(globalmem_thread);
    } else {
        pr_err("Cannot create kthread\n");
        goto irq;
    }
    globalmem_thread2 = kthread_run(thread_function2, NULL, "globalmem Thread 2");
    if (globalmem_thread2) {
        pr_info("Kthread2 Created Successfully...\n");
    } else {
        pr_err("Cannot create kthread2\n");
        goto irq;
    }

    // Init the tasklet bt Dynamic Method.
    tasklet = kmalloc(sizeof(struct tasklet_struct), GFP_KERNEL);
    if (tasklet == NULL) {
        pr_info("globalmem_device: cannot allocate Memory");
        goto irq;
    }
    tasklet_init(tasklet, tasklet_fn, 0);

    // Initialize methods for race conditions.
    mutex_init(&globalmem_mutex);
    spin_lock_init(&globalmem_spinlock);
    rwlock_init(&globalmem_rwlock);

    // Setup your timer to call my_timer_callback.
    // If you face some issues and using older kernel version, then you can try setup_timer API(Change Callback function's argument to unsingned long instead of struct timer_list *.
    timer_setup(&globalmem_timer, timer_callback, 0);
    // Setup timer interval to based on TIMEOUT Macro.
    mod_timer(&globalmem_timer, jiffies + msecs_to_jiffies(TIMEOUT));

    // Setup hrtimer to call my_timer_callback.
    ktime = ktime_set(TIMEOUT_SEC, TIMEOUT_NSEC);
    hrtimer_init(&globalmem_hr_timer, CLOCK_MONOTONIC /* Always to move forward in time */, HRTIMER_MODE_REL /* Relative */);
    globalmem_hr_timer.function = &hr_timer_callback;
    hrtimer_start( &globalmem_hr_timer, ktime, HRTIMER_MODE_REL);

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
    struct my_list *cursor, *temp;

    hrtimer_cancel(&globalmem_hr_timer);
    del_timer(&globalmem_timer);
    tasklet_kill(tasklet);
    if (tasklet != NULL) {
        kfree(tasklet);
    }

    kthread_stop(globalmem_thread);
    kthread_stop(globalmem_thread2);

    // Go through the list and free the memory. Type-safe against the removal of list entry.
    list_for_each_entry_safe(cursor, temp, &head_node, list)
    {
        list_del(&cursor->list);
        kfree(cursor);
    }
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
