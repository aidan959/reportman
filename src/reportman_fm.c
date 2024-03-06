#define _POSIX_C_SOURCE 199309L // for POSIX timers

#include <stdlib.h>   
#include <string.h>   

#include <signal.h>   
#include <time.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>

#include "include/reportman_fm.h"
#include "libs/include/reportman.h"
#include "libs/include/directory_tool.h"


static timer_t __schedule_backup(time_t transfer_time);
static timer_t __schedule_transfer(time_t transfer_time);
void __configure_reportman_fm_args(int argc, char *argv[], reportman_fm_t *args);
static int __started_from_daemon(void);
static int __report_to_daemon(void);
static timer_t __backup_timer;
static timer_t __transfer_timer;


static reportman_fm_t __fm_conf = {
    .from_daemon = false,
    .daemon_to_fm_read_id = -1,
    .fm_to_daemon_write_id = -1,
    .backup_time = 0,
    .transfer_time = 0,
    .do_backup = false,
    .do_transfer = false,
};

int main(int argc, char ** argv){
    __configure_reportman_fm_args(argc, argv, &__fm_conf);
    __started_from_daemon();

    // was not called from daemon
    if (__fm_conf.do_backup) 
        transfer_directory(R_DASHBOARD_DIRECTORY, R_BACKUP_DIRECTORY, BACKUP);
    
    if (__fm_conf.do_transfer) 
        transfer_directory(R_REPORTS_DIRECTORY, R_DASHBOARD_DIRECTORY, TRANSFER);
    exit(EXIT_SUCCESS);
}

/// @brief Runs if executed from daemon
/// @param  
/// @return Discardable
static int __started_from_daemon(void)
{
    if(!__fm_conf.from_daemon){
        return 0;
    }
    if(__fm_conf.daemon_to_fm_read_id < 0 || __fm_conf.fm_to_daemon_write_id < 0) {
        syslog(LOG_ERR, "Pipes not received from daemon.");
        fprintf(stderr, "-fd can ONLY be called from a daemon.");
        return D_FAILURE;
    }
    __schedule_backup(__fm_conf.backup_time);
    __schedule_transfer(__fm_conf.transfer_time);

    __report_to_daemon();

    exit(EXIT_FAILURE);

    for(;;) {
        // spin lock and let directory_tool do its thing
        sleep(20);
    }
}

static int __report_to_daemon(void) {
    fprintf(stderr, "Waiting for ack from daemon.");

    if(ipc_send_ack(__fm_conf.fm_to_daemon_write_id)) {
        fprintf(stderr, "Successfully got ack from daemon.");
    }
    else{
        syslog(LOG_ERR, "Failed to send ack to daemon.");
        return D_FAILURE;
    }
    if(ipc_get_ack(__fm_conf.daemon_to_fm_read_id)) {
        fprintf(stderr, "Successfully got ack from daemon.");
    }
    else{
        syslog(LOG_ERR, "Failed to get ack from daemon.");
        return D_FAILURE;
    }
    // syslog(LOG_DEBUG, "Sending __backup_timer (%p) to parent", __backup_timer);
    // write(__fm_conf.fm_to_daemon_write_id, __backup_timer, sizeof(__backup_timer));
    // syslog(LOG_DEBUG, "Sending __transfer_timer (%p) to parent", __transfer_timer);

    // write(__fm_conf.fm_to_daemon_write_id, __transfer_timer, sizeof(__transfer_timer));
    return D_FAILURE;
} 

/// @brief Schedules a backup at UNIX time transfer_time
/// @param transfer_time 
/// @return POSIX timer value
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

/// @brief Schedules a transfer at UNIX time transfer_time
/// @param transfer_time 
/// @return POSIX timer value
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

/// @brief 
/// @param argc passed arguments
/// @param argv number of arguments
/// @param args reportman_fm configuration 
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
        else if (arg_parse_flag(argv[i], "-b", "--backup"))
        {
            args->do_backup = true; 
            continue;
        }
        else if (arg_parse_flag(argv[i], "-t", "--transfer"))
        {
            args->do_transfer = true; 
            continue;
        }
        else if (arg_unrecognised(argv[i])) {
            syslog(LOG_ERR, "Argument '%s' is not defined", argv[i]);
            fprintf(stderr, "Argument '%s' is not defined", argv[i]);
            exit(EXIT_FAILURE);
        }

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
