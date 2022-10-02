#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#define BUFSIZE 8192

struct HTTPResponseHeader
{
    int version;
    int status;
    char *contentType;
    int length;
};

/*
Request Method: GET

Request URI: /Protocols/rfc1945/rfc1945

Request Version: HTTP/1.1
*/
struct HTTP_REQUEST
{
    char *method;
    char *URI;
    char *version;
};

enum CONTENT_TYPE
{
    text_html,
    text_plain,
    image_png,
    image_gif,
    image_jpg,
    text_css,
    application_javascript,
};

/* different processes for different requests will be handling this routine */
int handle_request(int client, char *buf)
{
    printf("Parsing the HTTP request in this routine \n");

    FILE *fp; // file descriptor for page to send to client
    ssize_t fsize;

    return 0;
}

int main(int argc, char **argv)
{
    int sockfd;                    /* server socket file descriptor*/
    int portno;                    /* port to listen on */
    int clientlen;                 /* byte size of client's address -- need to look into this, seems this is filled in by recvfrom */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    struct hostent *hostp;         /* client host info */
    char buf[BUFSIZE];             /* message buf from client. This is the http request I think......*/
    char temp_buf[BUFSIZE];        // for parsing user input
    char *hostaddrp;               /* dotted decimal host addr string */
    int optval;                    /* flag value for setsockopt */
    int n;                         /* message byte size */
    int client_socket;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    portno = atoi(argv[1]);
    char server_message[256] = "This is the webserver\n";
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons((unsigned short)portno);
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); // resolved to any IP address on machine

    // bind socket to specifed IP and port, using same port
    if (bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
    {
        printf("Error binding\n");
        exit(1);
    }

    listen(sockfd, 100); // listening for connections from cleints
    printf("Listening on port %d\n", portno);

    /* continously handle client requests */
    while (1)
    {
        // accepting a connection means creating a client socket
        /* this client_socket is how we send data back to the connected client */
        client_socket = accept(sockfd, NULL, NULL); // the client will connect() to the socket that the server is listening on
        /* Write logic for multiple connection */
        // FORK

        // dummy response
        send(client_socket, server_message, sizeof(server_message), 0);

        memset(buf, 0, BUFSIZE);

        // printf("Marker 1\n");
        /* read from the socket */
        n = recv(client_socket, buf, BUFSIZE, 0);
        if (n < 0)
        {
            printf("Bad request\n");
        }
        printf("\nClient said:\n%s", buf);

        /* Parse request: GET /Protocols/rfc1945/rfc1945 HTTP/1.1 */
        // int handle_result = handle_request(client_socket, buf);

        /* send HTTP response back to client: webpage */
        char *http_response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 13\r\n\r\nHello World!";
        printf("%s\n", http_response);
        bzero(buf, sizeof(buf));
        strcpy(buf, http_response);
        send(client_socket, buf, sizeof(buf), 0);
        close(client_socket);
    }

    close(sockfd);

    return 0;
}
