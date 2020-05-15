.PHONY: clean install deinstall release html

INSTALL_DIR=/usr/local
MAN_DIR=${INSTALL_DIR}/share/man/man1

all: remind man1/remind.1.gz

remind:	remind.c datafile.c date.c datafile.h date.h
	cc -lm -g -o remind remind.c datafile.c date.c
man1/remind.1.gz: man1/remind.1
	gzip -c man1/remind.1 >man1/remind.1.gz
clean:
	rm -f remind *.o man1/remind.1.gz remind.html remind*.tar.gz
install:
	cp remind ${INSTALL_DIR}/bin
	mkdir -p ${MAN_DIR}
	cp man1/remind.1.gz ${MAN_DIR}
deinstall:
	rm -f ${INSTALL_DIR}/bin/remind ${MAN_DIR}/remind.1.gz
release:
	@if [ v${version} = v ]; then \
		echo Please specify version required \(version=x.x\); \
	else \
		git archive --prefix=remind-${version}/ v${version} \
			>remind-${version}.tar; \
		gzip remind-${version}.tar; \
		echo remind-${version}.tar.gz created; \
	fi
html:
	mandoc -T html man1/remind.1 >remind.html
