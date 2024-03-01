#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "directory_tool.h"



static int __create_dir_if_not_exist(const char *directory, int mode)
{
    struct stat st = {0};
    
    if (stat(directory, &st) == -1)
        return mkdir(directory, mode);
}

int init_directories(int num_dirs, const char **dirs)
{
    for (int i = 0; i < num_dirs; i++)
    {
        __create_dir_if_not_exist(dirs[i], 0700);
    }
}
