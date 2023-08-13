SRCDIR	 = src

all: ${SRCDIR}

src:
	${MAKE} -C $@

clean:
	${MAKE} -C ${SRCDIR} clean

.PHONY: ${SRCDIR}