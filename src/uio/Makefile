KVERS = $(shell uname -r)

# Kernel modules
obj-m += example_uio.o

# Specify flags for the module compilation.
#EXTRA_CFLAGS=-g -O0

build: kernel_modules userapp

kernel_modules:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) modules

userapp:
	g++ -o main_uio main_uio.cpp

clean:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) clean
