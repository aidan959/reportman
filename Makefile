CC=/usr/sbin/gcc
CFLAGS=-Wextra -Wall -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code

SRC=src
OBJ=obj
BIN=bin


MON_T=monitor_tool
DIR_T=directory_tool
DAE=daemonize
MAIN=cto_daemon

SOURCES=$(MAIN).c $(MON_T).c $(DIR_T).c $(DAE).c
OBJECTS=$(addprefix $(OBJ)/, $(SOURCES:.c=.o))

PROG_NAME=$(BIN)/reportmand

all: $(PROG_NAME)

$(PROG_NAME): $(OBJECTS)
	$(CC) $^ -o $@
	

obj/$(MAIN).o: $(SRC)/$(MAIN).c
	$(CC) $(CFLAGS) $< -o $@

obj/%.o: $(SRC)/%.c $(SRC)/%.h
	$(CC) $(CFLAGS) $< -o $@

run:
	./bin/$(PROG_NAME)

clean:
	rm *.o -R ./bin