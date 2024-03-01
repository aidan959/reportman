CC=/usr/sbin/gcc
CFLAGS=-Wextra -Wall -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code -c

SRC=src
OBJ=obj
BIN=bin

SRC_TST=src
OBJ_TST=obj

MON_T=monitor_tool
DIR_T=directory_tool
DAE=daemonize
MAIN=cto_daemon

ifeq ($(PREFIX),)
    PREFIX := /usr/sbin
endif


SOURCES := $(filter-out $(wildcard $(SRC)/tst/*), $(wildcard $(SRC)/*.c) $(wildcard $(SRC)/**/*.c))
SOURCES_TST := $(wildcard $(SRC_TST)/*)

OBJECTS := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES))
OBJECTS_TST := $(patsubst $(SRC_TST)/%.c, $(OBJ_TST)/%.o, $(SOURCES_TST))

PROG_NAME=$(BIN)/reportmand
TST_NAME=$(BIN)/reportmandtst


all: $(PROG_NAME)

$(PROG_NAME): $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) $^ -o $@
	

$(OBJ)/$(MAIN).o: $(SRC)/$(MAIN).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@

$(OBJ)/%.o: $(SRC)/%.c $(SRC)/%.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@ 

install: $(PROG_NAME) | $(PROG_NAME)
	@if [ "$(shell id -u)" != "0" ]; then\
		$(warning  This script must be run as root to install to $(PREFIX), and add service to systemctl)echo;\
	fi
	sudo install -m 744 $^ $(PREFIX)/

test: $(TST_OBJECTS)
	$(CC) $^ -o $@
obj/tst/$(MAIN).o: $(SRC)/$(MAIN).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@

obj/%.o: $(SRC)/%.c $(SRC)/%.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@ 

run: | $(PROG_NAME)
	./bin/$(PROG_NAME)


clean:
	rm -R ./bin ./obj || true