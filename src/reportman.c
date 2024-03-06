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
#include "include/reportman.h"


int __parse_signed_int_arg(char * input);
bool __parse_bool_arg(char * input);
void to_lower_case(char *str);


unsigned short __parse_short_arg(char * input) {
    char *endptr;
    long value = strtol(input, &endptr, 10);
    if (endptr == input) {
        printf("Argument could not be converted to a short : %s\n", input);
        exit(EXIT_FAILURE);
    }
    if (value < 0 || value > 0b1111111111111111) {
        printf("Argument out of range: %s\n", input);
        exit(EXIT_FAILURE);
    }
    return (unsigned short)value;
}
unsigned int __parse_unsigned_int_arg(char * input){
    char *endptr;
    long value = strtol(input, &endptr, 10);
    if (endptr == input) {
        printf("Argument could not be converted to an unsigned int : %s\n", input);
        exit(EXIT_FAILURE);
    }
    if (value < 0 || value > 0b11111111111111111111111111111111) {
        printf("Unsigned int argument out of range: %s\n", input);
        exit(EXIT_FAILURE);
    }
    return (unsigned int)value;
}
int __parse_signed_int_arg(char * input){
    char *endptr;
    long value = strtol(input, &endptr, 10);
    if (endptr == input) {
        printf("Argument could not be converted to an unsigned int : %s\n", input);
        exit(EXIT_FAILURE);
    }
    if (value < -2147483648 || value > 2147483647) {
        printf("Signed int argument out of range: %s\n", input);
        exit(EXIT_FAILURE);
    }
    return (int)value;
}
bool __parse_bool_arg(char * input){
    char *endptr;
    to_lower_case(input);
    if(strcmp(input, "true"))
        return true;
    if(strcmp(input, "false"))
        return false;
    long value = strtol(input, &endptr, 10);
    if (endptr == input) {
        printf("Argument could not be converted to an unsigned int : %s\n", input);
        exit(EXIT_FAILURE);
    }
    if (value < 0 || value > 1) {
        printf("Bool argument out of range: %s\n", input);
        exit(EXIT_FAILURE);
    }
    return (bool)value;
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



int arg_parse_string(int argc, char *argv[], int *argi, const char * short_name, const char * long_name,const char * value_name, char *output){
    if(!(strcmp(argv[*argi], short_name) == 0 || strcmp(argv[*argi], long_name) == 0))
        return R_PARSE_NO_MATCH;
    
    if (*argi + 1 >= argc)
    {
        fprintf(stderr, "No %s specified with %s\n", value_name, argv[*argi]);
        exit(EXIT_FAILURE);
    }

    strcpy(output, argv[++(*argi)]);
    return R_PARSE_SUCCESS;
    
}

int arg_parse_ushort(int argc, char *argv[], int *argi, const char * short_name, const char * long_name,const char * value_name, unsigned short *output){
    if(!(strcmp(argv[*argi], short_name) == 0 || strcmp(argv[*argi], long_name) == 0))
        return R_PARSE_NO_MATCH;
    

    if (*argi + 1 >= argc)
    {
        fprintf(stderr, "No %s specified with %s\n", value_name, argv[*argi]);
        exit(EXIT_FAILURE);
    }

    *output = __parse_short_arg(argv[++(*argi)]); 

    return R_PARSE_SUCCESS;
    
}
int arg_parse_flag(char *arg, const char * short_name, const char * long_name){
    if((strcmp(arg, short_name) == 0 || strcmp(arg, long_name) == 0)){
        return R_PARSE_SUCCESS;
    }

    return R_PARSE_NO_MATCH;
}
int arg_parse_uint(int argc, char *argv[], int *argi, const char * short_name, const char * long_name,const char * value_name, unsigned int *output) {
    if(!(strcmp(argv[*argi], short_name) == 0 || strcmp(argv[*argi], long_name) == 0))
        return R_PARSE_NO_MATCH;
    if (*argi + 1 >= argc)
    {
        fprintf(stderr, "No %s specified with %s\n", value_name, argv[*argi]);
        exit(EXIT_FAILURE);
    }

    *output = __parse_unsigned_int_arg(argv[++(*argi)]); 

    return R_PARSE_SUCCESS;
}

int arg_parse_int(int argc, char *argv[], int *argi, const char * short_name, const char * long_name,const char * value_name, int *output) {
    if(!(strcmp(argv[*argi], short_name) == 0 || strcmp(argv[*argi], long_name) == 0))
        return R_PARSE_NO_MATCH;
    if (*argi + 1 >= argc)
    {
        fprintf(stderr, "No %s specified with %s\n", value_name, argv[*argi]);
        exit(EXIT_FAILURE);
    }

    *output = __parse_signed_int_arg(argv[++(*argi)]); 

    return R_PARSE_SUCCESS;
}

int arg_parse_bool(int argc, char *argv[], int *argi, const char * short_name, const char * long_name,const char * value_name, bool *output) {
    if(!(strcmp(argv[*argi], short_name) == 0 || strcmp(argv[*argi], long_name) == 0))
        return R_PARSE_NO_MATCH;
    if (*argi + 1 >= argc)
    {
        fprintf(stderr, "No %s specified with %s\n", value_name, argv[*argi]);
        exit(EXIT_FAILURE);
    }

    *output = __parse_bool_arg(argv[++(*argi)]); 
    return R_PARSE_SUCCESS;
}

int arg_unrecognised(char * arg) {
    return strncmp(arg, "-", 1) == 0;
}


int get_hh_mm_str(const char * input_HH_MM,time_t * next_time) {
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