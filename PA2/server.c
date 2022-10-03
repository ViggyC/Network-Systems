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
#define TEMP_SIZE 1024

/* Source: https://jameshfisher.com/2016/12/20/http-hello-world/ */

typedef struct
{
    char version[TEMP_SIZE];
    char status[TEMP_SIZE];
    char contentType[TEMP_SIZE];
    int length;
} HTTPResponseHeader;

/*
Request Method: GET

Request URI: /Protocols/rfc1945/rfc1945

Request Version: HTTP/1.1
*/
typedef struct
{
    char *method;
    char *URI;
    char *version;
} HTTP_REQUEST;

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

int getContentType(char *contentType, char *fileExtension)
{

    if (strcmp(fileExtension, ".html") == 0)
    {
        strcpy(contentType, "text/html");
    }
    if (strcmp(fileExtension, ".txt") == 0)
    {
        strcpy(contentType, "text/plain");
    }
    if (strcmp(fileExtension, ".png") == 0)
    {
        strcpy(contentType, "img/png");
    }
    if (strcmp(fileExtension, ".gif") == 0)
    {
        strcpy(contentType, "image/gif");
    }
    if (strcmp(fileExtension, ".jpg") == 0)
    {
        strcpy(contentType, "image/jpg");
    }

    if (strcmp(fileExtension, ".css") == 0)
    {
        strcpy(contentType, "text/css");
    }

    if (strcmp(fileExtension, ".js") == 0)
    {
        strcpy(contentType, "application/javascript");
    }

    return 0;
}

/********************************** Invalid Requests****************************************/
int NotFound(int client)
{

    char response[] = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    // snprintf(response);
    /* review this method of sending to the client*/
    for (int sent = 0; sent < sizeof(response); sent += send(client, response + sent, sizeof(response) - sent, 0))
        ;
    close(client);

    return 0;
}

int BadRequest(int client)
{

    char response[] = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\nConnection: close\r\n\r\nHello World!";
    // snprintf(response);
    /* review this method of sending to the client*/
    for (int sent = 0; sent < sizeof(response); sent += send(client, response + sent, sizeof(response) - sent, 0))
        ;
    close(client);

    return 0;
}

int MethodNotAllowed(int client)
{
    char response[] = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    // snprintf(response);
    /* review this method of sending to the client*/
    for (int sent = 0; sent < sizeof(response); sent += send(client, response + sent, sizeof(response) - sent, 0))
        ;
    close(client);

    return 0;
}

int Forbidden(int client)
{
    char response[] = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\nConnection: close\r\n\r\nHello World!";
    // snprintf(response);
    /* review this method of sending to the client*/
    for (int sent = 0; sent < sizeof(response); sent += send(client, response + sent, sizeof(response) - sent, 0))
        ;
    close(client);

    return 0;
}

int HTTPVersionNotSupported(int client)
{
    char response[] = "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 0\r\nContent-Type: <>\r\nConnection: close\r\n\r\n";
    // snprintf(response);
    /* review this method of sending to the client*/
    for (int sent = 0; sent < sizeof(response); sent += send(client, response + sent, sizeof(response) - sent, 0))
        ;
    close(client);

    return 0;
}

/**********************************End of Invalid Requests****************************************/

int service_request(int client, void *client_args)
{
    HTTP_REQUEST *client_request = (HTTP_REQUEST *)client_args;
    HTTPResponseHeader http_response; /* to send back to client*/
    FILE *fp;                         // file descriptor for page to send to client
    ssize_t fsize;

    /* Just get version right away, part of server response*/
    strcpy(http_response.version, client_request->version);
    // printf("HTTP version response: %s\n", http_response.version);

    /*open the URI*/
    char relative_path[TEMP_SIZE];
    bzero(relative_path, sizeof(relative_path)); // clear it!!!!!!!!!!
    strcat(relative_path, "www");
    strcat(relative_path, client_request->URI);
    printf("Relative path: %s\n", relative_path);

    fp = fopen(relative_path, "rb");
    if (fp == NULL)
    {
        printf("File does not exist: send back 404\n");
        NotFound(client);
    }
    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    printf("Content Length: %d\n", (int)fsize);

    /* GET CONTENT TYPE!!!!!!*/
    char *file_extention = strchr(relative_path, '.');
    printf("%s\n", file_extention);
    getContentType(http_response.contentType, file_extention);
    printf("Reponse Content Type: %s\n", http_response.contentType);

    /* Generate actual payload to send with header status*/
    char payload[fsize];
    fread(payload, 1, fsize, fp);

    /*Generate response*/
    /* send HTTP response back to client: webpage */
    /* note the header must be very secific */
    char response_header[BUFSIZE];
    sprintf(response_header, "%s 200 OK\r\nContent-Type:%s\r\nContent-Length:%ld\r\n\r\n", http_response.version, http_response.contentType, fsize);
    // printf("%s\n", response_header);
    /* Now attach the payload*/
    char full_response[fsize + strlen(response_header)];
    strcpy(full_response, response_header);
    // use memcpy() to attach payload to header
    memcpy(full_response + strlen(full_response), payload, fsize);

    /* AND we got it! */
    send(client, full_response, sizeof(full_response), 0);
    close(client);

    return 0;
}

/* different processes for different requests will be handling this routine */
/* Process will stay in this routine if 200 OK */
int parse_request(int client, char *buf)
{
    printf("Parsing the HTTP request in this routine \n");
    buf[strlen(buf) - 1] = '\0';
    printf("Client sent: %s\n", buf);

    /*Parse client request*/
    HTTP_REQUEST client_request;
    char temp_buf[BUFSIZE];
    strcpy(temp_buf, buf);
    client_request.method = strtok(temp_buf, " "); // GET
    client_request.URI = strtok(NULL, " ");        // route/URI - relative path
    client_request.version = strtok(NULL, "\r");   // version, end in \r
    printf("HTTP method: %s\n", client_request.method);
    printf("HTTP page: %s\n", client_request.URI);
    printf("HTTP version: %s\n", client_request.version);

    client_request.method[strlen(client_request.method)] = '\0';
    client_request.version[strlen(client_request.version)] = '\0';

    if (strcmp(client_request.method, "GET") != 0)
    {
        printf("Method not allowed\n");
        MethodNotAllowed(client);
        return 0;
    }
    // printf("HTTP version: %s\n", client_request.version);
    // printf("%d\n", strcmp(client_request.version, "HTTP/1.1"));
    // printf("Browser version: %d\n", strcmp(client_request.version, "HTTP/1.1"));

    if (strcmp(client_request.version, "HTTP/1.1") != 0)
    {
        printf("Invalid HTTP version\n");
        HTTPVersionNotSupported(client);
        return 0;
    }

    /* After parsing and hanlding bad requests, pass routine to service the actual file*/
    int handle = service_request(client, &client_request);

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
        /* Write logic for multiple connections - fork() or threads */

        // dummy test response(use with NetCat)
        // send(client_socket, server_message, sizeof(server_message), 0);

        memset(buf, 0, BUFSIZE);

        // printf("Marker 1\n");
        /* read from the socket */
        n = recv(client_socket, buf, BUFSIZE, 0);
        if (n < 0)
        {
            printf("Bad request\n");
        }

        /* Parse request: GET /Protocols/rfc1945/rfc1945 HTTP/1.1 */
        int handle_result = parse_request(client_socket, buf);

        /* After parsing need to send response */
    }

    close(sockfd);

    return 0;
}
