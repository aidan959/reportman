#include <stdbool.h>
typedef struct 
{
    bool from_daemon;
    int daemon_to_fm_read_id;
    int fm_to_daemon_write_id;
    time_t backup_time;
    time_t transfer_time;
} reportman_fm_t;

