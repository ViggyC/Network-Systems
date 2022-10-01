#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>
#define BUFSIZE 8192

struct HTTPHeader
{
};

int main(int argc, char **argv)
{
    int sockfd;                    /* server socket file descriptor*/
    int portno;                    /* port to listen on */
    int clientlen;                 /* byte size of client's address -- need to look into this, seems this is filled in by recvfrom */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    struct hostent *hostp;         /* client host info */
    char buf[BUFSIZE];             /* message buf  from client*/
    char temp_buf[BUFSIZE];        // for parsing user input
    char *hostaddrp;               /* dotted decimal host addr string */
    int optval;                    /* flag value for setsockopt */
    int n;                         /* message byte size */

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    portno = atoi(argv[1]);
    char server_message[256] = "This is the server\n";
    int network_socket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(5000);
    server_address.sin_addr.s_addr = INADDR_ANY; // resolved to any IP address on machine

    // bind socket to specifed IP and port, using same port
    bind(network_socket, (struct sockaddr *)&server_address, sizeof(server_address));
    listen(network_socket, 5); // listening for connections from cleints

    int client_socket;
    // accepting a connection means creating a client socket
    client_socket = accept(network_socket, NULL, NULL);
    /* Write logic for multiple connection */

    // send a message to the connection stream
    send(client_socket, server_message, sizeof(server_message), 0);

    pclose(network_socket);

    return 0;
}