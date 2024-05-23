override CFLAGS=-Wall -Wextra -Wshadow -fanalyzer -g -O0 -fsanitize=address,undefined

ifdef CI
override CFLAGS=-Wall -Wextra -Wshadow -Werror
endif

.PHONY: clean all

all: sop-pos

sop-pos: sop-pos.c l4-common.h
	gcc $(CFLAGS) -o sop-pos sop-pos.c

clean:
	rm -f sop-pos
