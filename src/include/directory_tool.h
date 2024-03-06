#define TRANSACT_SUCCESS 0
#define TRANSACT_ERR_OPEN -1
#define TRANSACT_HASH_FAILED -2

#define LOCKED_DIRECTORY_MODE 0440
#define UNLOCKED_DIRECTORY_MODE 0660

#include <stdio.h>

const char * str_transfer_name(transfer_method method);

int gettime(timer_t timer_id,struct itimerspec *curr_value);
int verify_files(FILE *source, FILE * dest);
int transact_transfer_file(const char *source_path, const char *dest_path, transfer_method method);
void hash_file(unsigned char * sha256_hash, FILE *source); 
int transfer_directory(const char *source, const char *destination, transfer_method method);
timer_t transfer_at_time(const char * source_directory,const char * target_directory, time_t time, time_t repeat_interval, transfer_method method);
int init_directories(int num_dirs, const char **dirs);
