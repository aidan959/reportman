#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <stdbool.h>
#include <time.h>
#include "daemonize.h"
#include "directory_tool.h"
#include "backup_tool.h"

#include "monitor_tool.h"
#include "cto_daemon.h"



void process_args(int argc, char *argv[], execution_arguments_t *args);

int main(int argc, char *argv[])
{
    execution_arguments_t args = {.make_daemon = true};

    process_args(argc, argv, &args);

    const char *dirs[] = {
        REPORTS_DIRECTORY,
        BACKUP_DIRECTORY,
        DASHBOARD_DIRECTORY
    };

    if (args.make_daemon)
        become_daemon(0);

    init_directories( sizeof(dirs) / sizeof(char *), dirs);
    time_t queue_time = time(NULL) + 5;
    time_t queue_interval = 100;

    backup_at_time(REPORTS_DIRECTORY, DASHBOARD_DIRECTORY, queue_time, queue_interval );

    bool is_daemon;
    switch (fork())
    {
    case -1:
        syslog(LOG_ERR, "Could not create file child process.");
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
        break;

    default:
        is_daemon = true;

        break;
    }

    while (is_daemon)
    {
        syslog(LOG_NOTICE, "cto daemon started");
        sleep(20);
        break;
    }

    // Log termination and close log file
    syslog(LOG_NOTICE, "cto daemon closed");
    closelog();

    return EXIT_SUCCESS;
}

void process_args(int argc, char *argv[], execution_arguments_t *args)
{
    int i;
    for (i = 1; i < argc; i++)
    {

        if (strcmp(argv[i], "--no-daemon") == 0)
        {
            args->make_daemon = false; /* This is used as a boolean value. */
            continue;
        }
        else
        {
            /* Process non-optional arguments here. */
        }
    }
}