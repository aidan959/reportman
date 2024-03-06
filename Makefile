CC=/usr/sbin/gcc
CFLAGS=-g -Wextra -Wall -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code -c
LIBS=-lcrypto
SRC=src
INC=include
OBJ=obj
BIN=bin
SRC_TST=src/tst
OBJ_TST=obj/tst

NAME=reportman
DAEMON=$(NAME)d
CLIENT=$(NAME)c
MONITOR=$(NAME)_monitor
FM=$(NAME)_fm

LOG_DIR=/var/log/$(NAME)

EXECUTABLES = $(DAEMON) $(CLIENT) $(MONITOR) $(FM)
EXECUTABLES_SRC = $(addprefix $(SRC)/,$(addsuffix .c, $(EXECUTABLES)))
EXECUTABLES_OBJ = $(addprefix $(OBJ)/,$(addsuffix .o, $(EXECUTABLES)))

MAIN_TST=test

SBIN = /usr/sbin
USRBIN = /usr/bin



SOURCES_C := $(filter-out $(EXECUTABLES_SRC),$(filter-out $(wildcard $(SRC_TST)/*), $(wildcard $(SRC)/*.c) $(wildcard $(SRC)/**/*.c))) $(SRC)/$(CLIENT).c
SOURCES_D := $(filter-out $(EXECUTABLES_SRC),$(filter-out $(wildcard $(SRC_TST)/*), $(wildcard $(SRC)/*.c) $(wildcard $(SRC)/**/*.c))) $(SRC)/$(DAEMON).c
SOURCES_MON := $(filter-out $(EXECUTABLES_SRC),$(filter-out $(wildcard $(SRC_TST)/*), $(wildcard $(SRC)/*.c) $(wildcard $(SRC)/**/*.c))) $(SRC)/$(MONITOR).c
SOURCES_FM := $(filter-out $(EXECUTABLES_SRC),$(filter-out $(wildcard $(SRC_TST)/*), $(wildcard $(SRC)/*.c) $(wildcard $(SRC)/**/*.c))) $(SRC)/$(FM).c


SOURCES_TST := $(wildcard $(SRC_TST)/*)

DAEMON_BIN=$(BIN)/$(DAEMON)
CLIENT_BIN=$(BIN)/$(CLIENT)
MONITOR_BIN=$(BIN)/$(MONITOR)
FM_BIN=$(BIN)/$(FM)

TST_NAME=$(BIN)/reportmandtst

OBJECTS_C := $(filter-out $(EXECUTABLES_OBJ),$(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES_C))) $(OBJ)/$(CLIENT).o
OBJECTS_D := $(filter-out $(EXECUTABLES_OBJ),$(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES_D))) $(OBJ)/$(DAEMON).o
OBJECTS_MON := $(filter-out $(EXECUTABLES_OBJ),$(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES_MON))) $(OBJ)/$(MONITOR).o
OBJECTS_FM := $(filter-out $(EXECUTABLES_OBJ),$(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES_FM))) $(OBJ)/$(FM).o


OBJECTS_TST := $(patsubst $(SRC_TST)/%.c, $(OBJ_TST)/%.o, $(SOURCES_TST))


all: $(DAEMON_BIN) $(CLIENT_BIN) $(MONITOR_BIN) $(FM_BIN)
test: $(TST_NAME)

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

$(OBJ)/%.o: $(SRC)/%.c $(SRC)/$(INC)/%.h
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





