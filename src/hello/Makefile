KVERS = $(shell uname -r)

# Kernel modules
obj-m += hello.o

# Specify flags for the module compilation.
#EXTRA_CFLAGS=-g -O0

build: kernel_modules

kernel_modules:
	make -C /root/develop/linux/WSL2-Linux-Kernel-linux-msft-wsl-5.10.102.1 M=$(CURDIR) modules

clean:
	make -C /root/develop/linux/WSL2-Linux-Kernel-linux-msft-wsl-5.10.102.1 M=$(CURDIR) clean
