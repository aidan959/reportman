#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "directory_tool.h"



static int __create_dir_if_not_exist(const char *directory,  unsigned int mode)
{
    struct stat st = {0};
    
    if (stat(directory, &st) == -1)
        return mkdir(directory, mode);
    return 0;
}

int init_directories(int num_dirs, const char **dirs)
{
    for (int i = 0; i < num_dirs; i++)
    {
        int create_resp = __create_dir_if_not_exist(dirs[i], 0700);
        if(create_resp < 0) return -1;
    }
    return 0;
}
