#include <errno.h>
#include <syslog.h>
#include <linux/limits.h>
#include "libs/include/reportman.h"
#include "libs/include/monitor_tool.h"
#include "include/reportman_mon.h"

static void __configure_monitor_args(int argc, char *argv[]);

static int __monitor_paths(void);
static int __started_from_daemon(void);

static const char *const __default_dirs[3] = {
    R_BACKUP_DIRECTORY,
    R_DASHBOARD_DIRECTORY,
    R_REPORTS_DIRECTORY
};
static monitor_args_t __monitor_args = {
    .from_daemon = false,
    .dirs = __default_dirs,
    .no_dirs = 3,
    .pipes = {
        .write = -1,
        .read = -1
    },
    .conf = {
        .log_file = M_LOG_PATH,
        .log_sys_name = "reportman_mon",
        .log_to_sys = false,
        .log_to_file = true,
        .log_to_stdout = true
        }
};

int main(int argc, char *argv[])
{
    __configure_monitor_args(argc, argv);
    __started_from_daemon();
}
static int __monitor_paths(void)
{
    int signal_fd;
    int inotify_fd;
    struct pollfd fds[FM_FD_POLL_MAX];
    monitor_t monitor = {};
    mt_initialize_log_file();

    mt_update_monitor_conf(&__monitor_args.conf);

    if (__monitor_args.no_dirs < 1)
    {
        syslog(LOG_ERR, "At least one directory should be passed to monitor_paths.");
        exit(EXIT_FAILURE);
    }

    
    if ((signal_fd = r_initialize_signals()) < 0)
    {
        syslog(LOG_ALERT, "Could not initialize signals");
        exit(EXIT_FAILURE);
    }

    char monitor_list_message[PATH_MAX * 4] = "";
    for(size_t i =0; i < __monitor_args.no_dirs; i++){
        if (__monitor_args.dirs[i] == NULL){
            syslog(LOG_ERR, "Directory provided %lu is NULL", i);
            exit(EXIT_FAILURE);
        }
        snprintf(monitor_list_message, sizeof(monitor_list_message), "Monitoring %s ", __monitor_args.dirs[i]);
        if (__monitor_args.conf.log_to_sys)
            snprintf(monitor_list_message, sizeof(monitor_list_message), "%sto syslog (%s)", monitor_list_message,__monitor_args.conf.log_sys_name);
        if (__monitor_args.conf.log_to_sys && __monitor_args.conf.log_to_file)
            snprintf(monitor_list_message, sizeof(monitor_list_message), "%s and ", monitor_list_message);
        if (__monitor_args.conf.log_to_file)
            snprintf(monitor_list_message, sizeof(monitor_list_message), "%sto log file(%s)", monitor_list_message, __monitor_args.dirs[i]);
        syslog(LOG_NOTICE, monitor_list_message);
    }
    // initialize file descriptors
    if ((inotify_fd = mt_initialize_inotify(__monitor_args.no_dirs, __monitor_args.dirs, &monitor)) < 0)
    {
        syslog(LOG_CRIT, "Could not initialize inotify.");
        exit(EXIT_FAILURE);
    }


    // enable polling
    fds[FM_FD_POLL_SIGNAL].fd = signal_fd;
    fds[FM_FD_POLL_SIGNAL].events = POLLIN;
    fds[MT_FD_POLL_INOTIFY].fd = inotify_fd;
    fds[MT_FD_POLL_INOTIFY].events = POLLIN;
    fds[FM_FD_POLL_PARENT].fd = __monitor_args.pipes.read;
    fds[FM_FD_POLL_PARENT].events = POLLIN;
    nfds_t poll_count;
    if(__monitor_args.pipes.read > 0)
        poll_count = (nfds_t)FM_FD_POLL_MAX;
    else
        poll_count = (nfds_t)FM_FD_POLL_MAX - 1; 
    for (;;)
    {
        // blocks to read for parent / signals / inotify events
        if (poll(fds, poll_count, -1) < 0)
        {
            syslog(LOG_ERR,
                   "Couldn't poll(): '%s'\n",
                   strerror(errno));
            exit(EXIT_FAILURE);
        }
        // INOTIFY event received
        if (fds[MT_FD_POLL_INOTIFY].revents & POLLIN)
        {
            char buffer[INOTIFY_BUFFER_SIZE];
            ssize_t length;

            // reads all events we can fit in the buffer
            if ((length = read(fds[MT_FD_POLL_INOTIFY].fd,
                               buffer,
                               INOTIFY_BUFFER_SIZE)) > 0)
            {
                struct inotify_event *event;

                event = (struct inotify_event *)buffer;
                while (IN_EVENT_OK(event, length))
                {
                    mt_event_process(event, &monitor);
                    event = IN_EVENT_NEXT(event, length);
                }
            }
        }
        // Received message from parent
        if(fds[FM_FD_POLL_PARENT].revents & POLLIN)
        {
            IPC_COMMANDS command;
            if(!ipc_get_command(&__monitor_args.pipes, &command)) {
                syslog(LOG_ERR, "Unexpected error reading command from daemon.");
            }
            else
            {
                switch (command) {
                case IPC_COMMAND_HEALTH_PROBE:
                    if(!ipc_send_command(&__monitor_args.pipes, IPC_COMMAND_ACK)){
                        syslog(LOG_ERR, "Failed to send ACK to daemon.");
                    };
                    break;
                case IPC_COMMAND_NO:
                case IPC_COMMAND_YES:
                case IPC_COMMAND_ACK:
                default:
                    syslog(LOG_ERR, "Received unexpected command from daemon.");
                    break;
                }
            };
        }
        // FDs was populated / there are events to process
        if (fds[FM_FD_POLL_SIGNAL].revents & POLLIN)
        {
            struct signalfd_siginfo fdsi;

            // signal size received from read was incorrect
            ssize_t read_size;
            if ((read_size = read(fds[FM_FD_POLL_SIGNAL].fd, &fdsi, sizeof(fdsi))) != sizeof(fdsi))
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
    }

    mt_shutdown_inotify(inotify_fd, &monitor);

    mt_shutdown_signals(signal_fd, &__monitor_args.pipes);

    mt_close_log_file();

    syslog(LOG_NOTICE, "reportman_mon exited successfully.");
    exit(EXIT_SUCCESS);
}


/// @brief Reads command line arguments and populates monitor_args_t
/// @param argc
/// @param argv
/// @param args
static void __configure_monitor_args(int argc, char *argv[])
{
    int i;
    bool default_dirs = true;
    size_t count = 0;
    size_t capacity = 1;
    char **string_array = malloc((capacity) * sizeof(char *));
    for (i = 1; i < argc; i++)
    {
        char arg_string[COMMUNICATION_BUFFER_SIZE];
        int arg_int[1];

        bool arg_bool[1];
        if (arg_parse_flag(argv[i], "-fd", "--from-daemon"))
        {
            __monitor_args.from_daemon = true;
            continue;
        }
        else if (arg_parse_int(argc, argv, &i, "--d-to-c", "--d-to-c", "daemon to mon read pipe id", arg_int))
        {
            __monitor_args.pipes.read = *arg_int;
            continue;
        }
        else if (arg_parse_int(argc, argv, &i, "--c-to-d", "--c-to-d", "mon to daemon write pipe id", arg_int))
        {
            __monitor_args.pipes.write = *arg_int;
            continue;
        }
        else if (arg_parse_string(argc, argv, &i, "-lf", "--log-file", "log file path", arg_string))
        {
            char *log_file = malloc(sizeof(char) * PATH_MAX);
            strcpy(log_file, arg_string);
            __monitor_args.conf.log_file = log_file;
            continue;
        }
        else if (arg_parse_string(argc, argv, &i, "-ls", "--log-sys-name", "log sys name", arg_string))
        {
            char *log_file = malloc(sizeof(char) * PATH_MAX);
            strcpy(log_file, arg_string);
            __monitor_args.conf.log_sys_name = log_file;
            continue;
        }
        else if (arg_parse_bool(argc, argv, &i, "--log-to-file", "--log-to-file", "bool", arg_bool))
        {
            __monitor_args.conf.log_to_file = *arg_bool;
            continue;
        }
        else if (arg_parse_bool(argc, argv, &i, "--log-to-sys", "--log-to-sys", "bool", arg_bool))
        {
            __monitor_args.conf.log_to_sys = *arg_bool;
            continue;
        }
        else if (arg_unrecognised(argv[i]))
        {
            syslog(LOG_ERR, "Argument '%s' is not defined", argv[i]);
            fprintf(stderr, "Argument '%s' is not defined", argv[i]);
            exit(EXIT_FAILURE);
        }
        else
        {
            default_dirs = false;

            if (count == capacity)
            {
                capacity *= 2;
                string_array = realloc(string_array, capacity * sizeof(char *));
            }

            string_array[count] = malloc(strlen(argv[i]) + 1);
            strcpy(string_array[count], argv[i]);
            count++;
        }
        if (!default_dirs)
        {
            __monitor_args.dirs = (const char *const *)string_array;
            __monitor_args.no_dirs = count;
        }
    }
}

/// @brief Runs if executed from daemon
/// @param
/// @return Discardable
static int __started_from_daemon(void)
{
    if (!__monitor_args.from_daemon)
    {
        openlog("reportman_mon", LOG_PID, LOG_USER);
        return 0;
    }
    if (__monitor_args.pipes.read < 0 || __monitor_args.pipes.write < 0)
    {
        syslog(LOG_ERR, "Pipes not received from daemon.");
        fprintf(stderr, "-fd can ONLY be called from a daemon.\n");
        return D_FAILURE;
    }
    openlog("reportman_mon", LOG_PID, LOG_DAEMON);

    syslog(LOG_NOTICE, "reportman_mon started by daemon.");
    acknowledge_daemon(&__monitor_args.pipes);

    test_data_t test_data = {
        .test_bool = false,
        .test_num = 100,
        .test_string = "blahhhhh",
    };
    ipc_send_test_data(&__monitor_args.pipes, &test_data);

    ipc_get_test_data(&__monitor_args.pipes, &test_data);
    syslog(LOG_WARNING, "%d\n%s\n%s\n", test_data.test_num, test_data.test_string, test_data.test_bool ? "true" : "false");

    return __monitor_paths();
}

