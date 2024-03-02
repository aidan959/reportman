sudo setcap 'CAP_WAKE_ALARM=ep' bin/reportmand
sudo setcap 'CAP_WAKE_ALARM=ep' obj/daemonize.o
sudo setcap 'CAP_WAKE_ALARM=ep' obj/directory_tool.o
sudo setcap 'CAP_WAKE_ALARM=ep' obj/monitor_tool.o
sudo setcap 'CAP_WAKE_ALARM=ep' obj/backup_tool.o
sudo setcap 'CAP_WAKE_ALARM=ep' obj/cto_daemon.o


