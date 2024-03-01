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
#include <sys/inotify.h>
/* Contains path and  */
typedef struct
{
    char *path;
    int wd;
} dir_monitored_t;

/* Contains path and  */
typedef struct
{
    dir_monitored_t *monitors;
    int no_monitors;
} monitor_t;


/* Size of buffer to use when reading inotify events */
#define INOTIFY_BUFFER_SIZE 8192

/* Enumerate list of FDs to poll */
enum
{
    FD_POLL_SIGNAL = 0,
    FD_POLL_INOTIFY = 1,
    FD_POLL_MAX = 2
};

/* FANotify-like helpers to iterate events */
#define IN_EVENT_DATA_LEN (sizeof(struct inotify_event))
#define IN_EVENT_NEXT(event, length)              \
    ((length) -= (event)->len,                    \
     (struct inotify_event *)(((char *)(event)) + \
                              (event)->len))
#define IN_EVENT_OK(event, length)                    \
    ((long)(length) >= (long)IN_EVENT_DATA_LEN &&     \
     (long)(event)->len >= (long)IN_EVENT_DATA_LEN && \
     (long)(event)->len <= (long)(length))

/* Setup inotify notifications (IN) mask. All these defined in inotify.h. */
static int event_mask =
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


static void __event_process(struct inotify_event *event, monitor_t *monitor);
static void __shutdown_inotify(int inotify_fd, monitor_t *monitor);
static int __initialize_inotify(int num_paths, const char **paths, monitor_t *monitor);
static void __shutdown_signals(int signal_fd);
static int __initialize_signals(void);
int monitor_paths(int num_paths, const char **dirs);