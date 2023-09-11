# Introduction
Sample code for Linux driver development.

# Getting started
- Download and install kernel source
```shell
# Check linux version
$ uname -r
5.10.102.1-microsoft-standard-WSL2
# Download: linux-msft-wsl-5.10.102.1 https://github.com/microsoft/WSL2-Linux-Kernel/releases
$ tar -xzvf WSL2-Linux-Kernel-linux-msft-wsl-5.10.102.1.tar.gz
$ cd WSL2-Linux-Kernel-linux-msft-wsl-5.10.102.1

# Create symbol link under kernel module.
$ mkdir /lib/modules/5.10.102.1-microsoft-standard-WSL2
$ ln -s /root/develop/linux/WSL2-Linux-Kernel-linux-msft-wsl-5.10.102.1 build

```
# Build and test
- Build Makefile project via `make`
- Generate compiler_commands.json file.
```shell
$ bear -- make KCONFIG_CONFIG=Microsoft/config-wsl -j8
```

- Run project `hello`

```shell
# Install / remove kernel module.
$ insmod ./hello.ko
# Pass arguments to kernel module.
$ echo 11 > /sys/module/hello/parameters/cb_hello_value
$ rmmod ./hello.ko

# Print kernel log and wait for new messages.
$ dmesg --follow
Hello World enter
Hello World exit
New value of cb_hello_value = 11
```
- `globalmem`: A simple char device driver with mutex.
```shell
# Install globalmem device driver.
$ insmod ./globalmem.ko

# Read / write arguments to kernel module.
$ echo 11 > /sys/module/globalmem/parameters/globalmem_cb_value

# Read / write /dev/globalmem using cat linux command.
$ echo "hello world" > /dev/globalmem
$ cat /dev/globalmem
hello world
$ dmesg --follow
written 12 bytes(s) from 0
read 4096 bytes(s) from 0

# ioctl device (/dev/globalmem) using user space application.
$ ./main_app

# Procfs (Process Filesystem) info exported by kernel module.
$ cat /proc/example/globalmem
try proc array
$ echo "device driver test" > /proc/example/globalmem
$ cat /proc/example/globalmem
device driver test

# Sysfs contain information about devices and drivers.
$ echo 1 > /sys/kernel/example_sysfs/sysfs_value
$ cat /sys/kernel/example_sysfs/sysfs_value
1

# Waitqueue to wait for a termination event to exit from sleep.
$ rmmod globalmem
Waiting for event - wait queue...
Event came from exit function

# Completion to wait for a read complete event to exit from sleep.
$ cat /dev/globalmem
Waiting for event - completion...
Event came from read function

# Raise interrupt using using `int` instruction when read sysfs value. Note: WSL2 linux kernel need to rebuild to export irq vector.
$ cat /sys/kernel/example_sysfs/sysfs_value
Raise interrupt IRQ 11
Shared IRQ: Interrupt occurred
# Bottom half code deferred to a work queue.
Executing workqueue function

# Linked list consists sequence of nodes (list_head [*prev, *next] -> [list_head, data 1] -> [list_head, data 2])
$ echo 1 > /sys/kernel/example_sysfs/sysfs_value
$ cat /sys/kernel/example_sysfs/sysfs_value
Node 0 data = 1
Total Nodes = 1

# Kernel thread to print some text at every 5 seconds. Mutex / spinlock used to protect shared variable between threads.
$ insmod globalmem.ko
In globalmem thread function 0
In globalmem thread function 1

# Tasklet to deferred interrupt Bottom half work.
$ cat /sys/kernel/example_sysfs/sysfs_value
Executing Tasklet Function : arg = 0

# Signal sent from kernel to user space app.
$ cat /sys/kernel/example_sysfs/sysfs_value
Sending signal to app
$ ./main_app
4
Received signal from kernel : Value =  1

# Kernel timer (ms resolution) / hrtimer (ns resolution) to trigger at every 5 seconds.
$ insmod globalmem.ko
Kernel timer callback function called [1]
hrtimer callback function called [1]

```

- `globalfifo`: A simple char device driver with block I/O, non-block poll.
```shell
# Block read / write fifo data.
$ insmod ./globalfifo.ko
#  List character device major number (/proc/devices) and create device.
$ mknod /dev/globalfifo c 231 0 -m 666
$ cat /dev/globalfifo
hello world
$ echo "hello world" > /dev/globalfifo

# Non-block read / write poll.
$ ./main_poll
# Receive async IO signal (SIGIO) from globalfifo when writing data.
$ ./main_aio
```
- `second`: kernel timer device driver (Linux WSL jiffies about 100 / per second).
```shell
$ insmod ./second.ko
$ mknod /dev/second c 232 0 -m 666
$ ./main_second
Seconds after open /dev/second: 1
Seconds after open /dev/second: 2
...
```
