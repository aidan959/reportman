CC=/usr/sbin/gcc
CFLAGS=-Wextra -Wall -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code -c

SRC=src
OBJ=obj
BIN=bin

MON_T=monitor_tool
DIR_T=directory_tool
DAE=daemonize
MAIN=cto_daemon

ifeq ($(PREFIX),)
    PREFIX := /usr/sbin
endif


SOURCES := $(filter-out $(wildcard src/tst/*), $(wildcard src/*.c) $(wildcard src/**/*.c))

OBJECTS := $(patsubst src/%.c, obj/%.o, $(SOURCES))

PROG_NAME=$(BIN)/reportmand

all: $(PROG_NAME)

$(PROG_NAME): $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) $^ -o $@
	

obj/$(MAIN).o: $(SRC)/$(MAIN).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@

obj/%.o: $(SRC)/%.c $(SRC)/%.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@ 

install: $(PROG_NAME) | $(PROG_NAME)
	@if [ "$(shell id -u)" != "0" ]; then\
		$(warning  This script must be run as root to install to $(PREFIX), and add service to systemctl)echo;\
	fi
	sudo install -m 744 $^ $(PREFIX)/

run: | $(PROG_NAME)
	./bin/$(PROG_NAME)

clean:
	rm -R ./bin ./obj