#define _POSIX_C_SOURCE 199309L // for POSIX timers
#define OPENSSL_API_COMPAT 10002
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <signal.h>
#include <time.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include "backup_tool.h"

static const char * __back_up_source_directory;
static const char * __back_up_target_directory;


static void __back_up_directory(int sigg_no);
static void __set_timer_signals(struct sigaction* act, void(*handler)(int) );
static void __create_timer_abs(struct sigevent * sev, struct itimerspec * its, timer_t * timerid, time_t time, time_t repeat_interval);
static void __start_timer(timer_t timerid, struct itimerspec * its);

/// @brief Sets a timer with callback to a backup function.
/// @param source_directory 
/// @param target_directory 
/// @param time Must be an absolute value. Time in seconds for timer to set off atTODO / maybe change?
/// @param repeat_interval After first time is reached, 
/// @return 
timer_t backup_at_time(const char * source_directory, const char * target_directory, time_t time, time_t repeat_interval){
    struct sigaction act;
    struct sigevent sev;
    struct itimerspec its;
    timer_t timer_id;

    __back_up_source_directory = source_directory;
    __back_up_target_directory = target_directory;

    __set_timer_signals(&act, __back_up_directory);
    __create_timer_abs(&sev, &its, &timer_id, time, repeat_interval);
    __start_timer(timer_id, &its);

    char time_string[26];
    struct tm * tm_info;
    tm_info = localtime(&time);
    strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", tm_info);
    syslog(LOG_NOTICE,
           "Back-up of %s to %s scheduled for %s.",
           source_directory,
           target_directory,
           time_string);

    return timer_id;
}
static void __back_up_directory(int sigg_no){
    switch (sigg_no){
        case __SIGRTMIN:
            syslog(LOG_NOTICE, "Backup executed by timer.");
            break;
        default:
            syslog(LOG_WARNING, "Timer receieved unexpected signal: %s (%d)", strsignal(sigg_no), sigg_no);
            return;
    }
    transact_move_file(__back_up_source_directory, __back_up_target_directory);
}


/// @brief Moves file from source to destination - hashes the file before removing.
/// @param source_path 
/// @param dest_path 
/// @return Successfully moved file.
int transact_move_file(const char *source_path, const char *dest_path) {
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

    remove(source_path);
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
void move_directory(const char *source, const char *destination) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char src_path[1024];
    char dest_path[1024];

    // Create the destination directory
    mkdir(destination, 0777);

    if ((dir = opendir(source)) == NULL) {
        perror("Failed to open directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if(snprintf(src_path, sizeof(src_path), "%s/%s", source, entry->d_name) < 0){
            syslog(LOG_CRIT, "Snprintf: %s",  strerror(errno));
        }
        if(snprintf(dest_path, sizeof(dest_path), "%s/%s", destination, entry->d_name)){
            syslog(LOG_CRIT, "Snprintf: %s",  strerror(errno));
        }

        if (stat(src_path, &statbuf) == -1) {
            syslog(LOG_CRIT,"Stat: %s", strerror(errno));
            continue;
        }
        
        if (S_ISDIR(statbuf.st_mode)) {
            move_directory(src_path, dest_path);
            continue;
        }
        syslog(LOG_NOTICE, "Backing up %s to %s\n", src_path, dest_path);
        if(transact_move_file(src_path, dest_path) != TRANSACT_SUCCESS){
            syslog(LOG_ERR, "Backing up %s to %s FAILED. TRANSACT CANCELLED.\n", src_path, dest_path);

        }
        
    }
    closedir(dir);
}
static void __set_timer_signals(struct sigaction* act, void(*handler)(int) ) {  
    // Set up signal handler
    act->sa_flags = SA_SIGINFO;
    act->sa_handler = handler;
    //act.sa_sigaction = &handler;
    sigemptyset(&act->sa_mask);
    if (sigaction(SIGRTMIN, act, NULL) < 0) {
        syslog(LOG_ERR, "Could not set sigaction. %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

static void __create_timer_abs(struct sigevent * sev, struct itimerspec * its, timer_t * timerid, time_t time, time_t repeat_interval){
    // Set up timer
    sev->sigev_notify = SIGEV_SIGNAL;
    sev->sigev_signo = SIGRTMIN;
    sev->sigev_value.sival_ptr = timerid;
    if (timer_create(CLOCK_BOOTTIME_ALARM, sev, timerid) < 0) {
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