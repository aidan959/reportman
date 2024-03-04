#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <unistd.h>
#include "reportmanc.h"
void error(const char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[]) {
    int sockfd;
    ssize_t num_bytes;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[COMMUNICATION_BUFFER_SIZE];
    client_arguments_t client_args = { .daemon_port = REPORTMAND_BIND_PORT };

    configure_client_args(argc, argv, &client_args);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) 
        error("ERROR opening socket");

    server = gethostbyname("localhost");
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memmove((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr_list[0], server->h_length);
    serv_addr.sin_port = htons(client_args.daemon_port);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR connecting");

    printf("Sending command: %s\n", argv[2]);
    num_bytes = write(sockfd, argv[2], strlen(argv[2]));
    if (num_bytes < 0) 
         error("ERROR writing to socket");

    memset(buffer, 0, COMMUNICATION_BUFFER_SIZE);
    num_bytes = read(sockfd, buffer, COMMUNICATION_BUFFER_SIZE - 1);
    if (num_bytes < 0) 
         error("ERROR reading from socket");
    printf("%s\n", buffer);

    close(sockfd);
    return 0;
}
