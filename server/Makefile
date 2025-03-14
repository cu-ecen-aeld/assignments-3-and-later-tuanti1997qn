CC ?= $(CROSS_COMPILE)gcc

all:
	$(CC) -ggdb -Werror -Wall -o aesdsocket aesdsocket.c
clean:
	@if [ -f aesdsocket ]; then rm aesdsocket; fi

.PHONY: all clean
