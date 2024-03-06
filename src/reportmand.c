#define _POSIX_C_SOURCE 199309L // for POSIX timers
#define _XOPEN_SOURCE 600
#include <stdio.h>

#include <stdlib.h>
#include <syslog.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <time.h>
#include <errno.h>
#include <linux/limits.h>
#include <unistd.h>
#include <libgen.h>

#include "libs/include/daemonize.h"
#include "libs/include/directory_tool.h"
#include "libs/include/reportman.h"
#include "include/reportmand.h"

static int __wait_for_reportman_fm(void);
char *__save_pipe_buffer(FILE *fp);
void __free_running_pids(running_pid_t *pid, unsigned long num_pids);
running_pid_t *__parse_lsof_line(char *line);
unsigned long __parse_lsof_output(char *lsof_output, running_pid_t **pids);
static void __configure_daemon_args(int argc, char *argv[], daemon_arguments_t *args);
int __get_other_pid(unsigned short singleton_port);
static void __clean_close(int sig);
static void __handle_command(char *command, unsigned long long int client_idk, command_response_t *response);
static int __listen_to_clients(void);
static void __kill_pid(int pid);
static int __force_singleton(int singleton_result, unsigned short port);
static int __is_daemon(void);
static void __become_daemon(void);
static int __initialize_pipes(int daemon_pipes[2], int child_pipes[2]);
static int __get_path(const char *input_path, char *output_path);
static bool __is_path_empty(const char path[PATH_MAX]);
static bool __has_path_parent(const char *path);
static bool __has_write_permission_to_parent(const char path[PATH_MAX]);
static bool __expand_path_to_absolute(const char path[PATH_MAX], char output_path[PATH_MAX]);
static bool __remove_last_item_path(const char path[PATH_MAX], char directory[PATH_MAX], char item[PATH_MAX]);
static pid_t __start_child_process(const char *executable, const char *extra_args[], int *write_fd, int *read_fd);
static void __acquire_singleton(void);
static void __init_directories(void);

const char REPORTS_DIRECTORY[] = R_REPORTS_DIRECTORY;
const char BACKUP_DIRECTORY[] = R_BACKUP_DIRECTORY;
const char DASHBOARD_DIRECTORY[] = R_DASHBOARD_DIRECTORY;

static timer_t __transfer_timer;
static timer_t __backup_timer;

static int __daemon_to_fm_write;
static int __fm_to_daemon_read;

static int __daemon_to_monitor_write;
static int __monitor_to_daemon_read;

static bool __client_request_close = false;

int d_socket;

static daemon_arguments_t __exec_args = {
    .make_daemon = true,
    .daemon_port = REPORTMAND_BIND_PORT,
    .force = false,
    .close = false,
    .transfer_time_str = "23:30",
    .backup_time_str = "01:00",
    .log_to_sys = false,
    .log_to_file = true,
    .monitor_log_file_path = M_LOG_PATH,
    .monitor_log_sys_name = "reportman_monitor"};

static const char *__directories[] = {
    REPORTS_DIRECTORY,
    BACKUP_DIRECTORY,
    DASHBOARD_DIRECTORY};
