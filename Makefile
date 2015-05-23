CFLAGS =	-g -O0 -D_BSD_SOURCE

.PHONY: all
all: battstat

battstat: battstat.c
	clang $(CFLAGS) -o $@ battstat.c -lacpi -lpthread

clean:
	rm -f battstat
