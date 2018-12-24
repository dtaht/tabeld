PREFIX = /usr/local
MANDIR = $(PREFIX)/share/man

CDEBUGFLAGS = -O3 -Wall

DEFINES = $(PLATFORM_DEFINES)

CFLAGS = $(CDEBUGFLAGS) $(DEFINES) $(EXTRA_DEFINES)

LDLIBS = 

SRCS = kdump.c rtod.c

OBJS = kdump.o rtod.o

kdump: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o kdump kdump.o $(LDLIBS)

rtod: rtod.o

kdump.o: kdump.c version.h

rtod.o: rtod.c version.h

version.h:
	./generate-version.sh > version.h

.SUFFIXES: .man .html

.man.html:
	mandoc -Thtml $< > $@

.PHONY: all install install.minimal uninstall clean

all: kdump

TAGS: $(SRCS) $(HEADERS)
	etags $(SRCS) $(HEADERS)

install.minimal: kdump
	-rm -f $(TARGET)$(PREFIX)/bin/kdump
	mkdir -p $(TARGET)$(PREFIX)/bin
	cp -f kdump$(TARGET)$(PREFIX)/bin

install: install.minimal all
	mkdir -p $(TARGET)$(MANDIR)/man8
	cp -f kdump.man $(TARGET)$(MANDIR)/man8/babeld.8

uninstall:
	-rm -f $(TARGET)$(PREFIX)/bin/kdump
	-rm -f $(TARGET)$(MANDIR)/man8/kdump.8

clean:
	-rm -f kdump.html version.h *.o *~ core gmon.out
