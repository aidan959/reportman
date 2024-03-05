#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>

#include "daemonize.h"
#include "directory_tool.h"
#include "monitor_tool.h"
#include "reportmand.h"

char *__save_pipe_buffer(FILE *fp);
void __free_running_pids(running_pid_t *pid, unsigned long num_pids);
running_pid_t *__parse_lsof_line(char *line);
unsigned long __parse_lsof_output(char *lsof_output, running_pid_t **pids);
int __get_other_pid(unsigned short singleton_port);
static void __clean_close(int sig);
static void __handle_command(char *command, unsigned long long int client_idk, command_response_t *response);
static timer_t __schedule_backup(time_t transfer_time);
static timer_t __schedule_transfer(time_t transfer_time);
static int __listen_to_clients(void);
static int __monitor_paths(const char **dirs, monitor_conf_t monitor_conf);
static void __kill_pid(int pid);
static int __force_singleton(int singleton_result, unsigned short port);
int __is_daemon(void);


const char REPORTS_DIRECTORY[] = "/srv/reportman/reports";
const char BACKUP_DIRECTORY[] = "/srv/reportman/backup";
const char DASHBOARD_DIRECTORY[] = "/srv/reportman/dashboard";

static timer_t transfer_timer;
static timer_t backup_timer;

static bool __client_request_close = false;

int d_socket;

