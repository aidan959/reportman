#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <errno.h>
#include <sys/signalfd.h>
#include <sys/inotify.h>
#include "reportman.h"
/* Size of buffer to use when reading inotify events */
#define INOTIFY_BUFFER_SIZE 8192

// Enumerate list of FDs to poll
enum
{
    FM_FD_POLL_SIGNAL = 0,
    MT_FD_POLL_INOTIFY = 1,
    FM_FD_POLL_PARENT = 2,
    FM_FD_POLL_MAX = 3
};



#define IN_EVENT_DATA_LEN (sizeof(struct inotify_event))
#define IN_EVENT_NEXT(event, length) ((length) -= (event)->len, (struct inotify_event *)(((char *)(event)) +  (event)->len))
#define IN_EVENT_OK(event, length) ((long)(length) >= (long)IN_EVENT_DATA_LEN && (long)(event)->len >= (long)IN_EVENT_DATA_LEN &&  (long)(event)->len <= (long)(length))

#define M_LOG_FILE_CREATE_ERR -1
#define M_LOG_FILE_CREATED 2
#define M_LOG_FILE_EXISTED 1


int monitor_paths(size_t num_paths, const char* const*dirs, monitor_conf_t monitor_conf );
void mt_event_process(struct inotify_event *event, monitor_t *monitor);
void mt_shutdown_inotify(int inotify_fd, monitor_t *monitor);
void mt_shutdown_signals(int signal_fd, ipc_pipes_t *pipes);
int mt_initialize_signals(void);
int mt_initialize_inotify(size_t num_paths, const char *const*paths, monitor_t *monitor);
void mt_update_monitor_conf(monitor_conf_t * monitor_conf);
int mt_initialize_log_file(void);
int mt_close_log_file(void);