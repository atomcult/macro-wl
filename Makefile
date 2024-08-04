.POSIX:
	
CC = c99
LDFLAGS = -linput -levdev
CFLAGS  = -I/usr/include/libevdev-1.0/libevdev

PREFIX = /usr/local
BINDIR = ${PREFIX}/bin

grabkb:
	$(CC) $(CFLAGS) grabkb.c -o grabkb $(LDFLAGS)

install:
	mkdir -p ${DESTDIR}${BINDIR}
	cp -f grabkb   ${DESTDIR}${BINDIR}/
	cp -f macro-wl ${DESTDIR}${BINDIR}/

uninstall:
	rm -f ${DESTDIR}${BINDIR}/grabkb
	rm -f ${DESTDIR}${BINDIR}/macro-wl

clean:
	rm -f grabkb

.PHONY: install uninstall clean
