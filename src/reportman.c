// Shared daemon and client functionality -> using this currently for header values and this needs to exist to link the h file
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <libgen.h> 
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <linux/limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "reportman.h"

unsigned short __parse_short_arg(char * input);
static int __starts_with(const char *str, const char *prefix);
static int __get_hh_mm_str(const char * input_HH_MM,time_t * next_time);
static int __get_path(const char * input_path, char * output_path);
static bool __is_path_empty(const char path[PATH_MAX]);
static bool __has_path_parent (const char * path);
static bool __has_write_permission_to_parent (const char path[PATH_MAX]);
static bool __has_write_permission_to_parent (const char path[PATH_MAX]);
static bool __expand_path_to_absolute(const char path[PATH_MAX], char output_path[PATH_MAX]);
static bool __remove_last_item_path(const char path[PATH_MAX], char directory[PATH_MAX], char item[PATH_MAX]) ;
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
                exit(EXIT_FAILURE);
            }
            args->daemon_port =__parse_short_arg(argv[++i]); 
            continue;
        }
        else if (strcmp(argv[i], "-f") == 0||strcmp(argv[i], "--force") == 0)
        {
            args->force = true; 
        }
        else if (strcmp(argv[i], "-c") == 0||strcmp(argv[i], "--close") == 0)
        {
            args->close = true; 
        }
        else if (strcmp(argv[i], "-tt") == 0||strcmp(argv[i], "--transfer-time") == 0)
        {
            if (i + 1 >= argc)
            {
                printf("No time stamp specified with %s\n", argv[i]);
                exit(EXIT_FAILURE);
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
                exit(EXIT_FAILURE);
            }
            time_t backup_time;
            int response = __get_hh_mm_str(argv[++i], &backup_time);
            if (response < 0){
                exit(response);
            }
            args->backup_time = backup_time; 
        }
        else if (strcmp(argv[i], "-lf") == 0||strcmp(argv[i], "--log-file") == 0)
        {
            if (i + 1 >= argc)
            {
                printf("No log file specified with %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            char log_file[PATH_MAX];
            int response = __get_path(argv[++i], log_file);
            
            if (response < 0){
                exit(response);
            }
            args->monitor_log_file_path = log_file; 

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

static int __get_path(const char * input_path, char * output_path) {
    if(__is_path_empty(input_path) ) {
        fprintf(stderr, "No path has been supplied.\n");
        return D_INVALID_PATH_FORMAT;
    }

    char file_name[PATH_MAX];
    char directory[PATH_MAX];
    bool is_excl_file = __remove_last_item_path(input_path, directory, file_name);
    if (!is_excl_file) {
        // is file
        // updates output path
        fprintf(stderr,"Only provided file name\n");
        if(!__expand_path_to_absolute(input_path, output_path)) {
            return D_COULD_NOT_EXPAND;
        }
        return D_SUCCESS;
    }
    fprintf(stderr,"provided file and directory %s %s\n", file_name, directory);
    char expanded_directory[PATH_MAX];
    // updates output path
    if(!__expand_path_to_absolute(directory, expanded_directory)) {
        return D_COULD_NOT_EXPAND;
    }

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(output_path, PATH_MAX-1, "%s/%s", directory, file_name);
    #pragma GCC diagnostic pop

    if(!__has_path_parent(output_path)) {
        fprintf(stderr, "The file specified does not have a parent path.\n");
        return D_PARENT_PATH_NO_EXIST;
    }
    if(__has_write_permission_to_parent(output_path)) {
        fprintf(stderr, "This daemon does not have write permissions to the parent log directory.\n");
        return D_NO_WRITE_TO_LOG_PARENT_DIRECTORY;
    }
    return D_SUCCESS;
}
static bool __has_path_parent (const char * path) {
    char path_copy[PATH_MAX];
    strncpy(path_copy, path, PATH_MAX);
    char *parent_dir = dirname(path_copy);
    return access(parent_dir, F_OK) == 0;
}
static bool __has_write_permission_to_parent (const char path[PATH_MAX]) {
    char path_copy[PATH_MAX];
    strncpy(path_copy, path, PATH_MAX);

    char *parent_dir = dirname(path_copy);

    return access(parent_dir, W_OK) == W_OK;
}
/// @brief Returns if path is empty
/// @param path 
/// @return True -> path is empty, False -> Path is not empty
static bool __is_path_empty(const char path[PATH_MAX]) { 
    return path == NULL || path[0] == '\0';
}
/// @brief Attemps to expand the input path to an absolute value.
/// @param path 
/// @param output_path 
/// @return True -> successful, False -> unsuccessful
static bool __expand_path_to_absolute(const char path[PATH_MAX], char output_path[PATH_MAX]) {
    if (realpath(path, output_path) == NULL){
        fprintf(stderr, "%s could not resolve to absolute directory: %s\n", path, strerror(errno));
        return false;
    }
    return true;
}

/// @brief Returns if path has directory or not.
/// @param path 
/// @return  
static bool __remove_last_item_path(const char path[PATH_MAX], char directory[PATH_MAX], char item[PATH_MAX]) {
    char new_path[PATH_MAX];
    strcpy(new_path, path);
    char *last_slash = strrchr((const char *)new_path, '/');

    if(last_slash == NULL) {
        return false;
    } 
    *last_slash = '\0';
    strcpy(directory, new_path);
    strcpy(item, (new_path + strlen(new_path) + 1));
    fprintf(stderr, "%s", item);
    return true;

}