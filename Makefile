# wmcoremap - per-core CPU heatmap dockapp for Window Maker

PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
MANDIR  ?= $(PREFIX)/share/man/man1

PKGS    = x11 xext
CC      ?= cc
CFLAGS  ?= -O2 -g
CFLAGS  += -Wall -Wextra -std=c99 -D_GNU_SOURCE $(shell pkg-config --cflags $(PKGS))
LDLIBS  += $(shell pkg-config --libs $(PKGS)) -lm

SRC = src/wmcoremap.c src/cpu.c src/display.c src/font.c src/gpu.c src/grid.c src/sensor.c
OBJ = $(SRC:.c=.o)

all: wmcoremap

wmcoremap: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

src/wmcoremap.o: src/cpu.h src/display.h src/font.h src/gpu.h src/grid.h src/history.h src/sensor.h
src/cpu.o: src/cpu.h
src/display.o: src/display.h
src/font.o: src/font.h
src/gpu.o: src/gpu.h
src/grid.o: src/grid.h src/cpu.h src/display.h src/history.h
src/sensor.o: src/sensor.h

install: wmcoremap
	install -Dm755 wmcoremap $(DESTDIR)$(BINDIR)/wmcoremap
	install -Dm644 wmcoremap.1 $(DESTDIR)$(MANDIR)/wmcoremap.1

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/wmcoremap $(DESTDIR)$(MANDIR)/wmcoremap.1

clean:
	rm -f wmcoremap $(OBJ)

.PHONY: all install uninstall clean
