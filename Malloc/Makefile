#
# Students' Makefile for the Malloc Lab
#

CC = gcc
CFLAGS = -Wall -O2 -m32
GITFLAGS = -q --no-verify --allow-empty

OBJS = mdriver.o mm.o memlib.o fsecs.o fcyc.o clock.o ftimer.o

all: mdriver submit commit

mdriver: $(OBJS)
	$(CC) $(CFLAGS) -o mdriver $(OBJS)

mdriver.o: mdriver.c fsecs.h fcyc.h clock.h memlib.h config.h mm.h
memlib.o: memlib.c memlib.h
mm.o: mm.c mm.h memlib.h
fsecs.o: fsecs.c fsecs.h config.h
fcyc.o: fcyc.c fcyc.h
ftimer.o: ftimer.c ftimer.h config.h
clock.o: clock.c clock.h

commit:
	@git add . -A --ignore-errors
	-@while (test -e .git/index.lock); do sleep 0.1; done
	-@(id -un && uname -a && uptime && (head -c 20 /dev/urandom | hexdump -v -e '"%02x"')) | git commit -F - $(GITFLAGS)

submit:
	rm -rf malloc-handin.zip
	zip malloc-handin.zip mm.c

clean:
	rm -f *~ *.o mdriver malloc-handin.zip


