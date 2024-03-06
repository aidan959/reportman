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

/* Enumerate list of FDs to poll */
enum
{
    FD_POLL_SIGNAL = 0,
    FD_POLL_INOTIFY = 1,
    FD_POLL_MAX = 2
};



#define IN_EVENT_DATA_LEN (sizeof(struct inotify_event))
#define IN_EVENT_NEXT(event, length) ((length) -= (event)->len, (struct inotify_event *)(((char *)(event)) +  (event)->len))
#define IN_EVENT_OK(event, length) ((long)(length) >= (long)IN_EVENT_DATA_LEN && (long)(event)->len >= (long)IN_EVENT_DATA_LEN &&  (long)(event)->len <= (long)(length))

#define M_LOG_FILE_CREATE_ERR -1
#define M_LOG_FILE_CREATED 2
#define M_LOG_FILE_EXISTED 1


int monitor_paths(size_t num_paths, const char* const*dirs, monitor_conf_t monitor_conf );