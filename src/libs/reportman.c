// Shared daemon and client functionality -> using this currently for header values and this needs to exist to link the h file
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <libgen.h> 
#include <signal.h> 
#include <sys/signalfd.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <linux/limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/syslog.h>
#include "include/reportman.h"
#include <bits/sigaction.h>
#include <bits/types/sigset_t.h>
#include <sys/select.h>


int __parse_signed_int_arg(char * input);
bool __parse_bool_arg(char * input);
void to_lower_case(char *str);
static int __initialize_pipes(int daemon_pipes[2], int child_pipes[2]);

bool ipc_is_command(unsigned long msg){
    if ((msg & R_IPC_COMMAND_FLAG) != R_IPC_COMMAND_FLAG)
        return false;
    return true;
}

bool ipc_is_ack(unsigned long msg) {
    if ((msg & IPC_COMMAND_ACK) != IPC_COMMAND_ACK)
        return false;
    return true;
}

bool ipc_is_health_probe(unsigned long msg) {
    if ((msg & IPC_COMMAND_HEALTH_PROBE) != IPC_COMMAND_HEALTH_PROBE)
        return false;
    return true;
}

bool ipc_is_yes(unsigned long msg) {
    if ((msg & IPC_COMMAND_YES) != IPC_COMMAND_YES)
        return false;
    return true;
}
bool ipc_is_no(unsigned long msg) {
    if ((msg & IPC_COMMAND_NO) != IPC_COMMAND_NO)
        return false;
    return true;
}

bool ipc_is_unint(unsigned long msg) {
    if ((msg & R_IPC_VALUE_UINT_FLAG) != R_IPC_VALUE_UINT_FLAG)
        return false;
    return true;
}

bool ipc_send_yes(int pipe_id) {
    unsigned long msg[] = {R_IPC_COMMAND_YES};
    if(write(pipe_id, &msg, sizeof(unsigned long)) == -1) {
        return false;
    };
    return true;
}
bool ipc_send_no(int pipe_id) {
    unsigned long msg[] = {R_IPC_COMMAND_NO};
    if(write(pipe_id, &msg, sizeof(unsigned long)) == -1) {
        return false;
    };
    return true;
}
bool ipc_is_ulong(unsigned long msg) {
    if ((msg & R_IPC_VALUE_UINT_FLAG) == R_IPC_VALUE_UINT_FLAG)
        return true;
    return false;
}
bool ipc_send_ack(ipc_pipes_t *pipes) {
    unsigned long value = R_IPC_COMMAND_ACK;
    if(write(pipes->write, &value, sizeof(unsigned long)) == -1) {
        return false;
    };
    return true;
}
bool ipc_send_health_probe(ipc_pipes_t *pipes) {
    unsigned long value = R_IPC_COMMAND_HEALTH_PROBE;
    if(write(pipes->write, &value, sizeof(unsigned long)) == -1) {
        return false;
    };
    return true;
}
bool ipc_send_ulong(ipc_pipes_t *pipes, unsigned long value) {
    if (value > R_IPC_VALUE_UINT_MAX) // max value
        return false;
    
    unsigned long msg[] = {R_IPC_VALUE_UINT_FLAG | value};
    if(write(pipes->write, &msg, sizeof(unsigned long)) == -1) {
        return false;
    };
    return true;
}
unsigned long ipc_get_ulong(ipc_pipes_t *pipes) {
    unsigned long msg;
    if(read(pipes->read, &msg, sizeof(unsigned long))<0)
        return R_IPC_VALUE_UINT_FLAG;

    if (!ipc_is_ulong(msg)) {
        return R_IPC_VALUE_UINT_FLAG;
    } 
    return msg & ~R_IPC_VALUE_UINT_FLAG;
}
bool ipc_send_command(ipc_pipes_t *pipes, IPC_COMMANDS  command) {
    unsigned long value = command;
    if(write(pipes->write, &value, sizeof(unsigned long)) == -1) {
        return false;
    };
    return true;
}
bool ipc_get_command(ipc_pipes_t *pipes, IPC_COMMANDS *command) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(pipes->read, &readfds);
    struct timeval timeout;
    timeout.tv_sec = R_IPC_TIMEOUT;
    timeout.tv_usec = 0;
        // Set up an alarm for the timeout
    int ret = select(pipes->read + 1, &readfds, NULL, NULL, &timeout);
    if(ret == -1){
        syslog(LOG_ERR, "Error in select: %s", strerror(errno));
        return false;
    } else if(ret == 0) {
        syslog(LOG_ERR, "Timeout in select");
        return false;
    } else {
        unsigned long msg;
        if(read(pipes->read, &msg, sizeof(unsigned long))<0)
            return false;
        if (!(ipc_is_command(msg)))
            return false; 
        *command = (IPC_COMMANDS)msg;
        return true;
    }
}
bool ipc_get_ack(ipc_pipes_t *pipes) {
    unsigned long msg;
    if(read(pipes->read, &msg, sizeof(unsigned long))<0)
        return false;
    return ipc_is_ack(msg);
}
bool ipc_get_health_probe(ipc_pipes_t *pipes) {
    unsigned long msg;
    if(read(pipes->read, &msg, sizeof(unsigned long))<0)
        return false;
    return ipc_is_health_probe(msg);
}