int main(int argc, char *argv[])
{
    daemon_arguments_t exec_args = {
        .make_daemon = true,
        .daemon_port = REPORTMAND_BIND_PORT,
        .force = false,
        .transfer_time = 0,
        .backup_time = 0,
        .close = false
    };
    configure_daemon_args(argc, argv, &exec_args);
    signal(SIGINT, __clean_close);
    signal(SIGTERM, __clean_close);

    const char *dirs[] = {
        REPORTS_DIRECTORY,
        BACKUP_DIRECTORY,
        DASHBOARD_DIRECTORY};
    int singleton_result = d_acquire_singleton(&d_socket, exec_args.daemon_port);

    if (exec_args.close){
        if (singleton_result == IS_SINGLETON) 
        {
            printf("No reportman instance is running.\n");
        } else if (singleton_result > 0 ){
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
        syslog(LOG_ERR, "Could not bind to port %d", exec_args.daemon_port);
        exit(EXIT_FAILURE);
    default:
        if (!exec_args.force) {
            syslog(LOG_ERR, "Could not bind to port %d", exec_args.daemon_port);
            exit(EXIT_FAILURE);
        }
        singleton_result = __force_singleton(singleton_result, exec_args.daemon_port);
    }
    
    // Open log file
    if (exec_args.make_daemon && !__is_daemon()){
        become_daemon(0, d_socket);
        openlog("reportmand", LOG_PID, LOG_DAEMON);
    }
    else 
        openlog("reportmand", LOG_PID, LOG_USER);

    if(init_directories(sizeof(dirs) / sizeof(char *), dirs)){
        syslog(LOG_CRIT, "Failed to initialize directories %s, %s, %s", BACKUP_DIRECTORY, REPORTS_DIRECTORY, DASHBOARD_DIRECTORY);
        exit(EXIT_FAILURE);
    };

    switch (fork())
    {
        case -1:
            syslog(LOG_ERR, "Could not create file monitor child process.");
            exit(EXIT_FAILURE);
        case 0:
        {
            // saving 24 bytes of stack on daemon thread by scoping :)
            monitor_conf_t monitor_conf = {
                .log_file= "",
                .log_sys_name="",
                .log_to_file = "",
                .log_to_sys = "",

            };
            // blocking -> child that monitors directories
            exit(__monitor_paths(dirs, monitor_conf));
            
        }
        default:
            break;
    }
    openlog("reportmand", LOG_PID, LOG_DAEMON);

    transfer_timer = __schedule_transfer(exec_args.transfer_time);
    backup_timer = __schedule_backup(exec_args.backup_time);
    // blocks to go to main loop
    int exit_code = __listen_to_clients();

    closelog();
    syslog(LOG_NOTICE, "reportmand exiting");

    return exit_code;
}
static void __kill_pid(int singleton_result)
{
    if (kill(singleton_result, SIGTERM) < 0)
    {
        perror("Error killing other process, use systemctl instead.");
        exit(errno);
    }
}
static int __monitor_paths(const char **dirs, monitor_conf_t monitor_conf)
{
    close(d_socket);
    int monitor_exit = monitor_paths(sizeof(dirs) / sizeof(char *), dirs, monitor_conf);
    if (monitor_exit < 0)
    {
        syslog(LOG_ERR, "Path monitor exited unsuccessfully with value (%d)", monitor_exit);
    }
    else
    {
        syslog(LOG_NOTICE, "Path monitor exited successfully (%d).", monitor_exit);
    }
    return monitor_exit;
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
        if(__client_request_close)
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
        perror("Cannot create socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(singleton_port);

    if (bind(*sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        printf("Bind failed - another instance of reportmand may be running on port(%hu): %s\n", singleton_port, strerror(errno));

        return __get_other_pid(singleton_port);
    }

    if (listen(*sockfd, 5) < 0)
    {
        perror("Listening failed");
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
        no_errors = transfer_directory(REPORTS_DIRECTORY, BACKUP_DIRECTORY, (transfer_method)BACKUP);
        snprintf(response_msg, COMMUNICATION_BUFFER_SIZE, "Backup from %s to %s with %d errors. Check log for more details.", REPORTS_DIRECTORY, BACKUP_DIRECTORY, no_errors);
        break;

    case C_TRANSFER:
        syslog(LOG_NOTICE, "Client %llu requested a transfer.", client_idk);
        no_errors = transfer_directory(DASHBOARD_DIRECTORY, BACKUP_DIRECTORY, (transfer_method)TRANSFER);
        snprintf(response_msg, COMMUNICATION_BUFFER_SIZE, "Transfer from %s to %s with %d errors. Check log for more details.", DASHBOARD_DIRECTORY, BACKUP_DIRECTORY, no_errors);
        break;

    case C_GETTIMERS:
        syslog(LOG_NOTICE, "Client %llu requested to get timer.", client_idk);
        char backup_time_string[26];
        char transfer_time_string[26];

        struct itimerspec backup_time;
        struct itimerspec transfer_time; 

        gettime(backup_timer, &backup_time);
        gettime(transfer_timer, &transfer_time);
        struct tm * tm_info_backup;
        tm_info_backup = localtime(&backup_time.it_value.tv_sec);
        struct tm * tm_info_transfer;
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
            backup_time_string
        );
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

static timer_t __schedule_backup(time_t transfer_time)
{
    backup_timer = transfer_at_time(REPORTS_DIRECTORY, DASHBOARD_DIRECTORY, transfer_time, (time_t)D_INTERVAL, BACKUP);
    if (backup_timer == (void *)UNIMPLEMENTED_TRANSFER_METHOD)
    {
        syslog(LOG_ERR, "Unimplemented transfer method used.");
        return (void*)UNIMPLEMENTED_TRANSFER_METHOD;
    }
    return backup_timer;
}
static timer_t __schedule_transfer(time_t transfer_time)
{
    transfer_timer = transfer_at_time(DASHBOARD_DIRECTORY, BACKUP_DIRECTORY, transfer_time, (time_t)D_INTERVAL, TRANSFER);

    if (transfer_timer == (void *)UNIMPLEMENTED_TRANSFER_METHOD)
    {
        syslog(LOG_ERR, "Unimplemented transfer method used.");
        return (void*)UNIMPLEMENTED_TRANSFER_METHOD;
    }
    return transfer_timer;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
static void __clean_close(int sig)
{
    close(d_socket);
    exit(0);
}
#pragma GCC diagnostic pop

int __force_singleton(int singleton_result, unsigned short port){
    syslog(LOG_WARNING, "Forcing reportmand to start on port %d", port);
    printf("Forcing reportmand to start on port %d\n", port);

    __kill_pid(singleton_result);
    sleep(1);
    singleton_result = d_acquire_singleton(&d_socket, port);
    if (singleton_result != IS_SINGLETON)
    {
        fprintf(stderr,"Could not acquire singleton after force.");
        
        exit(EXIT_FAILURE);
    }
    return singleton_result;
}

int __is_daemon(void) {

    pid_t pid = getpid();

    pid_t pgid = getpgid(pid);

    pid_t sid = getsid(pid);

    if (pgid == sid) {
        return 1;
    } else {
        return 0;
    }
}