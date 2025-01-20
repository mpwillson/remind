.PHONY: clean install deinstall release html test doc

NAME=remind
OBJS=datafile.o date.o remind.o
INSTALL_DIR=/usr/local
BIN_DIR=${INSTALL_DIR}/bin
MAN_DIR=${INSTALL_DIR}/man/man1
LDLIBS=-lm
CFLAGS=-g
CFLAGS+=-DGIT_VERSION=\"$(shell git describe --tags --always --dirty)\"

target: 	${NAME} man1/${NAME}.1

${NAME}: 	${OBJS}

datafile.o:	datafile.h

date.o: 	date.h

man1/${NAME}.1: man1/${NAME}.in.1
	mandoc -Tlint $<
	cp $<  $@

clean:
	rm -f ${NAME} *.o  man1/${NAME}.html ${NAME}*.tar.gz test/test.results

install:
	cp ${NAME} ${BIN_DIR}
	mkdir -p ${MAN_DIR}
	gzip -c man1/${NAME}.1 >${MAN_DIR}/${NAME}.1.gz

deinstall:
	rm -f ${BIN_DIR}/${NAME} ${MAN_DIR}/${NAME}.1.gz

release:
	@-if [ v${version} = v ]; then \
		echo Please specify version required \(version=x.x\); \
	else \
		git archive --format=tar --prefix=${NAME}-${version}/ \
			v${version} | gzip >${NAME}-${version}.tar.gz; \
		if [ $$? = 0 ]; then \
			echo ${NAME}-${version}.tar.gz created; \
		else \
			rm ${NAME}-${version}.tar.gz; \
		fi; \
	fi

html:
	mandoc -O fragment -Thtml man1/${NAME}.1 >man1/${NAME}.html

test:
	sh test/test.sh >test/test.results 2>&1
	diff -u test/gold.results test/test.results
