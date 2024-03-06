#ifndef REPORTMAND_BIND_PORT
#define REPORTMAND_BIND_PORT 7770
#define COMMUNICATION_BUFFER_SIZE 1024
#define IS_SINGLETON 0
#define BIND_FAILED -1
#define COMMAND_NOT_FOUND 127
#define COMMAND_SUCCESSFUL 0
#define COMMAND_ERROR 1
#define LSOF_FD_NOT_FOUND 2
#define MAX_CLIENT_QUEUE 5
#define D_SUCCESS 0
#define D_FAILURE -1
#define D_FAILURE_TIMEOUT -2


#define UNIMPLEMENTED_TRANSFER_METHOD (unsigned)0x11111100

#define D_INVALID_PATH_FORMAT -1
#define D_NO_WRITE_TO_LOG_PARENT_DIRECTORY -2
#define D_PARENT_PATH_NO_EXIST -3
#define D_COULD_NOT_EXPAND -4



#define D_COULD_NOT_CREATE_LOG_FILE -2

#define D_INVALID_HH_MM_FORMAT -1
#define D_INVALID_HH_MM_RANGE -2
#define D_UNIMPLEMENT -2

#define D_DEFAULT_BACKUP_TIME "00:30"
#define D_DEFAULT_TRANSFER_TIME "23:30"


#define D_INTERVAL (24 * 60 * 60)


#define R_PARSE_SUCCESS 1
#define R_PARSE_NO_MATCH 0
#define R_PARSE_NO_VALUE -1

#define M_LOG_PATH "/var/log/reportmand/monitor.log"

#define R_REPORTS_DIRECTORY "/srv/reportman/reports"
#define R_BACKUP_DIRECTORY "/srv/reportman/backup"
#define R_DASHBOARD_DIRECTORY "/srv/reportman/dashboard"

#include <stdbool.h>
#define false 0
#define true 1

#include "reportman_types.h"

void configure_daemon_args(int argc, char *argv[], daemon_arguments_t *args);
void configure_client_args(int argc, char *argv[], client_arguments_t *args);
int arg_parse_string(int argc, char *argv[], int *argi, const char * short_name, const char * long_name,const char * value_name, char *output);
int arg_parse_flag(char *arg, const char * short_name, const char * long_name);
int arg_parse_ushort(int argc, char *argv[], int *argi, const char * short_name, const char * long_name,const char * value_name, unsigned short *output);
int arg_parse_long(int argc, char *argv[], int *argi, const char * short_name, const char * long_name,const char * value_name, long *output);
int arg_parse_uint(int argc, char *argv[], int *argi, const char * short_name, const char * long_name,const char * value_name, unsigned int *output);
int arg_parse_int(int argc, char *argv[], int *argi, const char * short_name, const char * long_name,const char * value_name, int *output);
int arg_parse_bool(int argc, char *argv[], int *argi, const char * short_name, const char * long_name,const char * value_name, bool *output);
int arg_unrecognised(char * arg);
unsigned short __parse_short_arg(char * input);
unsigned int __parse_unsigned_int_arg(char * input);

int parse_command(char *command, command_response_t * response) ;
int main(int argc, char *argv[]);
int get_hh_mm_str(const char * input_HH_MM,time_t * next_time);
/// @brief Converts HH:MM string to time_t
/// @param input_HH_MM 
/// @param next_time 
/// @return Success of function

#endif
