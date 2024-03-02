#include <stdlib.h>
typedef struct
{
    bool make_daemon;
} execution_arguments_t;

void process_args(int argc, char *argv[], execution_arguments_t *args);
int main(int argc, char *argv[]);

const char REPORTS_DIRECTORY[] = "/srv/allfactnobreak/reports";
const char BACKUP_DIRECTORY[] = "/srv/allfactnobreak/backup";
const char DASHBOARD_DIRECTORY[] = "/srv/allfactnobreak/dashboard";