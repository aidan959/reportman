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
#include <linux/limits.h>


#include <sys/inotify.h>
#include "include/monitor_tool.h"
#include <bits/sigaction.h>
#define M_LOG_BUFFER_SIZE 1024
#define M_DATE_STRING_SIZE 32

const char * LOG_PATH = M_LOG_PATH;

static const char * __get_log_message(struct inotify_event *event);
static void __log_event(char * event_file_message, char * event_log_message);
static bool __remove_new_line(char * str);
static void __get_current_date_time(char * buffer, size_t buffer_size);
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
    .log_file = M_LOG_PATH,
    .log_sys_name = "reportmand_monitor",
    .log_to_sys = false,
    .log_to_file = true,
};

static FILE* __log_file_fd = NULL;

void mt_event_process(struct inotify_event *event, monitor_t *monitor)
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
        char time_string[M_DATE_STRING_SIZE];
        __get_current_date_time(time_string, sizeof(time_string));
        if (event->len > 0){
            // TODO GET USER NAME OF FILE
            snprintf(event_log_message,M_LOG_BUFFER_SIZE,"Received event in '%s/%s': %s by %s",
                   monitors[i].path,
                   event->name,
                   __get_log_message(event),
                   "UNIMPLEMENTED"
            );
            snprintf(event_file_message,M_LOG_BUFFER_SIZE,"%s by [%s] @ [%s]: %s/%s",
                   __get_log_message(event),
                   "UNIMPLEMENTED",
                    time_string,
                   monitors[i].path,
                   event->name
            );
        }
        else
        // TODO GET USER NAME OF FILE
        {
            snprintf(event_log_message, M_LOG_BUFFER_SIZE,
                "Received event in '%s/%s': %s by %s",
                monitors[i].path,
                event->name,
                __get_log_message(event),
                "UNIMPLEMENTED"
            );
            snprintf(event_file_message, M_LOG_BUFFER_SIZE,
                "%s by [%s] @ %s: %s/%s",
                __get_log_message(event),
                time_string,
                "UNIMPLEMENTED",
                monitors[i].path,
                event->name
            );
        }

        __log_event(event_file_message, event_log_message);
        return;
    }
}

typedef struct {
    uint32_t mask;
    const char* message;
} event_mask_t;



event_mask_t event_masks[] = {
    {IN_ACCESS, "IN_ACCESS"},
    {IN_ATTRIB, "IN_ATTRIB"},
    {IN_OPEN, "IN_OPEN"},
    {IN_CLOSE_WRITE, "IN_CLOSE_WRITE"},
    {IN_CLOSE_NOWRITE, "IN_CLOSE_NOWRITE"},
    {IN_CREATE, "IN_CREATE"},
    {IN_DELETE, "IN_DELETE"},
    {IN_DELETE_SELF, "IN_DELETE_SELF"},
    {IN_MODIFY, "IN_MODIFY"},
    {IN_MOVE_SELF, "IN_MOVE_SELF"},
    {IN_MOVED_FROM, "IN_MOVED_FROM"},
    {IN_MOVED_TO, "IN_MOVED_TO"},
};
static const char * __get_log_message(struct inotify_event *event) {
    for (long unsigned int i = 0; i < sizeof(event_masks) / sizeof(event_mask_t); i++) {
        if (!(event->mask & event_masks[i].mask)) continue ;

        return event_masks[i].message;
    }
    return "UNKNOWN";
}

void mt_shutdown_inotify(int inotify_fd, monitor_t *monitor)
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

int mt_initialize_inotify(size_t num_paths, const char *const*paths, monitor_t *monitor)
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

void mt_shutdown_signals(int signal_fd, ipc_pipes_t *pipes)
{
    close(pipes->read);
    close(pipes->write);

    close(signal_fd);
}



int mt_initialize_log_file(void) {
    FILE *file;

    file = fopen(__monitor_conf.log_file, "r");

    if (file == NULL) {
        syslog(LOG_NOTICE, "Log file does not exist, creating... (%s)", __monitor_conf.log_file);
        file = fopen(__monitor_conf.log_file, "w");
        if (file == NULL) {
            syslog(LOG_ERR,"Error creating log file (%s): %s", __monitor_conf.log_file,  strerror(errno));
            return M_LOG_FILE_CREATE_ERR;
        }
        syslog(LOG_NOTICE, "Log file (%s) created", __monitor_conf.log_file);
        __log_file_fd = file;
        return M_LOG_FILE_CREATED;
    }
    close(fileno(file));
    // repon as append
    file = fopen(__monitor_conf.log_file, "a");
    syslog(LOG_NOTICE, "Using existing log file %s", __monitor_conf.log_file);
    __log_file_fd = file;
    return M_LOG_FILE_EXISTED;
}
int mt_close_log_file(void) {
    if(__log_file_fd  == NULL) return D_FAILURE;
    return fclose(__log_file_fd);
}
void mt_update_monitor_conf(monitor_conf_t * monitor_conf) {
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
    int fflush_resonse = fflush(__log_file_fd);
    if(fflush_resonse < 0){
        syslog(LOG_ERR, "fflush out of range: %s", strerror(errno));
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


static void __get_current_date_time(char * buffer, size_t buffer_size) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(buffer, buffer_size, "%d-%d-%d %d:%d:%d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}