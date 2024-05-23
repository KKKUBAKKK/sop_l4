override CFLAGS=-Wall -Wextra -Wshadow -fanalyzer -g -O0 -fsanitize=address,undefined

ifdef CI
override CFLAGS=-Wall -Wextra -Wshadow -Werror
endif

.PHONY: clean all

all: server local tcp

local:  l4_common.h l4-1_client_local.c
	gcc $(CFLAGS) -o l4-1_client_local l4-1_client_local.c

tcp:  l4_common.h l4-1_client_tcp.c
	gcc $(CFLAGS) -o l4-1_client_tcp l4-1_client_tcp.c

server:  l4_common.h l4-1_server.c
	gcc $(CFLAGS) -o l4-1_server l4-1_server.c

clean:
	rm -f local tcp server