int main(int argc, char *argv[])
{
    __configure_daemon_args(argc, argv, &__exec_args);

    signal(SIGINT, __clean_close);
    signal(SIGTERM, __clean_close);

    __acquire_singleton();

    __become_daemon();

    __init_directories();

    pid_t reportman_fm_pid = __start_child_process("reportman_fm", __directories, &__daemon_to_fm_write, &__fm_to_daemon_read);
    syslog(LOG_DEBUG, "reportman_fd running as child (%d)", reportman_fm_pid);

    __wait_for_reportman_fm();

    pid_t reportman_mon_pid = __start_child_process("reportman_monitor", __directories, &__daemon_to_monitor_write, &__monitor_to_daemon_read);

    syslog(LOG_DEBUG, "reportman_monitor running as child (%d)", reportman_mon_pid);

    // blocks to go to main loop
    int exit_code = __listen_to_clients();

    closelog();
    syslog(LOG_NOTICE, "reportmand exiting");

    return exit_code;
}
static void __init_directories(void)
{
    if (init_directories(sizeof(__directories) / sizeof(char *), __directories))
    {
        syslog(LOG_CRIT, "Failed to initialize directories %s, %s, %s", BACKUP_DIRECTORY, REPORTS_DIRECTORY, DASHBOARD_DIRECTORY);
        exit(EXIT_FAILURE);
    };
}
static void __acquire_singleton(void)
{
    int singleton_result = d_acquire_singleton(&d_socket, __exec_args.daemon_port);
    if (__exec_args.close)
    {
        if (singleton_result == IS_SINGLETON)
        {
            printf("No reportman instance is running.\n");
        }
        else if (singleton_result > 0)
        {
            __kill_pid(singleton_result);
            printf("reportmand (%d) closed successfully.\n", singleton_result);
        }
        exit(EXIT_SUCCESS);
    }

    switch (singleton_result)
    {
    case IS_SINGLETON:
        syslog(LOG_NOTICE, "reportmand started");
        break;
    case BIND_FAILED:
        syslog(LOG_ERR, "Could not bind to port %d", __exec_args.daemon_port);
        exit(EXIT_FAILURE);
    default:
        if (!__exec_args.force)
        {
            syslog(LOG_ERR, "Could not bind to port %d", __exec_args.daemon_port);
            exit(EXIT_FAILURE);
        }
        singleton_result = __force_singleton(singleton_result, __exec_args.daemon_port);
    }
}
static void __kill_pid(int singleton_result)
{
    if (kill(singleton_result, SIGTERM) < 0)
    {
        perror("Error killing other process, use systemctl instead.");
        exit(errno);
    }
}

static void __become_daemon(void)
{
    if (__exec_args.make_daemon && !__is_daemon())
    {
        become_daemon(0, d_socket);
        openlog("reportmand", LOG_PID, LOG_DAEMON);
    }
    else
        openlog("reportmand", LOG_PID, LOG_USER);
}

static int __listen_to_clients(void)
{
    char buffer[COMMUNICATION_BUFFER_SIZE];
    // lets give this a huge number before it restarts
    unsigned long long int client_id = 0;
    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_fd = accept(d_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0)
        {
            if (errno == EINTR)
                continue;
            syslog(LOG_ALERT, "accept() failed (unexpected): %s", strerror(errno));
            continue;
        }

        fd_set readfds;
        struct timeval tv;

        tv.tv_sec = 10;
        tv.tv_usec = 0;
        FD_ZERO(&readfds);
        FD_SET(client_fd, &readfds);

        int retval;
        while ((retval = select(client_fd + 1, &readfds, NULL, NULL, &tv)))
        {
            ssize_t len = read(client_fd, buffer, COMMUNICATION_BUFFER_SIZE - 1);
            if (len > 0)
            {
                syslog(LOG_NOTICE, "Client %llu has connected to dameon.", client_id);

                // FD_ISSET(0, &readfds) will be true.
                buffer[len] = '\0';
                syslog(LOG_NOTICE, "Received command %s from client %llu\n", buffer, client_id);
                command_response_t response;
                __handle_command(buffer, client_id, &response);
                write(client_fd, response.response, strlen(response.response));
            }
            else if (len == 0)
            {
                syslog(LOG_NOTICE, "Client %llu closed their connection.", client_id);
                break;
            }
            else if (len < 0)
            {
                syslog(LOG_ERR, "read failed: %s", strerror(errno));
            }
        }
        if (retval == -1)
        {
            syslog(LOG_ERR, "select() failed: %s", strerror(errno));
        }
        else
        {
            // no comms made in 10 seconds - lets close prematurely so another client can connect
            char timeout_msg[] = "Timeout reached. Closing connection.";
            write(client_fd, timeout_msg, sizeof(timeout_msg));
        }

        close(client_fd); // Close the client socket
        if (__client_request_close)
            break;
        client_id++;
    }
    return D_SUCCESS;
}

