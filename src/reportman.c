// Shared daemon and client functionality -> using this currently for header values and this needs to exist to link the h file
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "reportman.h"

unsigned short __parse_short_arg(char * input);
static int __starts_with(const char *str, const char *prefix);
static int __get_hh_mm_str(const char * input_HH_MM,time_t * next_time);

void to_lower_case(char *str);
void configure_daemon_args(int argc, char *argv[], daemon_arguments_t *args)
{
    int i;
    for (i = 1; i < argc; i++)
    {

        if (strcmp(argv[i], "--no-daemon") == 0)
        {
            args->make_daemon = false; 
            continue;
        }
        else if (strcmp(argv[i], "-p") == 0 ||strcmp(argv[i], "--port") == 0)
        {
            if (i + 1 >= argc)
            {
                printf("No port specified with %s\n", argv[i]);
                exit(1);
            }
            args->daemon_port =__parse_short_arg(argv[++i]); 
            continue;
        }
        else if (strcmp(argv[i], "-f") == 0||strcmp(argv[i], "--force") == 0)
        {
            args->force = true; 
        }
        else if (strcmp(argv[i], "-tt") == 0||strcmp(argv[i], "--transfer-time") == 0)
        {
            if (i + 1 >= argc)
            {
                printf("No time stamp specified with %s\n", argv[i]);
                exit(1);
            }
            time_t transfer_time;
            int response = __get_hh_mm_str(argv[++i], &transfer_time);
            if (response < 0){
                exit(response);
            }
            args->transfer_time = transfer_time; 
        }
        else if (strcmp(argv[i], "-bt") == 0||strcmp(argv[i], "--backup-time") == 0)
        {
            if (i + 1 >= argc)
            {
                printf("No time stamp specified with %s\n", argv[i]);
                exit(1);
            }
            time_t backup_time;
            int response = __get_hh_mm_str(argv[++i], &backup_time);
            if (response < 0){
                exit(response);
            }
            args->backup_time = backup_time; 
        }
        else if (strcmp(argv[i], "-c") == 0||strcmp(argv[i], "--close") == 0)
        {
            args->close = true; 
        }
        else
        {
            
        }
        
    }
    if (args->backup_time == 0) {
        time_t backup_time;
        int response = __get_hh_mm_str(D_DEFAULT_BACKUP_TIME, &backup_time);
        if (response < 0)
            exit(response);
        
        args->backup_time = backup_time; 
    }
    if (args->transfer_time == 0 ){
        time_t transfer_time;
        int response = __get_hh_mm_str(D_DEFAULT_BACKUP_TIME, &transfer_time);
        if (response < 0)
            exit(response);
        
        args->transfer_time = transfer_time; 
    }
}
  
void configure_client_args(int argc, char *argv[], client_arguments_t *args)
{
    char **command_list = malloc(((size_t)argc -1) * sizeof(char *));
    int command_count = 0;
    int i;
    for (i = 1; i < argc; i++)
    {

        if (strcmp(argv[i], "-p") == 0  || strcmp(argv[i], "--port") == 0)
        {
            if (i + 1 >= argc)
            {
                printf("No port specified after %s\n", argv[i]);
                exit(1);
            }
            args->daemon_port =__parse_short_arg(argv[++i]); 
        }
        else if(__starts_with(argv[i], "-"))
        {
            printf("Unrecognized option: %s\n", argv[i]);
            exit(EXIT_FAILURE);
        }
        else
        {
            command_list[command_count++] = argv[i];
        }


    }
    args->commands = command_list;
    args->num_commands = command_count;

}
static int __starts_with(const char *str, const char *prefix) {
    if (str == NULL || prefix == NULL) return 0;

    size_t lenstr = strlen(str);
    size_t lenprefix = strlen(prefix);
    
    if (lenprefix > lenstr) return 0;

    return strncmp(str, prefix, lenprefix) == 0;
}
unsigned short __parse_short_arg(char * input) {
    char *endptr;
    long value = strtol(input, &endptr, 10);
    if (endptr == input) {
        printf("Portnumber could not be converted to a short : %s\n", input);
        exit(1);
    }
    if (value < 0 || value > 65535) {
        printf("Portnumber out of range: %s\n", input);
        exit(1);
    }
    return (unsigned short)value;
}

/// @brief parses command to its representative enum
/// @param command 
/// @return 
int parse_command(char *command, command_response_t * response) {
    to_lower_case(command);
    if (strcmp(command, "backup") == 0) {
        response->command = C_BACKUP;
        return COMMAND_SUCCESSFUL;
    } else if (strcmp(command, "transfer") == 0) {
        response->command = C_TRANSFER;
        return COMMAND_SUCCESSFUL;
    }
    else if (strcmp(command, "close") == 0) {
        response->command = C_CLOSE;
        return COMMAND_SUCCESSFUL;
    
    }
    else if (strcmp(command, "get_timers") == 0) {
        response->command = C_GETTIMERS;
        return COMMAND_SUCCESSFUL;
    
    } else {
        response->command = C_UNKNOWN; 
        return COMMAND_NOT_FOUND;
    }
}

void to_lower_case(char *str) {
    if (str == NULL) return;

    for (int i = 0; str[i]; i++) {
        str[i] = (char)tolower((unsigned char) str[i]);
    }
}

/// @brief Converts HH:MM string to time_t
/// @param input_HH_MM 
/// @param next_time 
/// @return Success of function
static int __get_hh_mm_str(const char * input_HH_MM,time_t * next_time) {
    unsigned short target_hour;
    unsigned short target_minute;
    if( sscanf(input_HH_MM, "%hu:%hu", &target_hour, &target_minute) != 2) {
        fprintf(stderr,"Invalid time format: %s, should be HH:MM fromat", input_HH_MM);
        return(D_INVALID_HH_MM_FORMAT);
    }
    if (target_hour > 23 || target_minute > 59) {
        fprintf(stderr, "Invalid time. Ensure the hour is between 0-23 and minutes are between 0-59.\n");
        return(D_INVALID_HH_MM_RANGE);
    }

    time_t now = time(NULL);
    struct tm *next_target = localtime(&now);

    next_target->tm_min = (int)target_minute;
    next_target->tm_hour = (int)target_hour;
    next_target->tm_sec = 0;
    if (difftime(mktime(next_target), now) < 0) {
        next_target->tm_mday += 1;
    }
    *next_time = mktime(next_target);
    return(D_SUCCESS);
}