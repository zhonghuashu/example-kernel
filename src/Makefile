SUBDIRS  = 	hello globalmem globalfifo second usb gpio_driver gpio_irq

all: ${SUBDIRS}

hello:
	@echo ' '
	@echo '>>> Building sub-component: $@'
	${MAKE} -C $@

globalmem:
	@echo ' '
	@echo '>>> Building sub-component: $@'
	${MAKE} -C $@

globalfifo:
	@echo ' '
	@echo '>>> Building sub-component: $@'
	${MAKE} -C $@

second:
	@echo ' '
	@echo '>>> Building sub-component: $@'
	${MAKE} -C $@

usb:
	@echo ' '
	@echo '>>> Building sub-component: $@'
	${MAKE} -C $@

gpio_driver:
	@echo ' '
	@echo '>>> Building sub-component: $@'
	${MAKE} -C $@

gpio_irq:
	@echo ' '
	@echo '>>> Building sub-component: $@'
	${MAKE} -C $@

clean:
	${MAKE} -C hello clean
	${MAKE} -C globalmem clean
	${MAKE} -C globalfifo clean
	${MAKE} -C second clean
	${MAKE} -C usb clean
	${MAKE} -C gpio_driver clean
	${MAKE} -C gpio_irq clean

.PHONY: ${SUBDIRS}