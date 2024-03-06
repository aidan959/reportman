#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <stdbool.h>
#include <fcntl.h>
#include "include/daemonize.h"
/// @brief 
/// @param use_flags 
/// @param singleton_sock_fd do not close the singleton socket
void become_daemon(bool use_flags, int singleton_sock_fd)
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
        exit(EXIT_FAILURE); 

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
    

    chdir("/");
    if(!(use_flags & D_NO_FILE_CLOSE))  // close all open file descriptors
    {
        
        long maxfd = sysconf(_SC_OPEN_MAX);

        if(maxfd == -1)
            maxfd = 8192;

        for(int fd = 3; fd < maxfd; fd++){
            if(fd == singleton_sock_fd)
                continue;
            close(fd);
        }
    }
}
