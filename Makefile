PROG := tdfiglet
SRC := tdfiglet.c
PREFIX ?= /usr/local
FONTS := fonts/*
FONTDIR := $(PREFIX)/share/$(PROG)/fonts
CC ?= cc
CFLAGS += -DFONT_DIR=\"$(FONTDIR)\" -std=c99 -Wall -g

UNAME := $(shell sh -c 'uname -s 2>/dev/null')

ifeq ($(UNAME),Darwin)
	CFLAGS += -Wunused-result -Wunused-value
	LDFLAGS += -liconv
endif

default: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) -o $(PROG)

.PHONY: debug clean install

install:
	test -d $(FONTDIR) || mkdir -p $(PREFIX)/bin
	cp $(PROG) $(PREFIX)/bin
	test -d $(FONTDIR) || mkdir -p $(FONTDIR)
	for i in $(FONTS) ; do cp -v $$i $(FONTDIR) ; done

debug: $(SRC)
	$(CC) -DDEBUG -g $(CFLAGS) $(LDFLAGS) $(SRC) -o $(PROG)

clean:
	rm -rf $(PROG) $(PROG).dSYM

