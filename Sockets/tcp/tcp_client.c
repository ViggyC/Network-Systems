#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>

int main()
{
    int network_socket;

    // connect to internet tcp
    network_socket = socket(AF_INET, SOCK_STREAM, 0);

    // connect to remote server (socket address)
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8888);
    server_address.sin_addr.s_addr = INADDR_ANY;

    // cast server address
    int connection_status = connect(network_socket, (struct sockaddr *)&server_address, sizeof(server_address));
    if (connection_status == -1)
    {
        printf("Connection failed\n");
    }

    char server_response[256];
    // receive data from server
    recv(network_socket, &server_response, sizeof(server_response), 0);
    printf("Data from server: %s\n", server_response);

    // close socket
    pclose(network_socket);
    return 0;
}