#include "reportman.h"

int listen_to_clients(void);
int handle_command(char *command, unsigned long long int client_id);
int d_acquire_singleton(int *sockfd, short unsigned singleton_port);
