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
LOG_DIR=/var/log/$(NAME)

MAIN_TST=test

SBIN := /usr/sbin
USRBIN := /usr/bin



SOURCES_C := $(filter-out $(wildcard $(SRC)/$(DAEMON).c),$(filter-out $(wildcard $(SRC_TST)/*), $(wildcard $(SRC)/*.c) $(wildcard $(SRC)/**/*.c)))
SOURCES_D := $(filter-out $(wildcard $(SRC)/$(CLIENT).c),$(filter-out $(wildcard $(SRC_TST)/*), $(wildcard $(SRC)/*.c) $(wildcard $(SRC)/**/*.c)))
SOURCES_TST := $(wildcard $(SRC_TST)/*)


DAEMON_BIN=$(BIN)/$(DAEMON)
CLIENT_BIN=$(BIN)/$(CLIENT)

TST_NAME=$(BIN)/reportmandtst

OBJECTS_C := $(filter-out $(wildcard $(OBJ)/$(DAEMON).o),$(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES_C)))
OBJECTS_D := $(filter-out $(wildcard $(OBJ)/$(CLIENT).o),$(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES_D)))

OBJECTS_TST := $(patsubst $(SRC_TST)/%.c, $(OBJ_TST)/%.o, $(SOURCES_TST))


all: $(DAEMON_BIN) $(CLIENT_BIN)
test: $(TST_NAME)


$(DAEMON_BIN): $(OBJECTS_D)
	@mkdir -p $(@D)
	$(CC) $^ -o $@ $(LIBS)

$(OBJ)/$(DAEMON).o: $(SRC)/$(DAEMON).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@

$(CLIENT_BIN) : $(OBJECTS_C)
	@mkdir -p $(@D)
	$(CC) $^ -o $@ $(LIBS)

$(OBJ)/$(CLIENT).o: $(SRC)/$(CLIENT).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@

$(OBJ)/%.o: $(SRC)/%.c $(SRC)/%.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@ 

install: $(DAEMON_BIN) | $(CLIENT_BIN)
	@if [ "$(shell id -u)" != "0" ]; then\
		$(warning  This script must be run as root to install to $(SBIN), and add service to systemctl)echo;\
	fi
	@sudo mkdir -p $(LOG_DIR)
	
	@sudo groupadd afnbadmin || true
	@sudo useradd $(NAME) -s /sbin/nologin -r -M -d / || true
	
	@sudo chown -R $(NAME):afnbadmin $(LOG_DIR)
	@sudo chmod 640 $(LOG_DIR)

	@sudo install -m 744 $(DAEMON_BIN) $(SBIN)/
	@sudo setcap 'CAP_WAKE_ALARM' $(SBIN)/$(DAEMON)
	@sudo install -m 744 $(CLIENT_BIN) $(USRBIN)/

	@sudo cp $(NAME).service /etc/systemd/system/

	@sudo systemctl daemon-reload
	@sudo systemctl enable $(NAME).service


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