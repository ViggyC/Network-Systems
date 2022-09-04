#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

// accepts request

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portnumber, clientlength;
    char buffer[256];
    struct sockaddr_in server_addr, client_addr;
    int n;

    if (argc < 2)
    {
        fprintf(stderr, "No port number provided\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0); // IP address; TCP concept

    if (sockfd < 0)
    {
        error("COuldnt create socket\n");
    }

    bzero((char *)&server_addr, sizeof(server_addr));

    portnumber = atoi(argv[1]);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(portnumber);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        error("Error on binding\n");
    }

    listen(sockfd, 5);
    clientlength = sizeof(client_addr);

    // accept returns a new socket for the client
    // sockfd is used for listening to incoming connections
    newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, &clientlength);

    if (newsockfd < 0)
    {
        error("Could not accept\n");
    }

    bzero(buffer, 256);

    // server reads buffer message from client, client uses newsocketfd to stream
    n = read(newsockfd, buffer, 255);

    if (n < 0)
    {
        error("Error reading from socket\n");
    }

    printf("Here is the message: %s\n", buffer);

    // newsocketfd is used for communication!!
    n = write(newsockfd, "I got your message", 18);

    if (n < 0)
    {
        error("Could not write to client");
    }

    return 0;
}