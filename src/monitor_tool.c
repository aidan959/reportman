#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <errno.h>
#include <sys/signalfd.h>
#include <sys/stat.h>

#include <sys/inotify.h>
#include "monitor_tool.h"

const char * LOG_FOLDER = "/var/log/reportmand/";
const char * LOG_FILE = "monitor.log";

unsigned int mon_t_event_mask =
    (IN_ACCESS |        // File was accessed
     IN_ATTRIB |        // File's metadata was changed
     IN_MODIFY |        // File data was modified.
     IN_CREATE |        // File created in directory
     IN_DELETE |        // File deleted in directory
     IN_OPEN |          // File was openend
     IN_CLOSE_WRITE |   // Writable file closed
     IN_CLOSE_NOWRITE | // Unwritable file closed
     IN_DELETE_SELF |   // Self was deleted
     IN_MOVE_SELF |     // Self was moved
     IN_MOVED_FROM |    // File moved from directory
     IN_MOVED_TO);      // File moved to directory

static void
__event_process(struct inotify_event *event, monitor_t *monitor)
{
    int i;
    
    dir_monitored_t *monitors = monitor->monitors;
    int n_monitors = monitor->no_monitors;
    // loop all registered monitors to handle the event wd
    // TODO Use some other data structure which allows querying for the WD
    for (i = 0; i < n_monitors; ++i)
    {

        if (monitors[i].wd != event->wd)
            continue;
        // struct stat_t file_stat;
        // stat();
        // if(stat_t < 0) {
            
        // }
        if (event->len > 0)
            syslog(LOG_NOTICE,"Received event in '%s/%s': ",
                   monitors[i].path,
                   event->name);
        else
            syslog(LOG_NOTICE,"Received event in '%s': ",
                   monitors[i].path);

        if (event->mask & IN_ACCESS)
            syslog(LOG_NOTICE,"\tIN_ACCESS\n");
        if (event->mask & IN_ATTRIB)
            syslog(LOG_NOTICE,"\tIN_ATTRIB\n");
        if (event->mask & IN_OPEN)
            syslog(LOG_NOTICE,"\tIN_OPEN\n");
        if (event->mask & IN_CLOSE_WRITE)
            syslog(LOG_NOTICE,"\tIN_CLOSE_WRITE\n");
        if (event->mask & IN_CLOSE_NOWRITE)
            syslog(LOG_NOTICE,"\tIN_CLOSE_NOWRITE\n");
        if (event->mask & IN_CREATE)
            syslog(LOG_NOTICE,"\tIN_CREATE\n");
        if (event->mask & IN_DELETE)
            syslog(LOG_NOTICE,"\tIN_DELETE\n");
        if (event->mask & IN_DELETE_SELF)
            syslog(LOG_NOTICE,"\tIN_DELETE_SELF\n");
        if (event->mask & IN_MODIFY)
            syslog(LOG_NOTICE,"\tIN_MODIFY\n");
        if (event->mask & IN_MOVE_SELF)
            syslog(LOG_NOTICE,"\tIN_MOVE_SELF\n");
        if (event->mask & IN_MOVED_FROM)
            syslog(LOG_NOTICE,"\tIN_MOVED_FROM (cookie: %d)\n",
                   event->cookie);
        if (event->mask & IN_MOVED_TO)
            syslog(LOG_NOTICE,"\tIN_MOVED_TO (cookie: %d)\n",
                   event->cookie);
        fflush(stdout);
        return;
    }
}

static void
__shutdown_inotify(int inotify_fd, monitor_t *monitor)
{
    int i;
    dir_monitored_t *monitors = monitor->monitors;
    int n_monitors = monitor->no_monitors;
    for (i = 0; i < n_monitors; ++i)
    {
        free(monitors[i].path);
        inotify_rm_watch(inotify_fd, monitors[i].wd);
    }
    free(monitors);
    close(inotify_fd);
}

static int
__initialize_inotify(unsigned int num_paths,
                     const char **paths, monitor_t *monitor)
{
    
    int inotify_fd;

    // create a notifier
    if ((inotify_fd = inotify_init()) < 0)
    {
        syslog(LOG_ERR, "Couldn't setup new inotify device: '%s'\n",
               strerror(errno));
        return -1;
    }

    // create directory monitors
    monitor->no_monitors = (int) num_paths;
    monitor->monitors = malloc(num_paths * sizeof(dir_monitored_t));

    // add monitor for each directory to watch
    for (unsigned int i = 0; i < num_paths; ++i)
    {
        monitor->monitors[i].path = strdup(paths[i]);
        if ((monitor->monitors[i].wd = inotify_add_watch(inotify_fd,
                                                         monitor->monitors[i].path,
                                                         mon_t_event_mask)) < 0)
        {
            syslog(LOG_CRIT, "Couldn't add monitor in directory '%s': '%s'\n",
                   monitor->monitors[i].path,
                   strerror(errno));
            exit(EXIT_FAILURE);
        }
        syslog(LOG_NOTICE, "Started monitoring directory '%s'...\n",
               monitor->monitors[i].path);
    }

    return inotify_fd;
}

