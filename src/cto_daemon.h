#include <stdlib.h>
typedef struct
{
    bool make_daemon;
} execution_arguments_t;

void process_args(int argc, char *argv[], execution_arguments_t *args);
int main(int argc, char *argv[]);