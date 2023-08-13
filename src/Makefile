SUBDIRS  = 	hello globalmem globalfifo second

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

clean:
	${MAKE} -C hello clean
	${MAKE} -C globalmem clean
	${MAKE} -C globalfifo clean
	${MAKE} -C second clean

.PHONY: ${SUBDIRS}