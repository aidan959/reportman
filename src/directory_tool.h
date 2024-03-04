#define TRANSACT_SUCCESS 0
#define TRANSACT_ERR_OPEN -1
#define TRANSACT_HASH_FAILED -2
#define BACKUP_MODE (unsigned)0
#define TRANSFER_MODE (unsigned)1
#define UNIMPLEMENTED_TRANSFER_METHOD (unsigned)0x11111100
typedef enum  {
    BACKUP=BACKUP_MODE,
    TRANSFER=TRANSFER_MODE,
} transfer_method;
const char * str_transfer_name(transfer_method method);


int verify_files(FILE *source, FILE * dest);
int transact_transfer_file(const char *source_path, const char *dest_path, transfer_method method);
void hash_file(unsigned char * sha256_hash, FILE *source); 
int transfer_directory(const char *source, const char *destination, transfer_method method);
timer_t transfer_at_time(const char * source_directory,const char * target_directory, time_t time, time_t repeat_interval, transfer_method method);
int init_directories(int num_dirs, const char **dirs);
