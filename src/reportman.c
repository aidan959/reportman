// Shared daemon and client functionality -> using this currently for header values and this needs to exist to link the h file
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "reportman.h"

unsigned short __parse_short_arg(char * input);
static int __starts_with(const char *str, const char *prefix);
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
                printf("No port specified after %s\n", argv[i]);
                exit(1);
            }
            args->daemon_port =__parse_short_arg(argv[++i]); 
            continue;
        }
         else if (strcmp(argv[i], "-f") == 0||strcmp(argv[i], "--force") == 0)
        {
            args->force = true; 
        }
        else
        {
            
        }
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
/// @brief handles and executes a command
/// @param command 
/// @param client_id 
void handle_command(char *command, unsigned long long int client_idk, command_t * response) {
    // TODO use hash table with function pointers instead
    parse_command(command, response);

}
/// @brief parses command to its representative enum
/// @param command 
/// @return 
int parse_command(char *command, command_t * response) {
    tolower(command);
    if (strcmp(command, "backup") == 0) {
        response->command = BACKUP;
        return COMMAND_SUCCESSFUL;
    } else if (strcmp(command, "transfer") == 0) {
        response->command = TRANSFER;
        return COMMAND_SUCCESSFUL;
    } else {
        response->command = UNKNOWN; 
        return COMMAND_NOT_FOUND;
    }
}

void to_lower_case(char *str) {
    if (str == NULL) return;

    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char) str[i]);
    }
}