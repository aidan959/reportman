#include <stdbool.h>
#include <time.h>
#include <signal.h>
#define BACKUP_MODE (unsigned)0
#define TRANSFER_MODE (unsigned)1
#define COMMUNICATION_BUFFER_SIZE 1024


#define R_IPC_TIMEOUT 5

#define R_IPC_COMMAND_FLAG 0xFF000000
#define R_IPC_COMMAND_NO 0xFF000000
#define R_IPC_COMMAND_YES 0xFF000001
#define R_IPC_COMMAND_ACK 0xFF000002
#define R_IPC_COMMAND_PANIC 0xFFACFFAC

#define R_IPC_COMMAND_HEALTH_PROBE 0xFF000003
#define R_IPC_VALUE_UINT_FLAG 0xF0000000
#define R_IPC_VALUE_UINT_MAX 0xEFFFFFFF
#define IPC_TIMEOUT_ERR -3
typedef struct
{
    char *path;
    int wd;
} dir_monitored_t;

typedef struct
{
    int write;
    int read;
} ipc_pipes_t;

typedef struct
{
    dir_monitored_t *monitors;
    int no_monitors;
} monitor_t;

typedef struct
{
    const char * log_file;
    const char * log_sys_name;
    bool log_to_sys;
    bool log_to_file;
    bool log_to_stdout;
} monitor_conf_t;

typedef struct
{
    bool from_daemon;
    const char * const * dirs;
    size_t no_dirs;
    monitor_conf_t conf;
    ipc_pipes_t pipes;
} monitor_args_t;

typedef struct 
{
    bool from_daemon;
    bool debug;

    time_t backup_time;
    time_t transfer_time;
    bool do_backup;
    bool do_transfer;
    ipc_pipes_t pipes;
} reportman_fm_args_t;

typedef enum {
    C_BACKUP,
    C_TRANSFER,
    C_CLOSE,
    C_UNKNOWN,
    C_GETTIMERS
} command_t;


typedef enum {
    IPC_COMMAND_NO  = R_IPC_COMMAND_NO,
    IPC_COMMAND_YES = R_IPC_COMMAND_YES,
    IPC_COMMAND_ACK = R_IPC_COMMAND_ACK,
    IPC_COMMAND_PANIC = R_IPC_COMMAND_PANIC,
    IPC_COMMAND_HEALTH_PROBE = R_IPC_COMMAND_HEALTH_PROBE,
} IPC_COMMANDS;
typedef struct
{
    bool make_daemon;
    bool force;
    bool close;
    bool log_to_sys;
    bool log_to_file;
    const char *monitor_log_file_path;
    const char *monitor_log_sys_name;
    unsigned short daemon_port;
    const char* backup_time_str;
    const char* transfer_time_str;

} daemon_arguments_t;
typedef struct
{
    unsigned short daemon_port;
    char ** commands;
    int num_commands;
} client_arguments_t;
typedef struct
{
    int pid;
    char * command;
} running_pid_t;

typedef struct
{
    char * response;
    command_t command;
} command_response_t;

typedef enum  {
    BACKUP=BACKUP_MODE,
    TRANSFER=TRANSFER_MODE,
} transfer_method_t;

typedef struct {
    pid_t pid;
    ipc_pipes_t pipes;
    unsigned short max_retries;
    unsigned short retries;
    const char *executable;
    bool is_alive;
} child_process_t;


typedef struct {
    unsigned int test_num;
    char test_string[COMMUNICATION_BUFFER_SIZE];
    bool test_bool;
} test_data_t;

