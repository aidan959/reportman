#define _POSIX_C_SOURCE 199309L // for POSIX timers
#define OPENSSL_API_COMPAT 10002
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <dirent.h>
#include <time.h>
#include "directory_tool.h"

static const char * __back_up_source_directory;
static const char * __back_up_target_directory;

static const char * __transfer_source_directory;
static const char * __transfer_target_directory;

static int __create_dir_if_not_exist(const char *directory,  unsigned int mode);
static void __back_up_directory(int sigg_no);
static void __transfer_directory(int sigg_no);
static void __set_timer_signals(struct sigaction* act, void(*handler)(int), transfer_method method );
static void __create_timer_abs(struct sigevent * sev, struct itimerspec * its, timer_t * timerid, time_t time, time_t repeat_interval, transfer_method method);
static void __start_timer(timer_t timerid, struct itimerspec * its);

const char * str_transfer_name(transfer_method method) {
    switch (method) {
    case BACKUP: return "BACKUP"; 
    case TRANSFER: return "TRANSFER";
    default: return "UNDEFINED";
    }
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


/// @brief Sets a timer with callback to a move function.
/// @param source_directory 
/// @param target_directory 
/// @param time Must be an te value. Time in seconds for timer to set off at TODO / maybe change?
/// @param repeat_interval After first time is reached, 
/// @return 
timer_t transfer_at_time(const char * source_directory, const char * target_directory, time_t time, time_t repeat_interval, transfer_method method){
    struct sigaction act;
    struct sigevent sev;
    struct itimerspec its;
    timer_t timer_id;


    void (*handler)(int);
    switch(method) {
    case TRANSFER:
        __transfer_source_directory = source_directory;
        __transfer_target_directory = target_directory;
        handler = __transfer_directory;
        break;

    case BACKUP:
        __back_up_source_directory = source_directory;
        __back_up_target_directory = target_directory;
        handler = __back_up_directory;
        break;

    default:
        syslog(LOG_ERR,
           "Unimplimented mode: %s (%d)", str_transfer_name(method), method);
        return (void *)UNIMPLEMENTED_TRANSFER_METHOD;
    }

    __set_timer_signals(&act, handler, method);
    __create_timer_abs(&sev, &its, &timer_id, time, repeat_interval, method);
    __start_timer(timer_id, &its);

    char time_string[26];
    struct tm * tm_info;
    tm_info = localtime(&time);
    strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", tm_info);
    syslog(LOG_NOTICE,
           "%s of %s to %s scheduled for %s.",
           str_transfer_name(method),
           source_directory,
           target_directory,
           time_string);

    return timer_id;
}

/// @brief Moves file from source to destination - hashes the file before removing.
/// @param source_path 
/// @param dest_path 
/// @param method method of transfer
/// @return INT determining success
int transact_transfer_file(const char *source_path, const char *dest_path, transfer_method method) {
    FILE *source, *dest;
    char buffer[1024];
    size_t bytes;

    source = fopen(source_path, "rb");
    if (source == NULL) {
        syslog(LOG_ERR, "Error opening source file: %s", strerror(errno));
        return TRANSACT_ERR_OPEN;
    }

    dest = fopen(dest_path, "wb");
    if (dest == NULL) {
        syslog(LOG_ERR, "Error opening destination file: %s", strerror(errno));
        fclose(source);
        return TRANSACT_ERR_OPEN;
    }

    while ((bytes = fread(buffer, sizeof(char), sizeof(buffer), source)) > 0) {
        fwrite(buffer, sizeof(char), bytes, dest);
    }
    
    if (verify_files(source, dest) < 0) {
        syslog(LOG_ERR,
            "Destination file (%s) hash did not match source file (%s) hash. File will not be removed from source.",source_path, dest_path);
        fclose(dest);
        fclose(source);
        return TRANSACT_HASH_FAILED;
    }

    fclose(dest);
    fclose(source);
    switch(method){
        case TRANSFER:
            remove(source_path);
        case BACKUP:
        default:
            break;
    }
    return TRANSACT_SUCCESS;
}

/// @brief Compares two files SHA256 hash values.
/// @param source
/// @param dest 
int verify_files(FILE *source, FILE * dest){
    unsigned char source_hash[SHA256_DIGEST_LENGTH];
    unsigned char dest_hash[SHA256_DIGEST_LENGTH];

    hash_file(source_hash, source);
    hash_file(dest_hash, dest);

    if (memcmp(source_hash, dest_hash, SHA256_DIGEST_LENGTH)==0){
        return 0;
    }

    return -1;    

}

/// @brief Hashes source file's bytes.
/// @param sh256_hash returns the hashed version of source into this.
/// @param source will reset seek to 0 and seek to end
void hash_file(unsigned char * sha256_hash, FILE *source){ 
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    unsigned char buffer[1024];
    size_t bytes = 0;
    if (fseek(source, 0, SEEK_SET) < 0) {
        syslog(LOG_ERR, "Error seeking: %s", strerror(errno));
    }

    while ((bytes = fread(buffer, sizeof(char), sizeof(buffer), source))) {
        SHA256_Update(&sha256, buffer, bytes);
    }

    SHA256_Final(sha256_hash, &sha256);
}

/// @brief Recursively a directory to destination from source.
/// @param source 
/// @param destination 
/// @param method 
/// @return number of files which caused errors
int transfer_directory(const char *source, const char *destination, transfer_method method) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char src_path[1024];
    char dest_path[1024];
    int no_errors = 0;
    // Create the destination directory
    mkdir(destination, 0777);

    if ((dir = opendir(source)) == NULL) {
        syslog(LOG_CRIT,"Failed to open directory: %s", strerror(errno));
        return 1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        snprintf(src_path, sizeof(src_path), "%s/%s", source, entry->d_name);
        snprintf(dest_path, sizeof(dest_path), "%s/%s", destination, entry->d_name);

        if (stat(src_path, &statbuf) == -1) {
            syslog(LOG_CRIT,"Stat: %s", strerror(errno));
            continue;
        }
        
        if (S_ISDIR(statbuf.st_mode)) {
            no_errors += transfer_directory(src_path, dest_path, method);
            continue;
        }
        // todo MAKE THIS A FUNCTION / DRY
        switch (method) {
        case BACKUP:
            syslog(LOG_INFO, "Backing up %s to %s\n", src_path, dest_path);
            if(transact_transfer_file(src_path, dest_path, BACKUP) != TRANSACT_SUCCESS){
                syslog(LOG_ERR, "Backing up %s to %s FAILED. TRANSACT CANCELLED.\n", src_path, dest_path);
                no_errors += 1;
            } else {
                syslog(LOG_INFO, "Successfully backed up %s to %s\n", src_path, dest_path);
            }
            break;  
        case TRANSFER:
            syslog(LOG_INFO, "Transfering %s to %s\n", src_path, dest_path);
            if(transact_transfer_file(src_path, dest_path, TRANSFER) != TRANSACT_SUCCESS){
                syslog(LOG_ERR, "Transfering %s to %s FAILED. TRANSACT CANCELLED.\n", src_path, dest_path);
                no_errors += 1;
            } else {
                syslog(LOG_INFO, "Successfully transfered %s to %s\n", src_path, dest_path);
            }
            break;
        default:
            no_errors += 1;
            syslog(LOG_ERR, "Unkown transfer method used.");
        }
        
    }
    closedir(dir);
    return no_errors;
}

