CC ?= c99
LDFLAGS ?= -linput -levdev
CFLAGS  ?= -I/usr/include/libevdev-1.0/

PROG ?= grabkb

all:
	$(CC) $(CFLAGS) $(PROG).c -o $(PROG) $(LDFLAGS)
