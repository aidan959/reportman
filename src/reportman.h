#ifndef REPORTMAND_BIND_PORT
#define REPORTMAND_BIND_PORT 7770
#include <stdbool.h>
typedef struct
{
    bool make_daemon;
    short daemon_port;
} execution_arguments_t;

void config_args(int argc, char *argv[], execution_arguments_t *args);
int main(int argc, char *argv[]);

//const char REPORTS_DIRECTORY[] = "/srv/allfactnobreak/reports";
//const char BACKUP_DIRECTORY[] = "/srv/allfactnobreak/backup";
//const char DASHBOARD_DIRECTORY[] = "/srv/allfactnobreak/dashboard";
#endif
