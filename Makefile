PROG := tdfiglet
SRC := tdfiglet.c
PREFIX ?= /usr/local
FONTS := fonts/*
FONTDIR := $(PREFIX)/share/$(PROG)/fonts
CC ?= cc
CFLAGS += -DFONT_DIR=\"$(FONTDIR)\" -std=c99 -Wall

UNAME := $(shell sh -c 'uname -s 2>/dev/null')

ifeq ($(UNAME),Darwin)
	CFLAGS += -Wunused-result -Wunused-value
	LDFLAGS += -liconv
endif

default: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) -o $(PROG)

.PHONY: debug clean install

install:
	install -m 0755 -d $(PREFIX)/bin
	install -m 0755 -d $(FONTDIR)
	install -m 0755 $(PROG) $(PREFIX)/bin/$(PROG)
	for i in $(FONTS) ; do install -m 0644 $$i $(FONTDIR) ; done

debug: $(SRC)
	$(CC) -DDEBUG -g $(CFLAGS) $(LDFLAGS) $(SRC) -o $(PROG)

clean:
	rm -rf $(PROG) $(PROG).dSYM

