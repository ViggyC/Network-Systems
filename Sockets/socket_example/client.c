#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>

// does the requesting

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    int socketfd, portnumber, n;
    struct sockaddr_in server_addr;
    struct hostent *server;

    char buffer[256];

    if (argc < 3)
    {
        // localhost
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        exit(0);
    }

    portnumber = atoi(argv[2]);

    socketfd = socket(AF_INET, SOCK_STREAM, 0);

    if (socketfd < 0)
    {
        error("COuld not create socket");
    }

    server = gethostbyname(argv[1]);

    fprintf(stderr, server->h_addr_list);

    if (server == NULL)
    {
        fprintf(stderr, "NO host\n");
        exit(0);
    }

    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length);

    server_addr.sin_port = htons(portnumber);
    if (connect(socketfd, &server_addr, sizeof(server_addr)) < 0)
    {
        error("Error connecting\n");
    }

    printf("Enter message to send to server: ");
    bzero(buffer, 256);
    fgets(buffer, 255, stdin); // get message from cmd and store in buffer

    n = write(socketfd, buffer, strlen(buffer));
    if (n < 0)
    {
        error("Could not write to server\n");
    }
    bzero(buffer, 256);
    n = read(socketfd, buffer, 256);
    if (n < 0)
    {
        error("Could not read from socket\n");
    }
    printf("%s\n", buffer);

    return 0;
}
