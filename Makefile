# compiler stuff
CC ?= cc
CFLAGS ?= -Wall -pedantic
LIBS =

# installation stuff
PREFIX ?= /usr
BIN_DIR ?= $(PREFIX)/bin
MAN_DIR ?= $(PREFIX)/share/man

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

libretee: libretee.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

clean:
	rm -v -f libretee *.o

install: libretee
	install -vD -m 755 libretee $(BIN_DIR)/libretee
	install -vD -m 644 libretee.1 $(MAN_DIR)/man1/libretee.1

uninstall:
	rm -vrf $(DOC_DIR)/libretee
	rm -vf $(MAN_DIR)/man1/libretee.1