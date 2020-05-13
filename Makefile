.PHONY: install deinstall

INSTALL_DIR=/usr/local
MAN_DIR=${INSTALL_DIR}/share/man/man1

all: remind man1/remind.1.gz

remind:	remind.c datafile.c date.c datafile.h date.h
	cc -g -o remind remind.c datafile.c date.c
man1/remind.1.gz: man1/remind.1
	gzip -c man1/remind.1 >man1/remind.1.gz
clean:
	rm -f remind *.o man1/remind.1.gz
install:
	cp remind ${INSTALL_DIR}/bin
	mkdir -p ${MAN_DIR}
	cp man1/remind.1.gz ${MAN_DIR}
deinstall:
	rm -f ${INSTALL_DIR}/bin/remind ${MAN_DIR}/remind.1.gz
