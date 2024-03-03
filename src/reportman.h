#ifndef REPORTMAND_BIND_PORT

#define REPORTMAND_BIND_PORT 7770
#define IS_SINGLETON 0
#define COMMAND_NOT_FOUND 127
#define COMMAND_SUCCESSFUL 0
#define COMMAND_ERROR 1

#include <stdbool.h>
typedef struct
{
    bool make_daemon;
    short daemon_port;
} execution_arguments_t;
typedef struct
{
    int pid;
    char * command;

} running_pid_t;
void config_args(int argc, char *argv[], execution_arguments_t *args);
int main(int argc, char *argv[]);
int d_acquire_singleton(int *sockfd, int singleton_port);

//const char REPORTS_DIRECTORY[] = "/srv/allfactnobreak/reports";
//const char BACKUP_DIRECTORY[] = "/srv/allfactnobreak/backup";
//const char DASHBOARD_DIRECTORY[] = "/srv/allfactnobreak/dashboard";
#endif
