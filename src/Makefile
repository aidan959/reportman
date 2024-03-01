CC="/usr/sbin/gcc"

MON_T=monitor_tool
DIR_T=directory_tool
DAE=daemonize
MAIN=cto_daemon


build: cto_daemon.c
	gcc $(MON_T).c -o $(MON_T).o -c
	gcc $(DIR_T).c -o $(DIR_T).o -c
	gcc $(DAE).c -o $(DAE).o -c

	gcc $(MAIN).c -o $(MAIN).o -c

	gcc -o $(MAIN) $(MON_T).o $(DAE).o $(DIR_T).o $(MAIN).o 
	rm $(MON_T).o $(DIR_T).o $(DAE).o $(MAIN).o

run:
	./cto_daemon

clean:
	rm cto_daemon