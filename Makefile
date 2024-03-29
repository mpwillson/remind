.PHONY: clean install deinstall release html test

OBJS=datafile.o date.o remind.o
INSTALL_DIR=/usr/local
BIN_DIR=${INSTALL_DIR}/bin
MAN_DIR=${INSTALL_DIR}/man/man1
LDLIBS=-lm
CFLAGS=-g
CFLAGS+=-DGIT_VERSION=\"$(shell git describe --tags --always --dirty)\"

all: remind man1/remind.1.gz

remind: ${OBJS}

datafile.o:	datafile.h

date.o: date.h

man1/remind.1.gz: man1/remind.1
	gzip -c man1/remind.1 >man1/remind.1.gz

clean:
	rm -f remind *.o man1/remind.1.gz remind.html remind*.tar.gz \
		test/test.results

install:
	cp remind ${BIN_DIR}
	mkdir -p ${MAN_DIR}
	cp man1/remind.1.gz ${MAN_DIR}

deinstall:
	rm -f ${INSTALL_DIR}/bin/remind ${MAN_DIR}/remind.1.gz

release:
	@-if [ v${version} = v ]; then \
		echo Please specify version required \(version=x.x\); \
	else \
		git archive --format=tar --prefix=remind-${version}/ v${version} | \
			gzip >remind-${version}.tar.gz; \
		if [ $$? = 0 ]; then \
			echo remind-${version}.tar.gz created; \
		else \
			rm remind-${version}.tar.gz; \
		fi; \
	fi

html:
	mandoc -O fragment -T html man1/remind.1 >remind.html

test:
	sh test/test.sh >test/test.results 2>&1
	diff -u test/gold.results test/test.results
