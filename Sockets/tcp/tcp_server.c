#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>

int main()
{
    char server_message[256] = "This is the server eqibfp   wg  pg\n";
    int network_socket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8888);
    server_address.sin_addr.s_addr = INADDR_ANY; // resolved to any IP address on machine

    // bind socket to specifed IP and port, using same port
    bind(network_socket, (struct sockaddr *)&server_address, sizeof(server_address));
    listen(network_socket, 5); // listening for connections from cleints

    int client_socket;
    // accepting a connection means creating a client socket
    client_socket = accept(network_socket, NULL, NULL);

    // send a message to the connection stream
    send(client_socket, server_message, sizeof(server_message), 0);

    pclose(network_socket);

    return 0;
}