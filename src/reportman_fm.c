#define _POSIX_C_SOURCE 199309L // for POSIX timers

#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <time.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>
#include <sys/signalfd.h>
#include <poll.h>
#include <bits/sigaction.h>

#include "include/reportman_fm.h"
#include "libs/include/reportman.h"
#include "libs/include/directory_tool.h"

static timer_t __schedule_backup(time_t transfer_time);
static timer_t __schedule_transfer(time_t transfer_time);
void __configure_reportman_fm_args(int argc, char *argv[], reportman_fm_args_t *args);
static int __started_from_daemon(void);
static int __report_to_daemon(void);
static void __shutdown_signals(int signal_fd, ipc_pipes_t *pipes);

static timer_t __backup_timer;
static timer_t __transfer_timer;

static reportman_fm_args_t __fm_args = {
    .from_daemon = false,
    .debug = false,
    .pipes = {.read = -1,
              .write = -1},
    .backup_time = 0,
    .transfer_time = 0,
    .do_backup = false,
    .do_transfer = false,
};

int main(int argc, char **argv)
{
    __configure_reportman_fm_args(argc, argv, &__fm_args);
    __started_from_daemon();

    // was not called from daemon
    if (__fm_args.do_backup)
        transfer_directory(R_DASHBOARD_DIRECTORY, R_BACKUP_DIRECTORY, BACKUP);

    if (__fm_args.do_transfer)
        transfer_directory(R_REPORTS_DIRECTORY, R_DASHBOARD_DIRECTORY, TRANSFER);
    exit(EXIT_SUCCESS);
}

/// @brief Runs if executed from daemon
/// @param
/// @return Discardable
static int __started_from_daemon(void)
{
    if (!__fm_args.from_daemon)
    {
        openlog("reportman_fm", LOG_PID, LOG_USER);
        return 0;
    }
    if (!__fm_args.debug && (__fm_args.pipes.read < 0 || __fm_args.pipes.write < 0))
    {
        syslog(LOG_ERR, "Pipes not received from daemon.");
        fprintf(stderr, "-fd can ONLY be called from a daemon.");
        return D_FAILURE;
    }
    openlog("reportman_fm", LOG_PID, LOG_DAEMON);

    syslog(LOG_NOTICE, "reportman_fm started by daemon.");

    __schedule_backup(__fm_args.backup_time);
    __schedule_transfer(__fm_args.transfer_time);

    if(!__fm_args.debug) acknowledge_daemon(&__fm_args.pipes);
    if(!__fm_args.debug)__report_to_daemon();

    int signal_fd;
    struct pollfd fds[FM_FD_POLL_MAX];
    if ((signal_fd = r_initialize_signals()) < 0)
    {
        syslog(LOG_ALERT, "Could not initialize signals");
        exit(EXIT_FAILURE);
    }
    // enable polling
    fds[FM_FD_POLL_SIGNAL].fd = signal_fd;
    fds[FM_FD_POLL_SIGNAL].events = POLLIN;
    fds[FM_FD_POLL_PARENT].fd = __fm_args.pipes.read;
    fds[FM_FD_POLL_PARENT].events = POLLIN;
    nfds_t poll_count;
    if(__fm_args.pipes.read > 0)
        poll_count = (nfds_t)FM_FD_POLL_MAX;
    else
        poll_count = (nfds_t)FM_FD_POLL_MAX - 1; 
    for (;;)
    {
        // blocks to read for parent / signals
        if (poll(fds, poll_count, -1) < 0)
        {
            syslog(LOG_ERR,
                   "Couldn't poll(): '%s'\n",
                   strerror(errno));
            exit(EXIT_FAILURE);
        }
        if(fds[FM_FD_POLL_PARENT].revents & POLLIN)
        {
            char buffer[COMMUNICATION_BUFFER_SIZE];
            ssize_t length;
            if ((length = read(fds[FM_FD_POLL_PARENT].fd, buffer, COMMUNICATION_BUFFER_SIZE)) > 0)
            {
                syslog(LOG_NOTICE, "Received message from parent: %s", buffer);
            }
        }
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
        __shutdown_signals(signal_fd, &__fm_args.pipes);

    }
    exit(EXIT_SUCCESS);
}


/// @brief Closes pipe and signal file descriptors
/// @param signal_fd 
/// @param pipes 
static void __shutdown_signals(int signal_fd, ipc_pipes_t *pipes)
{
    close(pipes->read);
    close(pipes->write);

    close(signal_fd);
}


