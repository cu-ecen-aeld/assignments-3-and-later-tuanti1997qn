CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -ggdb -Wall -Werror
LDFLAGS ?= -lpthread -lrt

all:
	$(CC) ${CFLAGS} -o aesdsocket aesdsocket.c ${LDFLAGS}
clean:
	@if [ -f aesdsocket ]; then rm aesdsocket; fi

.PHONY: all clean
