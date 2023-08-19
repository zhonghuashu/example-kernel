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
$ rmmod ./hello.ko

# Print kernel log and wait for new messages.
$ dmesg --follow
[ 2631.437902] Hello World enter
[ 2813.972935] Hello World exit
```
- `globalmem`: A simple char device driver with mutex.
```shell
# Install globalmem device driver.
$ insmod ./globalmem.ko

# List globalmem character device (major 230)
$ cat /proc/devices
Character devices:
230 globalmem

# Create device node with major 230, minor 0
$ mknod /dev/globalmem c 230 0 -m 666

# Read / write globalmem.
$ echo "hello world" > /dev/globalmem
$ cat /dev/globalmem
hello world

$ dmesg --follow
[32727.080308] written 12 bytes(s) from 0
[32736.671514] read 4096 bytes(s) from 0
```

- `globalfifo`: A simple char device driver with block I/O, non-block poll.
```shell
# Block read / write fifo data.
$ insmod ./globalfifo.ko
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
