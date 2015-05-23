CFLAGS =	-Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Wcast-align
CFLAGS +=	-Wwrite-strings -Wmissing-prototypes -Wmissing-declarations
CFLAGS +=	-Wnested-externs -Winline -Wno-long-long  -Wunused-variable
CFLAGS +=	-Wstrict-prototypes -Werror
CFLAGS +=	-std=c99 -static -D_XOPEN_SOURCE -D_BSD_SOURCE

.PHONY: all
all: battstat

battstat: battstat.c
	clang $(CFLAGS) -o $@ battstat.c -lacpi -lpthread

clean:
	rm -f battstat

