CC=/usr/sbin/gcc
CFLAGS=-Wextra -Wall -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code -c
LIBS=-lcrypto

SRC=src
INC=include
OBJ=obj
BIN=bin

SRC_LIBS=$(SRC)/libs
OBJ_LIBS=$(OBJ)/libs

NAME=reportman

DAEMON=$(NAME)d
CLIENT=$(NAME)c
MONITOR=$(NAME)_monitor
FM=$(NAME)_fm

LOG_DIR=/var/log/$(NAME)

EXECUTABLES = $(DAEMON) $(CLIENT) $(MONITOR) $(FM)
EXECUTABLES_SRC = $(addprefix $(SRC)/,$(addsuffix .c, $(EXECUTABLES)))
EXECUTABLES_OBJ = $(addprefix $(OBJ)/,$(addsuffix .o, $(EXECUTABLES)))

SBIN = /usr/sbin
USRBIN = /usr/bin

SOURCES_C:=$(filter-out $(EXECUTABLES_SRC), $(wildcard $(SRC)/*.c) $(wildcard $(SRC)/**/*.c)) $(SRC)/$(CLIENT).c
SOURCES_D:=$(filter-out $(EXECUTABLES_SRC), $(wildcard $(SRC)/*.c) $(wildcard $(SRC)/**/*.c)) $(SRC)/$(DAEMON).c
SOURCES_MON:=$(filter-out $(EXECUTABLES_SRC), $(wildcard $(SRC)/*.c) $(wildcard $(SRC)/**/*.c)) $(SRC)/$(MONITOR).c
SOURCES_FM:=$(filter-out $(EXECUTABLES_SRC), $(wildcard $(SRC)/*.c) $(wildcard $(SRC)/**/*.c)) $(SRC)/$(FM).c

DAEMON_BIN=$(BIN)/$(DAEMON)
CLIENT_BIN=$(BIN)/$(CLIENT)
MONITOR_BIN=$(BIN)/$(MONITOR)
FM_BIN=$(BIN)/$(FM)

EXECUTABLES = $(DAEMON_BIN) $(CLIENT_BIN) $(MONITOR_BIN) $(FM_BIN)

OBJECTS_C := $(filter-out $(EXECUTABLES_OBJ),$(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES_C))) $(OBJ)/$(CLIENT).o
OBJECTS_D := $(filter-out $(EXECUTABLES_OBJ),$(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES_D))) $(OBJ)/$(DAEMON).o
OBJECTS_MON := $(filter-out $(EXECUTABLES_OBJ),$(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES_MON))) $(OBJ)/$(MONITOR).o
OBJECTS_FM := $(filter-out $(EXECUTABLES_OBJ),$(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES_FM))) $(OBJ)/$(FM).o

all: $(EXECUTABLES)

force: clean all

debug: CFLAGS += -g
debug: force

# DAEMON MAKE TARGETS
$(DAEMON_BIN): $(OBJECTS_D)
	@mkdir -p $(@D)
	@echo $(CC) $^ -o $@ $(LIBS)

	$(CC) $^ -o $@ $(LIBS)

$(OBJ)/$(DAEMON).o: $(SRC)/$(DAEMON).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@

# CLIENT MAKE TARGETS
$(CLIENT_BIN) : $(OBJECTS_C)
	@mkdir -p $(@D)
	$(CC) $^ -o $@ $(LIBS)

$(OBJ)/$(CLIENT).o: $(SRC)/$(CLIENT).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@

# MONITOR MAKE TARGETS
$(MONITOR_BIN) : $(OBJECTS_MON)
	@mkdir -p $(@D)
	$(CC) $^ -o $@ $(LIBS)

$(OBJ)/$(MONITOR).o: $(SRC)/$(MONITOR).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@

# FM MAKE TARGETS
$(FM_BIN) : $(OBJECTS_FM)
	@mkdir -p $(@D)
	$(CC) $^ -o $@ $(LIBS)

$(OBJ)/$(FM).o: $(SRC)/$(FM).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@

# LIBRARY MAKE TARGETS
$(OBJ_LIBS)/%.o: $(SRC_LIBS)/%.c $(SRC_LIBS)/$(INC)/%.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@ 

SERVICE_NAME = $(NAME).service

install: $(DAEMON_BIN) | $(CLIENT_BIN)
	@if [ "$(shell id -u)" != "0" ]; then\
		$(warning  This script must be run as root to install to $(SBIN), and add service to systemctl)echo;\
	fi
	@sudo mkdir -p $(LOG_DIR)
	@sudo mkdir -p /srv/$(NAME)

	
	@sudo groupadd afnbadmin || true
	@sudo useradd $(NAME) -s /sbin/nologin -r -M -d / || true
	@echo "sudo chown -R $(NAME):afnbadmin /srv/$(NAME)"
	@sudo chown -R $(NAME):afnbadmin $(LOG_DIR)
	@sudo chown -R $(NAME):afnbadmin /srv/$(NAME)

	@sudo chmod 640 $(LOG_DIR)

	install -o $(NAME) -m 754 $(DAEMON_BIN) $(SBIN)
	install -o $(NAME) -m 755 $(CLIENT_BIN) $(USRBIN)
	install -o $(NAME) -m 755 $(MONITOR_BIN) $(USRBIN)
	install -o $(NAME) -m 755 $(FM_BIN) $(USRBIN)

	cp $(NAME).service /etc/systemd/system/

	@sudo systemctl daemon-reload
	@sudo systemctl enable $(NAME).service

run: | $(DAEMON_BIN)
	./$(DAEMON_BIN)

clean:
	rm -R ./bin ./obj || true
