#include "../libs/include/reportman.h"

int listen_to_clients(void);
int handle_command(char *command, unsigned long long int client_id);
int d_acquire_singleton(int *sockfd, short unsigned singleton_port);
int acknowledge_client(ipc_pipes_t *pipes);

enum
{
    RMD_FD_POLL_SIGNAL = 0,
    RMD_FD_POLL_CLIENT = 1,
    RMD_FD_POLL_FM = 2,
    RMD_FD_POLL_MON = 3,
    RMD_FD_HEALTH_PROBE = 4,
    RMD_FD_POLL_MAX = 5
};
