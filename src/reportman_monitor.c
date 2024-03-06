#include <errno.h>
#include <syslog.h>
#include <linux/limits.h>
#include "libs/include/reportman.h"
#include "libs/include/monitor_tool.h"
#include "include/reportman_monitor.h"

static void __configure_monitor_args(int argc, char *argv[], monitor_args_t *args);
static monitor_args_t __monitor_args;
static int __monitor_paths(void);
static int __started_from_daemon(void);
int main(int argc, char * argv[]) {
    const char* const dirs[3] = {
        R_BACKUP_DIRECTORY,
        R_DASHBOARD_DIRECTORY,
        R_REPORTS_DIRECTORY
    };

    monitor_args_t monitor_args = {
        .from_daemon = false,
        .dirs = dirs,
        .no_dirs = 3,
        .daemon_to_fm_read_id = -1,
        .fm_to_daemon_write_id = -1,
        .conf = {
            .log_file = M_LOG_PATH,
            .log_sys_name = "reportman_monitor",
            .log_to_sys = false,
            .log_to_file = true,
            .log_to_stdout = true
        }
    };
    __configure_monitor_args(argc, argv, &monitor_args);
    __started_from_daemon();

}
static int __monitor_paths(void)
{
    openlog("reportman_monitor", LOG_PID, LOG_DAEMON);
    int monitor_exit = monitor_paths(__monitor_args.no_dirs, __monitor_args.dirs, __monitor_args.conf);
    if (monitor_exit < 0)
    {
        syslog(LOG_ERR, "Path monitor exited unsuccessfully with value (%d)", monitor_exit);
    }
    else
    {
        syslog(LOG_NOTICE, "Path monitor exited successfully (%d).", monitor_exit);
    }
    return monitor_exit;
}
static void __configure_monitor_args(int argc, char *argv[], monitor_args_t *args){
    __monitor_args = *args;
    int i;
    bool default_dirs = true;
    size_t count = 1;
    size_t capacity = count;
    char **string_array = malloc(capacity * sizeof(char *));
    for (i = 1; i < argc; i++)
    {
        char arg_string[COMMUNICATION_BUFFER_SIZE];
        int arg_int[1];

        bool arg_bool[1];
        if(arg_parse_flag(argv[i], "-fd", "--from-daemon")){
            args->from_daemon = true;
            continue;

        } 
        else if(arg_parse_int(argc, argv, &i, "--d-to-c", "--d-to-c", "daemon to fm read pipe id", arg_int)) {
            args->daemon_to_fm_read_id = *arg_int;
            continue;
        }
        else if(arg_parse_int(argc, argv, &i, "--c-to-d", "--c-to-d", "fm to daemon write pipe id", arg_int)) {
            args->fm_to_daemon_write_id = *arg_int;
            continue;
        }
        else if(arg_parse_string(argc,argv, &i, "-lf", "--log-file", "log file path", arg_string)) {
            char* log_file = malloc(sizeof(char) * PATH_MAX);
            strcpy(log_file, arg_string);
            args->conf.log_file = log_file;
            continue;

        }
        else if(arg_parse_string(argc,argv, &i, "-ls", "--log-sys-name", "log sys name", arg_string)) {
            char* log_file = malloc(sizeof(char) * PATH_MAX);
            strcpy(log_file, arg_string);
            args->conf.log_sys_name = log_file;
            continue;

        }
        else if(arg_parse_bool(argc,argv, &i, "--log-to-file", "--log-to-file", "bool", arg_bool)) {
            args->conf.log_to_file = *arg_bool;
            continue;
        }
        else if(arg_parse_bool(argc,argv, &i, "--log-to-sys", "--log-to-sys", "bool", arg_bool)) {
            args->conf.log_to_sys = *arg_bool;
            continue;
        }
        else if (arg_unrecognised(argv[i])) {
            syslog(LOG_ERR, "Argument '%s' is not defined", argv[i]);
            fprintf(stderr, "Argument '%s' is not defined", argv[i]);
            exit(EXIT_FAILURE);
        }
        else {
            default_dirs = false;

            if(count == capacity) {
                capacity *= 2;
                string_array = realloc(string_array, capacity * sizeof(char*));
            }

            string_array[count] = malloc(strlen(argv[i]) + 1);
            strcpy(string_array[count], argv[i]);
            count++;
        }

        if (!default_dirs) {
            __monitor_args.dirs = (const char * const *)string_array;
            __monitor_args.no_dirs = count;

        }

    }
}

static int __started_from_daemon(void)
{
    if(!__monitor_args.from_daemon){
        return 0;
    }

    return __monitor_paths();

}