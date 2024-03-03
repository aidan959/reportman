// Shared daemon and client functionality -> using this currently for header values and this needs to exist to link the h file
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "reportman.h"
void config_args(int argc, char *argv[], execution_arguments_t *args)
{
    int i;
    for (i = 1; i < argc; i++)
    {

        if (strcmp(argv[i], "--no-daemon") == 0)
        {
            args->make_daemon = false; 
            continue;
        }
        else
        {
            /* Process non-optional arguments here. */
        }
    }
}