bool ipc_get_no(ipc_pipes_t *pipes) {
    unsigned long msg;
    if(read(pipes->read, &msg, sizeof(unsigned long))<0)
        return false;
    return ipc_is_no(msg);
}
bool ipc_get_yes(ipc_pipes_t *pipes) {
    unsigned long msg;
    if(read(pipes->read, &msg, sizeof(unsigned long))<0)
        return false;
    return ipc_is_yes(msg);
}

bool ipc_send_test_data(ipc_pipes_t *pipes, const test_data_t *data) {
    

    if(write(pipes->write, data, sizeof(test_data_t)) == -1) {
        return false;
    };
    size_t test_string_len = strlen(data->test_string);
    if(write(pipes->write, data->test_string, sizeof(char) * test_string_len) == -1) {
        return false;
    };

    return true;
}


int ipc_get_test_data(ipc_pipes_t *pipes, test_data_t *data) {

    if(read(pipes->read, data, sizeof(test_data_t))<0)
        return false;
    ssize_t read_str = read(pipes->read, data->test_string, sizeof(char) * COMMUNICATION_BUFFER_SIZE);
    if(read_str<0)
        return false;
    data->test_string[read_str] = '\0';

    return true;
}

int acknowledge_daemon(ipc_pipes_t *pipes)
{
    
    if (!ipc_send_ack(pipes))
    {
        syslog(LOG_ERR, "Failed to send ack to daemon.");
        return D_FAILURE;
    }
    if (!ipc_get_ack(pipes))
    {
        syslog(LOG_ERR, "Failed to get ack from daemon.");
        return D_FAILURE;
    }

    return D_SUCCESS;
}

unsigned short __parse_short_arg(char * input) {
    char *endptr;
    long value = strtol(input, &endptr, 10);
    if (endptr == input) {
        printf("Argument could not be converted to a short : %s\n", input);
        exit(EXIT_FAILURE);
    }
    if (value < 0 || value > 0b1111111111111111) {
        printf("Argument out of range: %s\n", input);
        exit(EXIT_FAILURE);
    }
    return (unsigned short)value;
}

