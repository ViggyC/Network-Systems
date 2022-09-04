#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define BUFSIZE 1024

int main(int argc, char **argv)
{
    // char *hostname;
    // struct hostent *server;
    // char buf[BUFSIZE];

    // hostname = argv[1];

    // bzero(buf, BUFSIZE);

    // server = gethostbyname(hostname);
    // puts(server->h_name);

    // bcopy((char *)server->h_addr_list[0], buf, server->h_length);
    // puts(server->h_name);

    char *hostname;

    hostname = argv[1];
    puts(hostname);
    if (gethostname(hostname, BUFSIZE) == 0)
        puts(hostname);
    else
        perror("gethostname");

    return 0;

    return 0;
}