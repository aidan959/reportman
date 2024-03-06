#define _POSIX_C_SOURCE 199309L // for POSIX timers

#include <stdlib.h>   
#include <string.h>   

#include <signal.h>   
#include <time.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>

#include "include/reportman.h"
#include "include/reportman_fm.h"
#include "include/directory_tool.h"


static timer_t __schedule_backup(time_t transfer_time);
static timer_t __schedule_transfer(time_t transfer_time);
void __configure_reportman_fm_args(int argc, char *argv[], reportman_fm_t *args);
static int __started_from_daemon(void);

static timer_t __backup_timer;
static timer_t __transfer_timer;


static reportman_fm_t __fm_conf = {
    .from_daemon = false,
    .daemon_to_fm_read_id = -1,
    .fm_to_daemon_write_id = -1,
    .backup_time = 0,
    .transfer_time = 0,

};
int main(int argc, char ** argv){
    __configure_reportman_fm_args(argc, argv, &__fm_conf);
    __started_from_daemon();
}

static int __started_from_daemon(void)
{
    if(!__fm_conf.from_daemon){
        return 0;
    }
    __schedule_backup(__fm_conf.backup_time);
    __schedule_transfer(__fm_conf.transfer_time);

    for(;;) {
        sleep(20);
    }

}
static timer_t __schedule_backup(time_t transfer_time)
{
    __backup_timer = transfer_at_time(R_REPORTS_DIRECTORY, R_DASHBOARD_DIRECTORY, transfer_time, (time_t)D_INTERVAL, BACKUP);
    if (__backup_timer == (void *)UNIMPLEMENTED_TRANSFER_METHOD)
    {
        syslog(LOG_ERR, "Unimplemented transfer method used.");
        return (void*)UNIMPLEMENTED_TRANSFER_METHOD;
    }
    return __backup_timer;
}
static timer_t __schedule_transfer(time_t transfer_time)
{
    __transfer_timer = transfer_at_time(R_DASHBOARD_DIRECTORY, R_BACKUP_DIRECTORY, transfer_time, (time_t)D_INTERVAL, TRANSFER);

    if (__transfer_timer == (void *)UNIMPLEMENTED_TRANSFER_METHOD)
    {
        syslog(LOG_ERR, "Unimplemented transfer method used.");
        return (void*)UNIMPLEMENTED_TRANSFER_METHOD;
    }
    return __transfer_timer;
}
void __configure_reportman_fm_args(int argc, char *argv[], reportman_fm_t *args) {
    int i;


    for (i = 1; i < argc; i++)
    {
        char arg_string[COMMUNICATION_BUFFER_SIZE];
        int arg_int[1];
        
        if (arg_parse_flag(argv[i], "-fd", "--from-daemon"))
        {
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
        else if (arg_parse_string(argc, argv, &i, "-tt", "--transfer-time", "time stamp", arg_string))
        {
            time_t transfer_time;
            int response = get_hh_mm_str(arg_string, &transfer_time);
            if (response < 0){
                exit(response);
            }
            args->transfer_time = transfer_time; 
            continue;

        }
        else if (arg_parse_string(argc, argv, &i, "-bt", "--backup-time", "time stamp", arg_string))
        {
            time_t backup_time;
            int response = get_hh_mm_str(arg_string, &backup_time);
            if (response < 0){
                exit(response);
            }
            args->backup_time = backup_time; 
            continue;
        }
        else if (arg_unrecognised(argv[i])) {
            syslog(LOG_ERR, "Argument '%s' is not defined", argv[i]);
            fprintf(stderr, "Argument '%s' is not defined", argv[i]);
            exit(EXIT_FAILURE);
        }
        
        if (!args->backup_time) {
            time_t backup_time;
            int response = get_hh_mm_str(D_DEFAULT_BACKUP_TIME, &backup_time);
            if (response < 0)
                exit(response);
            
            args->backup_time = backup_time; 
        }
        if (!args->transfer_time){
            time_t transfer_time;
            int response = get_hh_mm_str(D_DEFAULT_TRANSFER_TIME, &transfer_time);
            if (response < 0)
                exit(response);
            
            args->transfer_time = transfer_time; 
        }
    }
}