static int __report_to_daemon(void)
{
    if (!ipc_send_ulong(&__fm_args.pipes, (unsigned long)__backup_timer))
    {
        syslog(LOG_ERR, "Failed to send backup timer to daemon.");
        return D_FAILURE;
    }
    if (!ipc_send_ulong(&__fm_args.pipes, (unsigned long)__transfer_timer))
    {
        syslog(LOG_ERR, "Failed to send transfer timer to daemon.");
        return D_FAILURE;
    }

    return D_SUCCESS;
}

/// @brief Schedules a backup at UNIX time transfer_time
/// @param transfer_time
/// @return POSIX timer value
static timer_t __schedule_backup(time_t transfer_time)
{
    __backup_timer = transfer_at_time(R_REPORTS_DIRECTORY, R_DASHBOARD_DIRECTORY, transfer_time, (time_t)D_INTERVAL, BACKUP);
    if (__backup_timer == (void *)UNIMPLEMENTED_TRANSFER_METHOD)
    {
        syslog(LOG_ERR, "Unimplemented transfer method used.");
        return (void *)UNIMPLEMENTED_TRANSFER_METHOD;
    }
    return __backup_timer;
}

/// @brief Schedules a transfer at UNIX time transfer_time
/// @param transfer_time
/// @return POSIX timer value
static timer_t __schedule_transfer(time_t transfer_time)
{
    __transfer_timer = transfer_at_time(R_DASHBOARD_DIRECTORY, R_BACKUP_DIRECTORY, transfer_time, (time_t)D_INTERVAL, TRANSFER);

    if (__transfer_timer == (void *)UNIMPLEMENTED_TRANSFER_METHOD)
    {
        syslog(LOG_ERR, "Unimplemented transfer method used.");
        return (void *)UNIMPLEMENTED_TRANSFER_METHOD;
    }
    return __transfer_timer;
}

/// @brief Reads command line arguments and populates monitor_args_t
/// @param argc passed arguments
/// @param argv number of arguments
/// @param args reportman_fm configuration
void __configure_reportman_fm_args(int argc, char *argv[], reportman_fm_args_t *args)
{
    int i;
    for (i = 1; i < argc; i++)
    {
        char arg_string[COMMUNICATION_BUFFER_SIZE];
        int arg_int[1];

        if (arg_parse_flag(argv[i], "-fd", "--from-daemon"))
        {
            args->from_daemon = true;
            continue;
        }
        else if (arg_parse_flag(argv[i], "--debug", "--debug"))
        {
            args->debug = true;
            continue;
        }
        else if (arg_parse_int(argc, argv, &i, "--d-to-c", "--d-to-c", "daemon to fm read pipe id", arg_int))
        {
            args->pipes.read = *arg_int;
            continue;
        }
        else if (arg_parse_int(argc, argv, &i, "--c-to-d", "--c-to-d", "fm to daemon write pipe id", arg_int))
        {
            args->pipes.write = *arg_int;
            continue;
        }
        else if (arg_parse_string(argc, argv, &i, "-tt", "--transfer-time", "time stamp", arg_string))
        {
            time_t transfer_time;
            int response = get_hh_mm_str(arg_string, &transfer_time);
            if (response < 0)
            {
                exit(response);
            }
            args->transfer_time = transfer_time;
            continue;
        }
        else if (arg_parse_string(argc, argv, &i, "-bt", "--backup-time", "time stamp", arg_string))
        {
            time_t backup_time;
            int response = get_hh_mm_str(arg_string, &backup_time);
            if (response < 0)
            {
                exit(response);
            }
            args->backup_time = backup_time;
            continue;
        }
        else if (arg_parse_flag(argv[i], "-b", "--backup"))
        {
            args->do_backup = true;
            continue;
        }
        else if (arg_parse_flag(argv[i], "-t", "--transfer"))
        {
            args->do_transfer = true;
            continue;
        }
        else if (arg_unrecognised(argv[i]))
        {
            syslog(LOG_ERR, "Argument '%s' is not defined", argv[i]);
            fprintf(stderr, "Argument '%s' is not defined", argv[i]);
            exit(EXIT_FAILURE);
        }
    }
    if (!args->backup_time)
    {
        time_t backup_time;
        int response = get_hh_mm_str(D_DEFAULT_BACKUP_TIME, &backup_time);
        if (response < 0)
            exit(response);
        args->backup_time = backup_time;
    }
    if (!args->transfer_time)
    {
        time_t transfer_time;
        int response = get_hh_mm_str(D_DEFAULT_TRANSFER_TIME, &transfer_time);
        if (response < 0)
            exit(response);

        args->transfer_time = transfer_time;
    }
}
