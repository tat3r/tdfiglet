PROG := tdfiglet
SRC := tdfiglet.c
PREFIX ?= /usr/local
FONTS := fonts/*
FONTDIR := $(PREFIX)/share/$(PROG)/fonts
CC ?= cc
CFLAGS += -DFONT_DIR=\"$(FONTDIR)\" -std=c99 -Wall
DFLAGS = -g
UNAME := $(shell sh -c 'uname -s 2>/dev/null')

ifeq ($(UNAME), Darwin)
	CC = clang
	CFLAGS += -Wunused-result -Wunused-value
	DLAGS += -fsanitize=address -fsanitize=undefined -fsanitize=leak
	LDFLAGS += -liconv
endif

default: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) -o $(PROG)

.PHONY: debug clean install

install:
	test -d $(PREFIX)/bin || mkdir -p $(PREFIX)/bin
	cp $(PROG) $(PREFIX)/bin
	test -d $(FONTDIR) || mkdir -p $(FONTDIR)
	rm -f $(FONTDIR)/*.tdf
	for i in $(FONTS) ; do cp -v $$i $(FONTDIR) ; done
	chmod ugo+r $(FONTDIR)/*.tdf

debug: $(SRC)
	$(CC) -DDEBUG $(CFLAGS) $(DFLAGS) $(LDFLAGS) $(SRC) -o $(PROG)

clean:
	rm -rf $(PROG) $(PROG).dSYM

