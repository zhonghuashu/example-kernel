KVERS = $(shell uname -r)

# Kernel modules
obj-m += globalfifo.o

# Specify flags for the module compilation.
#EXTRA_CFLAGS=-g -O0

build: kernel_modules userapp

kernel_modules:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) modules

userapp:
	g++ -o main_poll main_poll.cpp
	g++ -o main_sigio main_sigio.cpp
	g++ -o main_aio main_aio.cpp

clean:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) clean
	rm $(CURDIR)/main_poll
	rm $(CURDIR)/main_sigio
	rm $(CURDIR)/main_aio
