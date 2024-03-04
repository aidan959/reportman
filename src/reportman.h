#ifndef REPORTMAND_BIND_PORT
#define COMMUNICATION_BUFFER_SIZE 1024
#define REPORTMAND_BIND_PORT 7770
#define IS_SINGLETON 0
#define BIND_FAILED -1
#define COMMAND_NOT_FOUND 127
#define COMMAND_SUCCESSFUL 0
#define COMMAND_ERROR 1
#define LSOF_FD_NOT_FOUND 2
#define MAX_CLIENT_QUEUE 5
#define D_SUCCESS 0
#define D_INVALID_HH_MM_FORMAT -1
#define D_INVALID_HH_MM_RANGE -2
#define D_UNIMPLEMENT -2

#define D_INTERVAL (24 * 60 * 60)

#include <stdbool.h>
typedef enum {
    C_BACKUP,
    C_TRANSFER,
    C_EXIT,
    C_UNKNOWN
} command_t;
typedef struct
{
    bool make_daemon;
    bool force;
    unsigned short daemon_port;
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


void configure_daemon_args(int argc, char *argv[], daemon_arguments_t *args);
void configure_client_args(int argc, char *argv[], client_arguments_t *args);
int parse_command(char *command, command_response_t * response) ;
int main(int argc, char *argv[]);

//const char REPORTS_DIRECTORY[] = "/srv/allfactnobreak/reports";
//const char BACKUP_DIRECTORY[] = "/srv/allfactnobreak/backup";
//const char DASHBOARD_DIRECTORY[] = "/srv/allfactnobreak/dashboard";
#endif
