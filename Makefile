CC=/usr/sbin/gcc
CFLAGS=-g -Wextra -Wall -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code -c
LIBS=-lcrypto
SRC=src
OBJ=obj
BIN=bin

SRC_TST=src/tst
OBJ_TST=obj/tst


NAME=reportman
DAEMON=$(NAME)d
CLIENT=$(NAME)c

MAIN_TST=test
ifeq ($(PREFIX),)
    PREFIX := /usr/sbin
endif


SOURCES_C := $(filter-out $(wildcard $(SRC_TST)/*), $(wildcard $(SRC)/*.c) $(wildcard $(SRC)/**/*.c))
SOURCES_D := $(filter-out $(wildcard $(SRC_TST)/*), $(wildcard $(SRC)/*.c) $(wildcard $(SRC)/**/*.c))
SOURCES_TST := $(wildcard $(SRC_TST)/*)

OBJECTS_C := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES))
OBJECTS_D := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES))

OBJECTS_TST := $(patsubst $(SRC_TST)/%.c, $(OBJ_TST)/%.o, $(SOURCES_TST))

DAEMON_BIN=$(BIN)/$(DAEMON)
CLIENT_BIN=$(BIN)/$(CLIENT)

TST_NAME=$(BIN)/reportmandtst


all: $(DAEMON_BIN) $(CLIENT_BIN)
test: $(TST_NAME)


$(DAEMON_BIN): $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) $^ -o $@ $(LIBS)

$(OBJ)/$(DAEMON).o: $(SRC)/$(DAEMON).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@

$(CLIENT_BIN) : $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) $^ -o $@ $(LIBS)

$(OBJ)/$(CLIENT).o: $(SRC)/$(CLIENT).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@

$(OBJ)/%.o: $(SRC)/%.c $(SRC)/%.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@ 

install: $(DAEMON_BIN) | $(DAEMON_BIN)
	@if [ "$(shell id -u)" != "0" ]; then\
		$(warning  This script must be run as root to install to $(PREFIX), and add service to systemctl)echo;\
	fi
	sudo setcap 'CAP_WAKE_ALARM' $@
	sudo install -m 744 $^ $(PREFIX)/

$(TST_NAME): $(OBJECTS_TST)
	@mkdir -p $(@D)
	$(CC) $^ -o $@ $(LIBS)

$(OBJ_TST)/$(MAIN_TST).o: $(SRC_TST)/$(MAIN_TST).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@

$(OBJ_TST)/%.o: $(SRC_TST)/%.c $(SRC_TST)/%.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@ 

run: | $(DAEMON_BIN)
	./$(DAEMON_BIN)

runtest: | $(TST_NAME)
	./$(TST_NAME)

clean:
	rm -R ./bin ./obj || true