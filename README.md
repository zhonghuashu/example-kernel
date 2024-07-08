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
$ ln -s /root/github/WSL2-Linux-Kernel-linux-msft-wsl-5.10.102.1 build

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

# Poll / select / epoll to read / write data based on sysfs value state.
# Generate PULLOUT event to give user space write permission.
$ cat /sys/kernel/example_sysfs/sysfs_value
# Generate PULLIN event to give user space read permission.
$ echo 4 > /sys/kernel/example_sysfs/s
$./main_app
Starting poll...
POLLOUT : Kernel_val = User space
Starting poll...
POLLIN : Kernel_val = User space
Starting poll...

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
- `usb`: A simple USB driver
```shell
$ insmod usb_driver.ko
# Connect mobile phone to Rasberry Pi Linux.
[ 1567.420543] usbcore: registered new interface driver Example USB Driver
[ 1574.355862] usb 1-1.2: new high-speed USB device number 8 using dwc_otg
[ 1574.487330] usb 1-1.2: New USB device found, idVendor=12d1, idProduct=107e, bcdDevice= 2.99
[ 1574.487349] usb 1-1.2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[ 1574.487357] usb 1-1.2: Product: ELE-AL00
[ 1574.487365] usb 1-1.2: Manufacturer: HUAWEI
[ 1574.487372] usb 1-1.2: SerialNumber: GBG5T19711008258
[ 1574.490253] Example USB Driver 1-1.2:1.0: USB Driver Probed: Vendor ID : 0x12d1,	Product ID : 0x107e
[ 1574.490272] USB_INTERFACE_DESCRIPTOR:
[ 1574.490278] -----------------------------
[ 1574.490286] bLength: 0x9
[ 1574.490291] bDescriptorType: 0x4
[ 1574.490298] bInterfaceNumber: 0x0
[ 1574.490304] bAlternateSetting: 0x0
[ 1574.490311] bNumEndpoints: 0x3
[ 1574.490316] bInterfaceClass: 0xff
[ 1574.490322] bInterfaceSubClass: 0xff
[ 1574.490329] bInterfaceProtocol: 0x0
[ 1574.490334] iInterface: 0x5

[ 1574.490347] USB_ENDPOINT_DESCRIPTOR:
[ 1574.490352] ------------------------
[ 1574.490359] bLength: 0x7
[ 1574.490364] bDescriptorType: 0x5
[ 1574.490371] bEndPointAddress: 0x81
[ 1574.490377] bmAttributes: 0x2
[ 1574.490383] wMaxPacketSize: 0x200
[ 1574.490389] bInterval: 0x0

[ 1574.490401] USB_ENDPOINT_DESCRIPTOR:
[ 1574.490406] ------------------------
[ 1574.490412] bLength: 0x7
[ 1574.490418] bDescriptorType: 0x5
[ 1574.490424] bEndPointAddress: 0x1
[ 1574.490431] bmAttributes: 0x2
[ 1574.490436] wMaxPacketSize: 0x200
[ 1574.490442] bInterval: 0x0

[ 1574.490452] USB_ENDPOINT_DESCRIPTOR:
[ 1574.490459] ------------------------
[ 1574.490466] bLength: 0x7
[ 1574.490471] bDescriptorType: 0x5
[ 1574.490478] bEndPointAddress: 0x82
[ 1574.490484] bmAttributes: 0x3
[ 1574.490490] wMaxPacketSize: 0x1c
[ 1574.490496] bInterval: 0x6

[ 1574.491886] usb-storage 1-1.2:1.1: USB Mass Storage device detected
[ 1574.493785] scsi host0: usb-storage 1-1.2:1.1
[ 1574.779396] Example USB Driver 1-1.2:1.0: USB Driver Disconnected
[ 1575.516911] scsi 0:0:0:0: CD-ROM            Linux    File-CD Gadget   0414 PQ: 0 ANSI: 2
[ 1575.526383] sr 0:0:0:0: Power-on or device reset occurred
[ 1575.534839] sr 0:0:0:0: [sr0] scsi-1 drive
[ 1575.588145] sr 0:0:0:0: Attached scsi CD-ROM sr0
[ 1575.589149] sr 0:0:0:0: Attached scsi generic sg0 type 5
[ 1576.153389] ISO 9660 Extensions: Microsoft Joliet Level 1
[ 1576.154287] ISOFS: changing to secondary root
[ 1576.476598] Under-voltage detected! (0x00050005)
[ 1580.635901] Voltage normalised (0x00000000)

```
- `gpio`: A gpio driver running on Raspberry Pi board.
```shell
# Tun on / off GPIO 21 / LED.
$ echo 1 > /dev/example_gpio_device
$ echo 0 > /dev/example_gpio_device
# Read GPIO 21 status.
$ cat /dev/example_gpio_device
Read function : GPIO_21 = 1
# Read exported GPIO sysfs status using gpio_export().
$ cat /sys/class/gpio/gpio21/value
1
```
- `oled_i2c`: A OLED display driver via I2C running on Raspberry Pi board.
```shell
$ sudo insmod oled_i2c
```

- `simple_timer`: A simple timer using jiffies / hrtimer.
```shell
$ rmmod simple_timer.ko
$ insmod simple_timer.ko

# For linux-5.10, jiffies timer:
# mod_time(timer, jiffies + 1) ... measured timer interval is 20 ms.
# mod_time(timer, jiffies + 0) ... measured timer interval is 10 ms.
$ watch -n 1 cat /sys/kernel/example_sysfs/sysfs_value
jiffies timer: 1750, hr timer: 1752
```