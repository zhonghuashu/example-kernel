KVERS = $(shell uname -r)

# Kernel modules
obj-m += second.o

# Specify flags for the module compilation.
#EXTRA_CFLAGS=-g -O0

build: kernel_modules userapp

kernel_modules:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) modules

userapp:
	g++ -o main_second main_second.cpp

clean:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) clean
	rm $(CURDIR)/main_second
