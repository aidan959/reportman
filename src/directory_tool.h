#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
static int __create_dir_if_not_exist(const char *directory, int mode);
int init_directories(int num_dirs, const char **dirs);
int backup_directories(int num_dirs, const char **dirs);

