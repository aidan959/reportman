#define TRANSACT_SUCCESS 0
#define TRANSACT_ERR_OPEN -1
#define TRANSACT_HASH_FAILED -2
int verify_files(FILE *source, FILE * dest);
int transact_move_file(const char *source_path, const char *dest_path);
void hash_file(unsigned char * sha256_hash, FILE *source); 
void move_directory(const char *source, const char *destination);
timer_t backup_at_time(const char * source_directory,const char * target_directory, time_t time, time_t repeat_interval);

