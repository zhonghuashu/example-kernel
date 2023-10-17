KVERS = $(shell uname -r)

# Kernel modules
obj-m += oled_spi.o
oled_spi-objs := oled_spi.o ssd1306.o

build: kernel_modules

kernel_modules:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) modules

clean:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) clean