int d_acquire_singleton(int *sockfd, short unsigned singleton_port)
{
    struct sockaddr_in addr;

    // Create a socket
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*sockfd < 0)
    {
        fprintf(stderr, "Cannot create singleton socket: %s", strerror(errno));
        return D_FAILURE;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(singleton_port);

    if (bind(*sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        fprintf(stderr, "Bind failed - another instance of reportmand may be running on port(%hu): %s\n", singleton_port, strerror(errno));
        return __get_other_pid(singleton_port);
    }

    if (listen(*sockfd, 5) < 0)
    {
        fprintf(stderr, "Listening on singleton socket failed: %s\n", strerror(errno));
        close(*sockfd);
        exit(EXIT_FAILURE);
    }
    return IS_SINGLETON;
}
/// @brief gets the PID if the singleton port is not available.
/// @param singleton_port
/// @return result of the command
int __get_other_pid(unsigned short singleton_port)
{
    FILE *fp;

    char *lsof_output = NULL;
    unsigned long num_of_pids = 0;

    // Create the command string
    char command[50];
    sprintf(command, "lsof -i :%hu", singleton_port);

    // Execute the command and open a pipe to read its output
    fp = popen(command, "r");
    if (fp == NULL)
    {
        perror("Failed to run lsof");
        return 0;
    }
    // make copy of buffer before pipe close
    lsof_output = __save_pipe_buffer(fp);

    switch (pclose(fp))
    {
    case COMMAND_SUCCESSFUL:
        break;
    case LSOF_FD_NOT_FOUND:
        free(lsof_output);
        // TODO IMPLEMENT RETRY PATTERN IF THIS CASE HAPPENS - WE MAY HAVE GRABBED THE PORT TOO EARLY
        printf("No processes is using the singleton port %d\n", singleton_port);
        return -1;
    case COMMAND_NOT_FOUND:
        free(lsof_output);
        printf("lsof was not found by the shell. Please install LSOF to get the process id of conflicting host.");
        return -2;
    case COMMAND_ERROR:
        free(lsof_output);
        return -1;
    default:
        // command wasnt successful - free buffer and return
        free(lsof_output);
        return -1;
    }
    running_pid_t *running_pids = NULL;
    num_of_pids = __parse_lsof_output(lsof_output, &running_pids);
    int pid = running_pids[0].pid;
    for (unsigned long i = 0; i < num_of_pids; i++)
    {
        printf("Process \"%s\" (PID: %d) is using the singleton port %d\n", running_pids[i].command, running_pids[i].pid, singleton_port);
    }
    free(lsof_output);
    __free_running_pids(running_pids, num_of_pids);
    return pid;
}
/// @brief Parses output line of LSOF and
/// @param lsof_output
/// @param pid
/// @return
unsigned long __parse_lsof_output(char *lsof_output, running_pid_t **pids)
{
    char *line;
    running_pid_t *running_pids;
    unsigned long capcacity = 0;
    unsigned long count = 0;
    char *save_pointer = NULL;
    line = strtok_r(lsof_output, "\n", &save_pointer);
    while (line != NULL)
    {
        if (count >= capcacity)
        {
            // reduces number of extra assignments
            capcacity = capcacity == 0 ? 2 : capcacity * 2;
            running_pid_t *new_running_pids = realloc(running_pids, sizeof(running_pid_t) * capcacity);
            if (!new_running_pids)
            {
                free(running_pids);
                perror("Failed to malloc to display PIDS.");
                exit(1);
            }
            running_pids = new_running_pids;
        }

        running_pid_t *pid = __parse_lsof_line(line);
        if (pid == NULL)
        {
            line = strtok_r(NULL, "\n", &save_pointer);
            continue;
        }
        running_pids[count++] = *pid;
        line = strtok_r(NULL, "\n", &save_pointer);
    }
    *pids = running_pids;
    return count;
}

running_pid_t *__parse_lsof_line(char *line)
{
    char type[512], node[512], protocol[512], address[512], state[512];
    int pid, fd;
    char device[512], size_off[512];
    // heap allocate command so it can be returned
    char *command = malloc(sizeof(char) * 512);
    // Example format: "reportmand 1234 user 7u IPv4 7770 0t0 TCP *:http (LISTEN)"
    int result = sscanf(line, "%s %d %*s %d%s %s %s %s %s %s %s",
                        command, &pid, &fd, type, device, size_off, node, protocol, address, state);

    // Check if the line was parsed successfully
    if (result >= 9)
    {
        running_pid_t *running_pid = malloc(sizeof(running_pid));
        running_pid->pid = pid;
        running_pid->command = command;
        return running_pid;
    }
    free(command);
    return NULL;
}

void __free_running_pids(running_pid_t *pid, unsigned long num_pids)
{
    for (unsigned long i = 0; num_pids < i; i++)
    {
        free(pid[i].command);
    }
    free(pid);
}
char *__save_pipe_buffer(FILE *fp)
{
    char buffer[1024];
    size_t data_allocated = 0;
    size_t data_used = 0;
    char *lsof_output = NULL;
    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        size_t bufferLen = strlen(buffer);

        // Ensure there's enough space in the lsof_output buffer
        if (data_used + bufferLen + 1 > data_allocated)
        {
            size_t new_allocated = data_allocated == 0 ? bufferLen * 2 : data_allocated * 2;
            char *newData = realloc(lsof_output, new_allocated);
            if (!newData)
            {
                perror("Failed to allocate memory");
                free(lsof_output);
                pclose(fp);
                exit(1);
            }
            lsof_output = newData;
            data_allocated = new_allocated;
        }

        // Append the new lsof_output
        memcpy(lsof_output + data_used, buffer, bufferLen);
        data_used += bufferLen;
        lsof_output[data_used] = '\0'; // Ensure the string is null-terminated
    }
    return lsof_output;
}
static void __handle_command(char *command, unsigned long long int client_idk, command_response_t *response)
{
    char *response_msg = malloc(COMMUNICATION_BUFFER_SIZE);
    int no_errors;

    parse_command(command, response);

    switch (response->command)
    {
    case C_BACKUP:
        syslog(LOG_NOTICE, "Client %llu requested a backup.", client_idk);
        no_errors = transfer_directory(REPORTS_DIRECTORY, BACKUP_DIRECTORY, (transfer_method_t)BACKUP);
        snprintf(response_msg, COMMUNICATION_BUFFER_SIZE, "Backup from %s to %s with %d errors. Check log for more details.", REPORTS_DIRECTORY, BACKUP_DIRECTORY, no_errors);
        break;

    case C_TRANSFER:
        syslog(LOG_NOTICE, "Client %llu requested a transfer.", client_idk);
        no_errors = transfer_directory(DASHBOARD_DIRECTORY, BACKUP_DIRECTORY, (transfer_method_t)TRANSFER);
        snprintf(response_msg, COMMUNICATION_BUFFER_SIZE, "Transfer from %s to %s with %d errors. Check log for more details.", DASHBOARD_DIRECTORY, BACKUP_DIRECTORY, no_errors);
        break;

    case C_GETTIMERS:
        syslog(LOG_NOTICE, "Client %llu requested to get timer.", client_idk);
        char backup_time_string[26];
        char transfer_time_string[26];

        struct itimerspec backup_time;
        struct itimerspec transfer_time;

        gettime(__backup_timer, &backup_time);
        gettime(__transfer_timer, &transfer_time);
        struct tm *tm_info_backup;
        tm_info_backup = localtime(&backup_time.it_value.tv_sec);
        struct tm *tm_info_transfer;
        tm_info_transfer = localtime(&transfer_time.it_value.tv_sec);
        strftime(backup_time_string, sizeof(backup_time_string), "%Y-%m-%d %H:%M:%S", tm_info_backup);
        strftime(transfer_time_string, sizeof(transfer_time_string), "%Y-%m-%d %H:%M:%S", tm_info_transfer);
        snprintf(response_msg, COMMUNICATION_BUFFER_SIZE,
                 "Next transfer from %s to %s scheduled at %s\nNext backup of %s to %s scheduled at %s",
                 REPORTS_DIRECTORY,
                 DASHBOARD_DIRECTORY,
                 transfer_time_string,
                 DASHBOARD_DIRECTORY,
                 BACKUP_DIRECTORY,
                 backup_time_string);
        break;

    case C_CLOSE:
        syslog(LOG_NOTICE, "Client %llu requested to close.", client_idk);
        snprintf(response_msg, COMMUNICATION_BUFFER_SIZE, "Close accepted. Exit reportmand.");
        __client_request_close = true;
        break;

    case C_UNKNOWN:
        syslog(LOG_NOTICE, "Client %llu requested an unknown command.", client_idk);
        snprintf(response_msg, COMMUNICATION_BUFFER_SIZE, "Unknown command.");
        break;

    default:
        syslog(LOG_WARNING, "Client %llu requested an unimplemented command (DAEMON MAY HAVE IMPLEMENTATION).", client_idk);
        snprintf(response_msg, COMMUNICATION_BUFFER_SIZE, "Command not implemented.");
    }
    response->response = response_msg;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
static void __clean_close(int sig)
{
    close(d_socket);
    exit(0);
}
#pragma GCC diagnostic pop

int __force_singleton(int singleton_result, unsigned short port)
{
    syslog(LOG_WARNING, "Forcing reportmand to start on port %d", port);
    printf("Forcing reportmand to start on port %d\n", port);

    __kill_pid(singleton_result);
    sleep(1);
    singleton_result = d_acquire_singleton(&d_socket, port);
    if (singleton_result != IS_SINGLETON)
    {
        fprintf(stderr, "Could not acquire singleton after force.");

        exit(EXIT_FAILURE);
    }
    return singleton_result;
}

static int __is_daemon(void)
{

    pid_t pid = getpid();
    pid_t pgid = getpgid(pid);
    pid_t sid = getsid(pid);

    if (pgid == sid)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/// @brief Waits for reportman_fm to send timer data
/// @return returns success of function
static int __wait_for_reportman_fm(void)
{
    struct timeval timeout;

    time_t timeout_secs = 10;
    timeout.tv_sec = timeout_secs;
    timeout.tv_usec = 0;

    bool finish = false;

    while (!finish)
    {
        fd_set set;

        // Initialize the file descriptor set
        FD_ZERO(&set);
        FD_SET(__fm_to_daemon_read, &set);

        int read_fm = select(__fm_to_daemon_read + 1, &set, NULL, NULL, &timeout);

        if (read_fm == D_FAILURE)
        {
            syslog(LOG_ERR, "select could not block: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        else if (read_fm == 0)
        {
            syslog(LOG_ERR, "reportman_fm did not respond after %lds, exiting...: %s", timeout_secs, strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (FD_ISSET(__fm_to_daemon_read, &set)) {
            if (ipc_get_ack(__fm_to_daemon_read))
            {
                fprintf(stderr, "Successfully received Ack\n");
            }
            else
            {
                syslog(LOG_ERR, "reportman_fm dud not send IPC ACK exiting...: %s", strerror(errno));
                exit(EXIT_FAILURE);
            }
            if (ipc_send_ack(__daemon_to_fm_write))
            {
                fprintf(stderr, "Successfully sent Ack\n");
            }
            else
            {
                syslog(LOG_ERR, "reportman could not send IPC ACK exiting...: %s", strerror(errno));
                exit(EXIT_FAILURE);
            }

        }
        
        exit(EXIT_SUCCESS);
        if (read(__fm_to_daemon_read, &__transfer_timer, sizeof(__transfer_timer)) <= 0)
        {
            syslog(LOG_ERR, "reportman_fm could not receive transfer timer...: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        syslog(LOG_DEBUG, "Backup_timer_id: %p\tTransfer_timer_id: %p\n", __backup_timer, __transfer_timer);
        finish = true;
    }
    return D_SUCCESS;
}

/// @brief Creates a child process, mirrors new process and returns communication pipe ids
/// @param executable
/// @param extra_args
/// @param write_fd
/// @param read_fd
/// @return pid of child
static pid_t __start_child_process(const char *executable, const char *extra_args[], int *write_fd, int *read_fd)
{
    static int child_pipes[2];
    static int parent_pipes[2];

    if (__initialize_pipes(parent_pipes, child_pipes) == D_FAILURE)
    {
        return D_FAILURE;
    };

    pid_t fork_pid = fork();
    if (fork_pid == D_FAILURE)
    {
        syslog(LOG_ERR, "reportman could not spawn %s child: %s", executable, strerror(errno));
        exit(EXIT_FAILURE);
    }
    else if (fork_pid == D_SUCCESS)
    { // is child
        close(d_socket);
        close(parent_pipes[1]);
        close(child_pipes[0]);

        // egregiously sized but cleared on execl maybe?
        char parent_to_child_read_pipe[COMMUNICATION_BUFFER_SIZE];
        char child_to_parent_write_pipe[COMMUNICATION_BUFFER_SIZE];

        snprintf(parent_to_child_read_pipe, sizeof(parent_to_child_read_pipe), "%d", parent_pipes[0]);
        snprintf(child_to_parent_write_pipe, sizeof(child_to_parent_write_pipe), "%d", child_pipes[1]);

        int arg_count;
        for (arg_count = 0; extra_args[arg_count] != NULL; ++arg_count)
        {
            if (arg_count > D_MAX_INTERPROCESS_ARGUMENTS)
            {
                syslog(LOG_ERR, "Too many arguments passed to child process, or we are missing the NULL array terminator.");
                exit(EXIT_FAILURE);
            }
        }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
        const int fixed_args = 6;
        const char *args[arg_count + fixed_args]; // Extra space for fixed arguments and NULL terminator
        args[0] = executable;                     // First arg is the name of the program
        args[1] = "--from-daemon";
        args[2] = "--d-to-c";
        args[3] = parent_to_child_read_pipe;
        args[4] = "--c-to-d";
        args[5] = child_to_parent_write_pipe;

        for (int i = 0; i < arg_count; i++)
        {
            args[i + fixed_args] = (char *)(extra_args[i]);
        }

        args[arg_count + fixed_args] = NULL;

        execv(executable, (char *const *)args);

#pragma GCC diagnostic pop

        int err = errno;

        syslog(LOG_ERR, "execl FAILED to overlay %s %d", strerror(err), err);

        if (err == 2)
        {
            syslog(LOG_ERR, "Could not find reportman_monitor executable. Please ensure it is executable from call location.");
        }

        exit(EXIT_FAILURE);
    }
    close(parent_pipes[0]);
    close(child_pipes[1]);

    *write_fd = parent_pipes[1];
    *read_fd = child_pipes[0];

    syslog(LOG_DEBUG, "%s child spawned with PID: %d", executable, fork_pid);
    return fork_pid;
}
static int __initialize_pipes(int daemon_pipes[2], int child_pipes[2])
{
    if ((pipe(child_pipes) == -1) || (pipe(daemon_pipes) == -1))
    {
        syslog(LOG_ERR, "Could not create pipes: %s", strerror(errno));
        return D_FAILURE;
    }
    return D_SUCCESS;
}

static void __configure_daemon_args(int argc, char *argv[], daemon_arguments_t *args)
{
    int i;

    for (i = 1; i < argc; i++)
    {
        char arg_string[COMMUNICATION_BUFFER_SIZE];
        unsigned short arg_int[1];
        if (arg_parse_flag(argv[i], "-nd", "--no-daemon") == R_PARSE_SUCCESS)
        {
            args->make_daemon = false;
        }
        else if (arg_parse_flag(argv[i], "-f", "--force"))
        {
            args->force = true;
        }
        else if (arg_parse_flag(argv[i], "-c", "--close"))
        {
            args->close = true;
        }
        else if (arg_parse_ushort(argc, argv, &i, "-p", "--port", "port", arg_int))
        {
            args->daemon_port = *arg_int;
        }
        else if (arg_parse_string(argc, argv, &i, "-tt", "--transfer-time", "time stamp", arg_string))
        {
            char *transfer_time = malloc(sizeof(char) * COMMUNICATION_BUFFER_SIZE);
            strcpy(transfer_time, arg_string);
            args->transfer_time_str = transfer_time;
        }
        else if (arg_parse_string(argc, argv, &i, "-bt", "--backup-time", "time stamp", arg_string))
        {
            char *backup_time = malloc(sizeof(char) * COMMUNICATION_BUFFER_SIZE);
            strcpy(backup_time, arg_string);
            args->backup_time_str = backup_time;
        }
        else if (arg_parse_string(argc, argv, &i, "-lf", "--log-file", "log file", arg_string))
        {
            char *log_file = malloc(sizeof(char) * PATH_MAX);
            int response = __get_path(arg_string, log_file);

            if (response < 0)
            {
                exit(response);
            }

            args->monitor_log_file_path = log_file;
            continue;
        }
        else
        {
            fprintf(stderr, "Unrecognized argument passed '%s'.", argv[i]);
            exit(EXIT_FAILURE);
        }
    }
}

/// @brief Returns
/// @param input_path
/// @param output_path
/// @return
static int __get_path(const char *input_path, char *output_path)
{
    if (__is_path_empty(input_path))
    {
        fprintf(stderr, "No path has been supplied.\n");
        return D_INVALID_PATH_FORMAT;
    }

    char file_name[PATH_MAX];
    char directory[PATH_MAX];
    bool is_excl_file = __remove_last_item_path(input_path, directory, file_name);

    if (!is_excl_file)
    { // user only provided a file - no path
        if (!__expand_path_to_absolute(input_path, output_path))
        { // try to get this as absoulute
            return D_COULD_NOT_EXPAND;
        }
        strcpy(output_path, input_path);
        return D_SUCCESS;
    }

    char expanded_directory[PATH_MAX];

    if (!__expand_path_to_absolute(directory, expanded_directory))
    { // could not expand directory to absolute
        return D_COULD_NOT_EXPAND;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(output_path, PATH_MAX * 2, "%s/%s", directory, file_name);
#pragma GCC diagnostic pop

    if (!__has_path_parent(output_path))
    {
        fprintf(stderr, "The file specified does not have a parent path.\n");
        return D_PARENT_PATH_NO_EXIST;
    }
    if (__has_write_permission_to_parent(output_path))
    {
        fprintf(stderr, "This daemon does not have write permissions to the parent log directory.\n");
        return D_NO_WRITE_TO_LOG_PARENT_DIRECTORY;
    }

    return D_SUCCESS;
}

/// @brief Determines if a path's parent directory exists
/// @param path
/// @return True -> It does, False -> it doesn't
static bool __has_path_parent(const char *path)
{
    char path_copy[PATH_MAX];
    strncpy(path_copy, path, PATH_MAX);
    char *parent_dir = dirname(path_copy);
    return access(parent_dir, F_OK) == 0;
}

/// @brief Determines if we have write permissions to the parent directory of the path
/// @param path
/// @return True -> we have permission, False -> we do not have permission
static bool __has_write_permission_to_parent(const char path[PATH_MAX])
{
    char path_copy[PATH_MAX];
    strncpy(path_copy, path, PATH_MAX);

    char *parent_dir = dirname(path_copy);

    return access(parent_dir, W_OK) == W_OK;
}

/// @brief Returns if path is empty
/// @param path
/// @return True -> path is empty, False -> Path is not empty
static bool __is_path_empty(const char path[PATH_MAX])
{
    return path == NULL || path[0] == '\0';
}

/// @brief Attemps to expand the input path to an absolute value.
/// @param path
/// @param output_path
/// @return True -> successful, False -> unsuccessful
static bool __expand_path_to_absolute(const char path[PATH_MAX], char output_path[PATH_MAX])
{
    if (realpath(path, output_path) == NULL)
    {
        fprintf(stderr, "%s could not resolve to absolute directory: %s\n", path, strerror(errno));
        return false;
    }
    return true;
}

/// @brief Returns if path has directory or not.
/// @param path
/// @return
static bool __remove_last_item_path(const char path[PATH_MAX], char directory[PATH_MAX], char item[PATH_MAX])
{
    char new_path[PATH_MAX];
    strcpy(new_path, path);
    char *last_slash = strrchr((const char *)new_path, '/');

    if (last_slash == NULL)
    {
        return false;
    }
    *last_slash = '\0';
    strcpy(directory, new_path);
    strcpy(item, (new_path + strlen(new_path) + 1));
    fprintf(stderr, "%s", item);
    return true;
}
