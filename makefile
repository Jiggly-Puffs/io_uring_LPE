CCFLAGS ?= -Wall -D_GNU_SOURCE -luring -g -O3 -static
.PHONY: all

all: exp hello exp1

clean:
	rm -f $(all_targets)
exp: exp.c
	$(CC) exp.c -o ./exp ${CCFLAGS}
exp1: exp1.c
	$(CC) exp1.c -o ./exp1 ${CCFLAGS}
hello: hello.c
	$(CC) hello.c -o ./hello ${CCFLAGS} `pkg-config fuse --cflags --libs`