static void __shutdown_signals(int signal_fd)
{
    close(signal_fd);
}

static int __initialize_signals(void)
{
    int signal_fd;
    sigset_t sigmask;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGINT);
    sigaddset(&sigmask, SIGTERM);

    if (sigprocmask(SIG_BLOCK, &sigmask, NULL) < 0)
    {
        syslog(LOG_ERR,
               "Couldn't block signals: '%s'\n",
               strerror(errno));
        return -1;
    }

    // get new file descriptor for signals
    if ((signal_fd = signalfd(-1, &sigmask, 0)) < 0)
    {
        syslog(LOG_ERR,
               "Couldn't setup signal FD: '%s'\n",
               strerror(errno));
        return -1;
    }

    return signal_fd;
}

int monitor_paths(unsigned int num_paths,
                  const char **dirs)
{
    int signal_fd;
    int inotify_fd;
    struct pollfd fds[FD_POLL_MAX];
    monitor_t monitor = {};
    if (num_paths < 1)
    {
        syslog(LOG_WARNING, "At least one directory SHOULD be passed to monitor_paths.");
        exit(EXIT_FAILURE);
    }

    // TODO - may not need to call  initalize signals
    if ((signal_fd = __initialize_signals()) < 0)
    {
        syslog(LOG_ALERT, "Could not initialize signals");
        exit(EXIT_FAILURE);
    }

    // initialize file descriptors
    if ((inotify_fd = __initialize_inotify(num_paths, dirs, &monitor)) < 0)
    {
        syslog(LOG_CRIT, "Could not initialize inotify.");
        exit(EXIT_FAILURE);
    }

    // enable polling
    fds[FD_POLL_SIGNAL].fd = signal_fd;
    fds[FD_POLL_SIGNAL].events = POLLIN;
    fds[FD_POLL_INOTIFY].fd = inotify_fd;
    fds[FD_POLL_INOTIFY].events = POLLIN;
    for (;;)
    {
        // blocks to read for file changes
        if (poll(fds, FD_POLL_MAX, -1) < 0)
        {
            syslog(LOG_ERR,
                   "Couldn't poll(): '%s'\n",
                   strerror(errno));
            exit(EXIT_FAILURE);
        }

        // FDs was populated / there are events to process
        if (fds[FD_POLL_SIGNAL].revents & POLLIN)
        {
            struct signalfd_siginfo fdsi;

            // signal size received from read was incorrect
            ssize_t read_size;
            if ((read_size = read(fds[FD_POLL_SIGNAL].fd, &fdsi, sizeof(fdsi))) != sizeof(fdsi))
            {
                syslog(LOG_CRIT,
                       "Couldn't read signal, wrong size read(fsdi '%ld' != read() '%ld')",
                       sizeof(fdsi),
                       read_size);
                exit(EXIT_FAILURE);
            }

            if (fdsi.ssi_signo == SIGINT ||
                fdsi.ssi_signo == SIGTERM)
            {
                break;
            }

            syslog(LOG_CRIT,
                   "Received unexpected signal ? ");
        }

        // INOTIFY event received
        if (fds[FD_POLL_INOTIFY].revents & POLLIN)
        {
            char buffer[INOTIFY_BUFFER_SIZE];
            ssize_t length;

            // reads all events we can fit in the buffer
            if ((length = read(fds[FD_POLL_INOTIFY].fd,
                               buffer,
                               INOTIFY_BUFFER_SIZE)) > 0)
            {
                struct inotify_event *event;

                event = (struct inotify_event *)buffer;
                while (IN_EVENT_OK(event, length))
                {
                    __event_process(event, &monitor);
                    event = IN_EVENT_NEXT(event, length);
                }
            }
        }
    }

    // shuts down inotfiy
    __shutdown_inotify(inotify_fd, &monitor);
    // shutdown signals
    __shutdown_signals(signal_fd);

    return EXIT_SUCCESS;
}