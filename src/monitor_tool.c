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
#include "include/monitor_tool.h"
#include <bits/sigaction.h>
#define M_LOG_BUFFER_SIZE 1024
const char * LOG_PATH = M_LOG_PATH;

static const char * __get_log_message(struct inotify_event *event);
static void __event_process(struct inotify_event *event, monitor_t *monitor);
static void __shutdown_inotify(int inotify_fd, monitor_t *monitor);
static void __shutdown_signals(int signal_fd);
static int __initialize_signals(void);
static int __initialize_inotify(size_t num_paths, const char *const*paths, monitor_t *monitor);
static void __update_monitor_conf(monitor_conf_t * monitor_conf);
static void __log_event(char * event_file_message, char * event_log_message);
static bool __remove_new_line(char * str);
static int __initialize_log_file(void);

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

static monitor_conf_t __monitor_conf = {
    .log_file = "/var/log/reportmand/monitor.log",
    .log_sys_name = "reportmand_monitor",
    .log_to_sys = false,
    .log_to_file = true,
};

static FILE* __log_file_fd = NULL;

int monitor_paths(size_t num_paths, const char*const*dirs, monitor_conf_t monitor_conf)
{
    int signal_fd;
    int inotify_fd;
    struct pollfd fds[FD_POLL_MAX];
    monitor_t monitor = {};
    __initialize_log_file();
    __update_monitor_conf(&monitor_conf);

    if (num_paths < 1)
    {
        syslog(LOG_ERR, "At least one directory SHOULD be passed to monitor_paths.");
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

    __shutdown_inotify(inotify_fd, &monitor);

    __shutdown_signals(signal_fd);

    return EXIT_SUCCESS;
}

static void __event_process(struct inotify_event *event, monitor_t *monitor)
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

        char event_log_message[M_LOG_BUFFER_SIZE];
        char event_file_message[M_LOG_BUFFER_SIZE];

        if (event->len > 0)
            // TODO GET USER NAME OF FILE
            snprintf(event_log_message,M_LOG_BUFFER_SIZE,"Received event in '%s/%s': %s by %s",
                   monitors[i].path,
                   event->name,
                   __get_log_message(event),
                   "UNIMPLEMENTED"
                   );
        else
                   // TODO GET USER NAME OF FILE
            snprintf(event_log_message, M_LOG_BUFFER_SIZE,"Received event in '%s/%s': %s by %s",
                   monitors[i].path,
                   event->name,
                   __get_log_message(event),
                   "UNIMPLEMENTED"
                   );

        __log_event(event_file_message, event_log_message);
        return;
    }
}
static const char * __get_log_message(struct inotify_event *event) {
    if (event->mask & IN_ACCESS)
        return "IN_ACCESS";
    if (event->mask & IN_ATTRIB)
        return "IN_ATTRIB";
    if (event->mask & IN_OPEN)
        return "IN_OPEN";
    if (event->mask & IN_CLOSE_WRITE)
        return "IN_CLOSE_WRITE";
    if (event->mask & IN_CLOSE_NOWRITE)
        return "IN_CLOSE_NOWRITE";
    if (event->mask & IN_CREATE)
        return "IN_CREATE";
    if (event->mask & IN_DELETE)
        return "IN_DELETE";
    if (event->mask & IN_DELETE_SELF)
        return "IN_DELETE_SELF";
    if (event->mask & IN_MODIFY)
        return "IN_MODIFY";
    if (event->mask & IN_MOVE_SELF)
        return "IN_MOVE_SELF";
    if (event->mask & IN_MOVED_FROM)
        return "IN_MOVED_FROM";
    if (event->mask & IN_MOVED_TO)
        return "IN_MOVED_TO";
    return "UNDEFINED";
}
static void __shutdown_inotify(int inotify_fd, monitor_t *monitor)
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

static int __initialize_inotify(size_t num_paths, const char *const*paths, monitor_t *monitor)
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

static int __initialize_log_file(void) {
    FILE *file;

    file = fopen(LOG_PATH, "r");

    if (file == NULL) {
        syslog(LOG_NOTICE, "Log file does not exist, creating... (%s)", __monitor_conf.log_file);
        file = fopen(__monitor_conf.log_file, "w");
        if (file == NULL) {
            syslog(LOG_ERR,"Error creating log file");
            return M_LOG_FILE_CREATE_ERR;
        }
        syslog(LOG_NOTICE, "Log file (%s) created", __monitor_conf.log_file);
        __log_file_fd = file;
        return M_LOG_FILE_CREATED;
    }
    close(fileno(file));
    // repon as writable
    file = fopen(__monitor_conf.log_file, "w");
    syslog(LOG_NOTICE, "Using existing log file %s", __monitor_conf.log_file);
    __log_file_fd = file;
    return M_LOG_FILE_EXISTED;
}

static void __update_monitor_conf(monitor_conf_t * monitor_conf) {
    if (monitor_conf->log_file != NULL)
        __monitor_conf.log_file = monitor_conf->log_file;
    
    if (monitor_conf->log_sys_name != NULL)
        __monitor_conf.log_sys_name = monitor_conf->log_sys_name;

    __monitor_conf.log_to_sys = monitor_conf->log_to_sys;
    __monitor_conf.log_to_file = monitor_conf->log_to_file;
}

static void __log_event(char * event_file_message, char * event_log_message) {
    if (__monitor_conf.log_to_sys)
        syslog(LOG_NOTICE, event_log_message);
    if (!__monitor_conf.log_to_file)
        return;
    if (__log_file_fd == NULL){
        syslog(LOG_ERR, "File descriptor for %s is not open", __monitor_conf.log_file);
    }
    __remove_new_line(event_log_message);
    int fprintf_response = fprintf(__log_file_fd, "%s\n",event_file_message);
    if (fprintf_response < 0){
        syslog(LOG_ERR, "fprintf out of range: %s", strerror(errno));
    }  
}

static bool __remove_new_line(char * str) {
    size_t len = strcspn(str, "\n");

    if(str[len] == '\n'){
        str[len] = '\0';
        return true;

    }
    return false;
}
