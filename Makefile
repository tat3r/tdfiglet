PROG := tdfiglet
SRC := tdfiglet.c
CC := cc
CFLAGS += -std=c99 -Wall

UNAME := $(shell sh -c 'uname -s 2>/dev/null')

ifeq ($(UNAME),Darwin)
	CC := clang
	CFLAGS += -Wunused-result -Wunused-value
	LDFLAGS += -liconv
endif

default: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) -o $(PROG)

.PHONY: debug clean

debug: $(SRC)
	$(CC) -DDEBUG -g $(CFLAGS) $(LDFLAGS) $(SRC) -o $(PROG)

clean:
	rm -rf $(PROG) $(PROG).dSYM
