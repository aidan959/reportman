#ifndef REPORTMAND_BIND_PORT
#define REPORTMAND_BIND_PORT 7770
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
#define D_MAX_INTERPROCESS_ARGUMENTS 25

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

#define D_HEALTH_PROBE_INTERVAL_SEC 5

#define D_INTERVAL (24 * 60 * 60)

#define R_PARSE_SUCCESS 1
#define R_PARSE_NO_MATCH 0
#define R_PARSE_NO_VALUE -1

#define M_LOG_PATH "/var/log/reportman/monitor.log"

#define R_REPORTS_DIRECTORY "/srv/reportman/reports"
#define R_BACKUP_DIRECTORY "/srv/reportman/backup"
#define R_DASHBOARD_DIRECTORY "/srv/reportman/dashboard"
#include <sys/types.h>

#include <stdbool.h>
#define false 0
#define true 1

#include "reportman_types.h"
int start_child_process(child_process_t *child_process, const char *extra_args[], int d_socket);

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

bool ipc_is_command(unsigned long msg);
bool ipc_is_ack(unsigned long msg);
bool ipc_is_health_probe(unsigned long msg);

bool ipc_is_yes(unsigned long msg);
bool ipc_is_no(unsigned long msg);
bool ipc_is_ulong(unsigned long msg);

bool ipc_send_command(ipc_pipes_t *pipes, IPC_COMMANDS command);
bool ipc_send_panic(ipc_pipes_t *pipes, const char* log, bool * panic);

int ipc_get_command(ipc_pipes_t *pipes, IPC_COMMANDS * command, time_t timeout_secs);

bool ipc_send_ulong(ipc_pipes_t *pipes, unsigned long value);

bool ipc_send_ack(ipc_pipes_t *pipes);
bool ipc_get_ack(ipc_pipes_t *pipes);


bool ipc_send_health_probe(ipc_pipes_t *pipes);
bool ipc_send_health_probe(ipc_pipes_t *pipes);

bool ipc_send_test_data(ipc_pipes_t *pipes, const test_data_t *data);
int ipc_get_test_data(ipc_pipes_t *pipes, test_data_t *data);

unsigned long ipc_get_ulong(ipc_pipes_t *pipes);
int acknowledge_daemon(ipc_pipes_t *pipes);

int get_hh_mm_str(const char *input_HH_MM, time_t *next_time);

int r_initialize_signals(void);


#endif
