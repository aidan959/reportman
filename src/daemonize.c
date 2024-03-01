#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <stdbool.h>
#include "daemonize.h"
// Turns process into daemon.
void become_daemon(bool use_flags)
{
    switch (fork())
    {
    case -1:
        exit(EXIT_FAILURE);
    case 0:
        break;
    default:
        exit(EXIT_SUCCESS);
    }

    if (setsid() == -1)
        exit(EXIT_FAILURE); // run in new session without a parent spawn

    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    switch (fork())
    {
    case -1:
        exit(EXIT_FAILURE);
    case 0:
        break;
    default:
        exit(EXIT_SUCCESS);
    }

    umask(0);
    
    // moves to root
    chdir("/");
    if(!(use_flags & D_NO_FILE_CLOSE))  // close all open files
    {
        int maxfd = sysconf(_SC_OPEN_MAX);

        if(maxfd == -1)
            maxfd = 8192;

        for(int fd = 0; fd < maxfd; fd++)
            close(fd);
    }


    // Open log file
    openlog("cto_daemon", LOG_PID, LOG_DAEMON);
}
