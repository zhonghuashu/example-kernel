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

# Read / write / ioctl device (/dev/globalmem) using user space application.
$ ./main_app

# Read / write procfs (Process Filesystem) info exported by kernel module.
$ cat /proc/example/globalmem
try proc array
$ echo "device driver test" > /proc/example/globalmem
$ cat /proc/example/globalmem
device driver test

# Read / write files in sysfs contain information about devices and drivers.
$ echo 1 > /sys/kernel/example_sysfs/sysfs_value
$ cat /sys/kernel/example_sysfs/sysfs_value
1

# Waiting for a termination event to exit from sleep using waitqueue.
$ rmmod globalmem
Waiting For Event...
Event Came From Exit Function

# Raise interrupt when read sysfs value.(Kernel need to rebuild avoid error)
$ cat /sys/kernel/example_sysfs/sysfs_value
Raise interrupt IRQ 11
No irq handler for vector
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
