#include <stdbool.h>
#include <time.h>
#define BACKUP_MODE (unsigned)0
#define TRANSFER_MODE (unsigned)1



#define R_IPC_COMMAND_FLAG 0xFF000000
#define R_IPC_COMMAND_NO 0xFF000000
#define R_IPC_COMMAND_YES 0xFF000001
#define R_IPC_COMMAND_ACK 0xFF000002
#define R_IPC_COMMAND_STR 0xFF000002

#define R_IPC_VALUE_UINT_FLAG 0xF0000000
#define R_IPC_VALUE_UINT_MAX 0xEFFFFFFF

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
    IPC_COMMAND_FLAG= R_IPC_COMMAND_FLAG,
    IPC_COMMAND_NO  = R_IPC_COMMAND_NO,
    IPC_COMMAND_YES = R_IPC_COMMAND_YES,
    IPC_COMMAND_ACK = R_IPC_COMMAND_ACK,
    IPC_COMMAND_STR = R_IPC_COMMAND_STR,
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