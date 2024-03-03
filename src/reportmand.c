#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdbool.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "daemonize.h"
#include "directory_tool.h"
#include "monitor_tool.h"
#include "reportmand.h"

const char REPORTS_DIRECTORY[] = "/srv/allfactnobreak/reports";
const char BACKUP_DIRECTORY[] = "/srv/allfactnobreak/backup";
const char DASHBOARD_DIRECTORY[] = "/srv/allfactnobreak/dashboard";

int main(int argc, char *argv[])
{
    execution_arguments_t exec_args = {.make_daemon = true, .daemon_port = REPORTMAND_BIND_PORT};

    configure_daemon_args(argc, argv, &exec_args);

    int d_socket;
    
    d_acquire_singleton(&d_socket, exec_args.daemon_port);

    const char *dirs[] = {
        REPORTS_DIRECTORY,
        BACKUP_DIRECTORY,
        DASHBOARD_DIRECTORY
    };

    if (exec_args.make_daemon)
        become_daemon(0);

    init_directories( sizeof(dirs) / sizeof(char *), dirs);
    time_t back_up_time = time(NULL) + 5;
    time_t transfer_time = time(NULL) + 5;

    time_t queue_interval = 100;

    if(transfer_at_time(REPORTS_DIRECTORY, DASHBOARD_DIRECTORY, transfer_time, queue_interval, TRANSFER)
         == (void *)UNIMPLEMENTED_TRANSFER_METHOD ) {
        syslog(LOG_ERR, "Unimplemented transfer method used.");
    }
    if(transfer_at_time(REPORTS_DIRECTORY, DASHBOARD_DIRECTORY, back_up_time, queue_interval, BACKUP)
         == (void *)UNIMPLEMENTED_TRANSFER_METHOD ) {
        syslog(LOG_ERR, "Unimplemented transfer method used.");
    }
    bool is_daemon;
    switch (fork())
    {
    case -1:
        syslog(LOG_ERR, "Could not create file monitor child process.");
        break;
    case 0:
        is_daemon = false;
        int monitor_exit;
        if ((monitor_exit = monitor_paths(sizeof(dirs) / sizeof(char *), dirs)) < 0)
        {
            syslog(LOG_ERR, "Path monitor exited unsuccessfully with value (%d)", monitor_exit);
        }
        else
        {
            syslog(LOG_NOTICE, "Path monitor exited successfully (%d).", monitor_exit);
        }
        while(1) {
            sleep(1);
        }   
        return EXIT_SUCCESS;


    default:
        is_daemon = true;

        break;
    }

    // Server loop (accepting and handling clients)
    while (is_daemon) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_fd = accept(d_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd == -1) {
            perror("Accept failed");
            continue;
        }

        // At this point, you can communicate with the client using client_fd
        // For example, using read() and write() functions

        close(client_fd);  // Close the client socket
    }

    // Log termination and close log file
    syslog(LOG_NOTICE, "reportmand exiting");
    closelog();

    return EXIT_SUCCESS;
}

