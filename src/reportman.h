#define REPORTMAND_BIND

typedef struct
{
    bool make_daemon;
    short daemon_port;
} execution_arguments_t;

void config_args(int argc, char *argv[], execution_arguments_t *args);
