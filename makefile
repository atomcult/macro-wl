CC ?= c99
LDFLAGS += -linput -levdev
CFLAGS  += -I/usr/include/libevdev-1.0/

PROG ?= macro-wl

all:
	$(CC) $(CFLAGS) main.c -o $(PROG) $(LDFLAGS)