unsigned int __parse_unsigned_int_arg(char * input){
    char *endptr;
    long value = strtol(input, &endptr, 10);
    if (endptr == input) {
        printf("Argument could not be converted to an unsigned int : %s\n", input);
        exit(EXIT_FAILURE);
    }
    if (value < 0 || value > 0b11111111111111111111111111111111) {
        printf("Unsigned int argument out of range: %s\n", input);
        exit(EXIT_FAILURE);
    }
    return (unsigned int)value;
}
int __parse_signed_int_arg(char * input){
    char *endptr;
    long value = strtol(input, &endptr, 10);
    if (endptr == input) {
        printf("Argument could not be converted to an unsigned int : %s\n", input);
        exit(EXIT_FAILURE);
    }
    if (value < -2147483648 || value > 2147483647) {
        printf("Signed int argument out of range: %s\n", input);
        exit(EXIT_FAILURE);
    }
    return (int)value;
}
bool __parse_bool_arg(char * input){
    char *endptr;
    to_lower_case(input);
    if(strcmp(input, "true"))
        return true;
    if(strcmp(input, "false"))
        return false;
    long value = strtol(input, &endptr, 10);
    if (endptr == input) {
        printf("Argument could not be converted to an unsigned int : %s\n", input);
        exit(EXIT_FAILURE);
    }
    if (value < 0 || value > 1) {
        printf("Bool argument out of range: %s\n", input);
        exit(EXIT_FAILURE);
    }
    return (bool)value;
}
/// @brief parses command to its representative enum
/// @param command 
/// @return 
int parse_command(char *command, command_response_t * response) {
    to_lower_case(command);
    if (strcmp(command, "backup") == 0) {
        response->command = C_BACKUP;
        return COMMAND_SUCCESSFUL;
    } else if (strcmp(command, "transfer") == 0) {
        response->command = C_TRANSFER;
        return COMMAND_SUCCESSFUL;
    }
    else if (strcmp(command, "close") == 0) {
        response->command = C_CLOSE;
        return COMMAND_SUCCESSFUL;
    
    }
    else if (strcmp(command, "get_timers") == 0) {
        response->command = C_GETTIMERS;
        return COMMAND_SUCCESSFUL;
    
    } else {
        response->command = C_UNKNOWN; 
        return COMMAND_NOT_FOUND;
    }
}

void to_lower_case(char *str) {
    if (str == NULL) return;

    for (int i = 0; str[i]; i++) {
        str[i] = (char)tolower((unsigned char) str[i]);
    }
}



int arg_parse_string(int argc, char *argv[], int *argi, const char * short_name, const char * long_name,const char * value_name, char *output){
    if(!(strcmp(argv[*argi], short_name) == 0 || strcmp(argv[*argi], long_name) == 0))
        return R_PARSE_NO_MATCH;
    
    if (*argi + 1 >= argc)
    {
        fprintf(stderr, "No %s specified with %s\n", value_name, argv[*argi]);
        exit(EXIT_FAILURE);
    }

    strcpy(output, argv[++(*argi)]);
    return R_PARSE_SUCCESS;
    
}

int arg_parse_ushort(int argc, char *argv[], int *argi, const char * short_name, const char * long_name,const char * value_name, unsigned short *output){
    if(!(strcmp(argv[*argi], short_name) == 0 || strcmp(argv[*argi], long_name) == 0))
        return R_PARSE_NO_MATCH;
    

    if (*argi + 1 >= argc)
    {
        fprintf(stderr, "No %s specified with %s\n", value_name, argv[*argi]);
        exit(EXIT_FAILURE);
    }

    *output = __parse_short_arg(argv[++(*argi)]); 

    return R_PARSE_SUCCESS;
    
}
int arg_parse_flag(char *arg, const char * short_name, const char * long_name){
    if((strcmp(arg, short_name) == 0 || strcmp(arg, long_name) == 0)){
        return R_PARSE_SUCCESS;
    }

    return R_PARSE_NO_MATCH;
}
int arg_parse_uint(int argc, char *argv[], int *argi, const char * short_name, const char * long_name,const char * value_name, unsigned int *output) {
    if(!(strcmp(argv[*argi], short_name) == 0 || strcmp(argv[*argi], long_name) == 0))
        return R_PARSE_NO_MATCH;
    if (*argi + 1 >= argc)
    {
        fprintf(stderr, "No %s specified with %s\n", value_name, argv[*argi]);
        exit(EXIT_FAILURE);
    }

    *output = __parse_unsigned_int_arg(argv[++(*argi)]); 

    return R_PARSE_SUCCESS;
}

