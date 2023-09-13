KVERS = $(shell uname -r)

# Use C langauge standard C11.
ccflags-y := -std=gnu11

# Kernel modules
obj-m += usb_driver.o

# Specify flags for the module compilation.
#EXTRA_CFLAGS=-g -O0

build: kernel_modules

kernel_modules:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) modules

clean:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) clean
