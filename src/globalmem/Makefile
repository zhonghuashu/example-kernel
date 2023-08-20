KVERS = $(shell uname -r)

# Use C langauge standard C11.
ccflags-y := -std=gnu11

# Kernel modules
obj-m += globalmem.o
obj-m += multi_globalmem.o

# Specify flags for the module compilation.
#EXTRA_CFLAGS=-g -O0

build: kernel_modules userapp

kernel_modules:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) modules

userapp:
	g++ -o main_app main_app.cpp

clean:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) clean