static void __set_timer_signals(struct sigaction* act, void(*handler)(int), transfer_method method ) {  
    // Set up signal handler
    act->sa_flags = SA_SIGINFO;
    act->sa_handler = handler;
    //act.sa_sigaction = &handler;
    sigemptyset(&act->sa_mask);
    if (sigaction(SIGRTMIN + (int)method, act, NULL) < 0) {
        syslog(LOG_ERR, "Could not set sigaction. %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

static void __create_timer_abs(struct sigevent * sev, struct itimerspec * its, timer_t * timerid, time_t time, time_t repeat_interval, transfer_method method){
    
    // Set up timer
    sev->sigev_notify = SIGEV_SIGNAL;
    sev->sigev_signo = SIGRTMIN + (int)method;
    sev->sigev_value.sival_ptr = timerid;
    // TODO CHANGE TO CLOCK_BOOTTIME_ALARM 
    if (timer_create(CLOCK_REALTIME, sev, timerid) < 0) {
        syslog(LOG_ERR, "Could not create timer (). %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    // Set timer expiration time
    its->it_value.tv_sec = time;
    its->it_value.tv_nsec = 0;
    its->it_interval.tv_sec = repeat_interval;
    its->it_interval.tv_nsec = 0;
}

static void __start_timer(timer_t timerid, struct itimerspec * its) {
    // Start the timer
    if (timer_settime(timerid, TIMER_ABSTIME , its, NULL) < 0) {
        syslog(LOG_ERR, "Could not start timer. %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}
static int __create_dir_if_not_exist(const char *directory,  unsigned int mode)
{
    struct stat st = {0};
    
    if (stat(directory, &st) == -1)
        return mkdir(directory, mode);
    return 0;
}
static void __back_up_directory(int sigg_no){
    openlog("reportmand_directory_tool", LOG_PID, LOG_DAEMON);
    switch (sigg_no){
        case 34:
            break;
        default:
            syslog(LOG_WARNING, "Timer receieved unexpected signal: %s (%d)", strsignal(sigg_no), sigg_no);
            return;
    }
    syslog(LOG_NOTICE, "Backup of %s to %s executed by timer.", __back_up_source_directory, __back_up_target_directory);
    transfer_directory(__back_up_source_directory, __back_up_target_directory, BACKUP);
    syslog(LOG_NOTICE, "Backup of %s to %s completed by timer.", __back_up_source_directory, __back_up_target_directory);
}

static void __transfer_directory(int sigg_no){
    openlog("reportmand_directory_tool", LOG_PID, LOG_DAEMON);
    switch (sigg_no){
        case 35:
            break;
        default:
            syslog(LOG_WARNING, "Timer receieved unexpected signal: %s (%d)", strsignal(sigg_no), sigg_no);
            return;
    }
    syslog(LOG_NOTICE, "Transfer of %s to %s executed by timer.", __transfer_source_directory, __transfer_target_directory);
    transfer_directory(__transfer_source_directory, __transfer_target_directory, TRANSFER);
    syslog(LOG_NOTICE, "Transfer of %s to %s completed by timer.", __transfer_source_directory, __transfer_target_directory);
}

int gettime(timer_t timer_id,
        struct itimerspec *curr_value){
    return timer_gettime(timer_id, curr_value);
}