int arg_parse_int(int argc, char *argv[], int *argi, const char * short_name, const char * long_name,const char * value_name, int *output) {
    if(!(strcmp(argv[*argi], short_name) == 0 || strcmp(argv[*argi], long_name) == 0))
        return R_PARSE_NO_MATCH;
    if (*argi + 1 >= argc)
    {
        fprintf(stderr, "No %s specified with %s\n", value_name, argv[*argi]);
        exit(EXIT_FAILURE);
    }

    *output = __parse_signed_int_arg(argv[++(*argi)]); 

    return R_PARSE_SUCCESS;
}

int arg_parse_bool(int argc, char *argv[], int *argi, const char * short_name, const char * long_name,const char * value_name, bool *output) {
    if(!(strcmp(argv[*argi], short_name) == 0 || strcmp(argv[*argi], long_name) == 0))
        return R_PARSE_NO_MATCH;
    if (*argi + 1 >= argc)
    {
        fprintf(stderr, "No %s specified with %s\n", value_name, argv[*argi]);
        exit(EXIT_FAILURE);
    }

    *output = __parse_bool_arg(argv[++(*argi)]); 
    return R_PARSE_SUCCESS;
}

int arg_unrecognised(char * arg) {
    return strncmp(arg, "-", 1) == 0;
}

/// @brief Converts HH:MM string to next time_t of that time
/// @param input_HH_MM 
/// @param next_time 
/// @return Success of function
int get_hh_mm_str(const char * input_HH_MM,time_t * next_time) {
    unsigned short target_hour;
    unsigned short target_minute;
    if( sscanf(input_HH_MM, "%hu:%hu", &target_hour, &target_minute) != 2) {
        return(D_INVALID_HH_MM_FORMAT);
    }

    if (target_hour > 23 || target_minute > 59) {
        return(D_INVALID_HH_MM_RANGE);
    }

    time_t now = time(NULL);
    struct tm *next_target = localtime(&now);

    next_target->tm_min = (int)target_minute;
    next_target->tm_hour = (int)target_hour;
    next_target->tm_sec = 0;
    if (difftime(mktime(next_target), now) < 0) {
        next_target->tm_mday += 1;
    }
    *next_time = mktime(next_target);
    return(D_SUCCESS);
}

/// @brief Creates a child process, mirrors new process and returns communication pipe ids
/// @param executable
/// @param extra_args
/// @param child_process empty child process struct
/// @param d_socket connection socket
/// @return pid of child
int start_child_process(child_process_t *child_process, const char *extra_args[], int d_socket)
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
        syslog(LOG_ERR, "reportman could not spawn %s child: %s", child_process->executable, strerror(errno));
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

        int arg_count=0;
        if (extra_args != NULL) {
            for (arg_count = 0; extra_args[arg_count] != NULL; ++arg_count)
            {
                if (arg_count > D_MAX_INTERPROCESS_ARGUMENTS)
                {
                    syslog(LOG_ERR, "Too many arguments passed to child process, or we are missing the NULL array terminator.");
                    exit(EXIT_FAILURE);
                }
            }
        }

        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wcast-qual"
        const int fixed_args = 6;
        const char *args[arg_count + fixed_args]; // Extra space for fixed arguments and NULL terminator
        args[0] = child_process->executable;                     // First arg is the name of the program
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

        execv(child_process->executable, (char *const *)args);

        #pragma GCC diagnostic pop

        int err = errno;

        syslog(LOG_ERR, "execv FAILED to overlay %s", strerror(err));

        if (err == 2)
        {
            syslog(LOG_ERR, "Could not find %s executable. Please ensure it is executable from call location.", child_process->executable);
        }

        exit(EXIT_FAILURE);
    }
    close(parent_pipes[0]);
    close(child_pipes[1]);

    child_process->pipes.write = parent_pipes[1];
    child_process->pipes.read = child_pipes[0];

    syslog(LOG_DEBUG, "%s child spawned with PID: %d", child_process->executable, fork_pid);
    child_process->pid = fork_pid;
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


int r_initialize_signals(void)
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
    // signal(SIGINT, __clean_close);
    // signal(SIGTERM, __clean_close);
    return signal_fd;
}