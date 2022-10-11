/*Author: Vignesh Chandrasekhar */
/*Collaborators: Freddy Perez and James V*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#define BUFSIZE 8192
#define TEMP_SIZE 1024

/*https://www.stackpath.com/edge-academy/what-is-keep-alive/#:~:text=Overview,connection%20header%20can%20be%20used.*/

/* Major note: all http response headers end in: \r\n\r\n */
/* Source: https://jameshfisher.com/2016/12/20/http-hello-world/ */
/*https://developer.mozilla.org/en-US/docs/Glossary/Response_header */

/* Need to figure out how accept() is handling different clients - it is blocking!!!!*/

/* The accept() call is used by a server to accept a
connection request from a client. When a connection is available,
the socket created is ready for use to read data from the process that requested the connection.
The call accepts the first connection on its queue of pending connections for the given socket socket.*/

typedef struct
{
    char version[TEMP_SIZE];
    char status[TEMP_SIZE];
    char contentType[TEMP_SIZE];
    char connection[TEMP_SIZE];
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
    char *connection;
    char *host;
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

void error(char *msg)
{
    perror(msg);
    exit(0);
}

/* Graceful Exit*/
void sigint_handler(int sig)
{

    printf("child pid:%d\n", getpid());
    printf("parent pid:%d\n", getppid());
    //-1 means wait for anychild process
    waitpid(-1, NULL, WNOHANG);
    printf("Exiting...\n");
    exit(0);
}

int getContentType(char *contentType, char *fileExtension)
{

    if (strcmp(fileExtension, ".html") == 0 || strcmp(fileExtension, ".htm") == 0)
    {
        strcpy(contentType, "text/html");
    }
    else if (strcmp(fileExtension, ".txt") == 0)
    {
        strcpy(contentType, "text/plain");
    }
    else if (strcmp(fileExtension, ".png") == 0)
    {
        strcpy(contentType, "img/png");
    }
    else if (strcmp(fileExtension, ".gif") == 0)
    {
        strcpy(contentType, "image/gif");
    }
    else if (strcmp(fileExtension, ".jpg") == 0)
    {
        strcpy(contentType, "image/jpg");
    }

    else if (strcmp(fileExtension, ".css") == 0)
    {
        strcpy(contentType, "text/css");
    }

    else if (strcmp(fileExtension, ".js") == 0)
    {
        strcpy(contentType, "application/javascript");
    }

    else if (strcmp(fileExtension, ".ico") == 0)
    {
        strcpy(contentType, "image/x-icon");
    }

    return 0;
}

int isDirectory(const char *path)
{
    struct stat statbuf;
    if (stat(path, &statbuf) != 0)
        return 0;
    return S_ISDIR(statbuf.st_mode);
}

/********************************** Invalid Requests****************************************/
int NotFound(int client)
{
    /* what should this response actually look like*/
    char response[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 18\r\n\r\n404 Page Not Found";
    // snprintf(response);
    /* review this method of sending to the client*/
    for (int sent = 0; sent < sizeof(response); sent += send(client, response + sent, sizeof(response) - sent, 0))
        ;
    printf("HTTP RESPONSE: \n");
    printf("%s\n", response);
    close(client);
    exit(1);
    return 0;
}

int BadRequest(int client)
{

    char response[] = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n";
    // snprintf(response);
    /* review this method of sending to the client*/
    for (int sent = 0; sent < sizeof(response); sent += send(client, response + sent, sizeof(response) - sent, 0))
        ;
    printf("HTTP RESPONSE: \n");
    printf("%s\n", response);
    close(client);
    exit(1);
    return 0;
}

int MethodNotAllowed(int client)
{
    /* what headers should I include*/
    char response[] = "HTTP/1.1 405 Method Not Allowed\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n";
    // snprintf(response);
    /* review this method of sending to the client*/
    for (int sent = 0; sent < sizeof(response); sent += send(client, response + sent, sizeof(response) - sent, 0))
        ;
    printf("HTTP RESPONSE: \n");
    printf("%s\n", response);
    close(client);
    exit(0);

    return 0;
}

int Forbidden(int client)
{
    char response[] = "HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n";
    // snprintf(response);
    /* review this method of sending to the client*/
    for (int sent = 0; sent < sizeof(response); sent += send(client, response + sent, sizeof(response) - sent, 0))
        ;

    printf("HTTP RESPONSE: \n");
    printf("%s\n", response);
    close(client);
    exit(0);

    return 0;
}

int HTTPVersionNotSupported(int client)
{
    char response[] = "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n";
    // snprintf(response);
    /* review this method of sending to the client*/
    for (int sent = 0; sent < sizeof(response); sent += send(client, response + sent, sizeof(response) - sent, 0))
        ;

    printf("HTTP RESPONSE: \n");
    printf("%s\n", response);
    close(client);
    exit(1);

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

    /* Some client may not send a connection type - dumb*/
    if (client_request->connection != NULL)
    {
        strcpy(http_response.connection, client_request->connection);
    }

    /*open the URI*/
    char relative_path[TEMP_SIZE];
    bzero(relative_path, sizeof(relative_path)); // clear it!!!!!!!!!!

    /* hard code root directory www*/
    strcat(relative_path, "www");
    strcat(relative_path, client_request->URI);

    if (strcmp(client_request->URI, "/") == 0)
    {
        strcat(relative_path, "index.html");
        fp = fopen(relative_path, "rb");
        if (fp == NULL)
        {

            bzero(relative_path, sizeof(relative_path)); // clear it!!!!!!!!!!
            strcat(relative_path, "www");
            strcat(relative_path, client_request->URI);
            strcat(relative_path, "index.htm");
            printf("index.html does not exist\n");
            printf("Relative path: %s\n", relative_path);
        }
    }
    else if (isDirectory(relative_path) != 0)
    {
        printf("This is a directory\n");
        strcat(relative_path, "/index.html");
        fp = fopen(relative_path, "rb");
        if (fp == NULL)
        {

            bzero(relative_path, sizeof(relative_path)); // clear it!!!!!!!!!!
            strcat(relative_path, "www");
            strcat(relative_path, client_request->URI);
            strcat(relative_path, "/index.htm");
            // printf("index.html does not exist\n");
            // printf("Relative path: %s\n", relative_path);
        }
    }
    printf("Relative path: %s\n", relative_path);
    fp = fopen(relative_path, "rb");
    if (fp == NULL)
    {
        printf("%s DOES NOT EXIST!\n", relative_path);
        NotFound(client);
    }

    /* Pulled from PA1*/
    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* GET CONTENT TYPE!!!!!!*/
    char *file_extention = strchr(relative_path, '.');
    // printf("File extension: %s\n", file_extention);
    if (file_extention == NULL)
    {
        NotFound(client);
    }
    getContentType(http_response.contentType, file_extention);
    // printf("Reponse Content Type: %s\n", http_response.contentType);

    /* Generate actual payload to send with header status*/
    char payload[fsize];
    fread(payload, 1, fsize, fp);

    /*Generate response*/
    /* send HTTP response back to client: webpage */
    /* note the header must be very secific */
    char response_header[BUFSIZE];
    // if (strcmp(client_request->connection, "keep-alive") == 0)
    // {

    //     sprintf(response_header, "%s 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: %s\r\n\r\n", http_response.version, http_response.contentType, fsize, http_response.connection);
    // }
    // else
    // {
    //     sprintf(response_header, "%s 200 OK\r\nContent-Type:%s\r\nContent-Length:%ld\r\nConnection: %s\r\n\r\n", http_response.version, http_response.contentType, fsize, http_response.connection);
    // }

    /* use for non extra credit*/
    sprintf(response_header, "%s 200 OK\r\nContent-Type:%s\r\nContent-Length:%ld\r\n\r\n", http_response.version, http_response.contentType, fsize);

    printf("RESPONSE: \n");
    printf("%s\n", response_header);
    /* Now attach the payload*/
    char full_response[fsize + strlen(response_header)];
    strcpy(full_response, response_header);
    // use memcpy() to attach payload to header
    memcpy(full_response + strlen(full_response), payload, fsize);
    /* AND we got it! */
    /* the child processes will all be sending to different {client} addresses, per parent accept() */
    send(client, full_response, sizeof(full_response), 0);
    return 0;
}

/* different processes for different requests will be handling this routine */
/* Process will stay in this routine if 200 OK */
int parse_request(int client, char *buf)
{
    // printf("Parsing the HTTP request in this routine \n");
    buf[strlen(buf) - 1] = '\0';
    // printf("Client Request:\n%s\n", buf);

    /*Parse client request*/
    HTTP_REQUEST client_request;
    char temp_buf[BUFSIZE];
    strcpy(temp_buf, buf);
    client_request.method = strtok(temp_buf, " "); // GET
    client_request.URI = strtok(NULL, " ");        // route/URI - relative path
    client_request.version = strtok(NULL, "\r");   // version, end in \r
    strtok(NULL, " ");
    client_request.host = strtok(NULL, "\r");
    strtok(NULL, " ");
    client_request.connection = strtok(NULL, "\r");

    printf("REQUEST method: %s\n", client_request.method);
    printf("REQUEST page: %s\n", client_request.URI);
    printf("REQUEST version: %s\n", client_request.version);
    printf("REQUEST host: %s\n", client_request.host);
    printf("REQUEST connection: %s\n", client_request.connection);

    /* check this logic, sometimes recv() get an empty buffer*/
    if (client_request.method == NULL || client_request.URI == NULL || client_request.version == NULL)
    {
        // bad request?
        printf("Bad request\n");
        BadRequest(client);
    }

    /* Only required for HTTP1/1.1*/
    // if( (client_request.host == NULL || client_request.connection == NULL) {

    // }

    if (strstr(client_request.URI, "..") != NULL)
    {
        // printf("Bad url: ..\n");
        Forbidden(client);
    }

    /* null terminating for safety*/
    client_request.method[strlen(client_request.method)] = '\0';
    client_request.version[strlen(client_request.version)] = '\0';

    if (strcmp(client_request.method, "GET") != 0)
    {
        printf("Method not allowed\n");
        MethodNotAllowed(client);
    }
    // printf("HTTP version: %s\n", client_request.version);
    // printf("%d\n", strcmp(client_request.version, "HTTP/1.1"));
    // printf("Browser version: %d\n", strcmp(client_request.version, "HTTP/1.1"));

    if (strcmp(client_request.version, "HTTP/1.1") == 0 || strcmp(client_request.version, "HTTP/1.0") == 0)
    {
        // do nothing
    }
    else
    {
        printf("Invalid HTTP version\n");
        HTTPVersionNotSupported(client);
    }

    /* Connection: keep-alive */
    // if (strcmp(client_request.connection, "keep-alive") == 0)
    // {
    //     int flags = 1;
    //     if (setsockopt(client, SOL_SOCKET, SO_KEEPALIVE, (void *)&flags, sizeof(flags)))
    //     {
    //         perror("ERROR: setsocketopt(), SO_KEEPALIVE");
    //         exit(0);
    //     }
    // }

    /* After parsing and hanlding bad requests, pass routine to service the actual file*/
    /* Pass in client request struct as arg*/
    int handle = service_request(client, &client_request);
    return 0;
}

int main(int argc, char **argv)
{
    int sockfd;                    /* server socket file descriptor*/
    int portno;                    /* port to listen on */
    socklen_t clientlen;           /* byte size of client's address -- need to look into this, seems this is filled in by recvfrom */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    struct hostent *hostp;         /* client host info */
    char temp_buf[BUFSIZE];        // for parsing user input
    char *hostaddrp;               /* dotted decimal host addr string */
    int optval;                    /* flag value for setsockopt */
    int n;                         /* message byte size */
    int client_socket;             /* each process will have its own*/
    int child_socket;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    portno = atoi(argv[1]);
    char server_message[256] = "This is the webserver\n";

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    /* setsockopt: Handy debugging trick that lets
     * us rerun the server immediately after we kill it;
     * otherwise we have to wait about 20 secs.
     * Eliminates "ERROR on binding: Address already in use" error.
     */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
               (const void *)&optval, sizeof(int));

    memset(&serveraddr, '\0', sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons((unsigned short)portno);
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); // resolved to any IP address on machine

    clientlen = sizeof(struct sockaddr);

    // bind socket to specifed IP and port, using same port
    if (bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
    {
        printf("Error binding\n");
        exit(1);
    }

    if (listen(sockfd, 1024) == 0)
    {
        // listening for connections from cleints, need to determine how many
        printf("Listening on port %d\n", portno);
    }
    else
    {
        // error
        exit(1);
    }

    /* continously handle client requests */
    while (1)
    {
        // accepting a connection means creating a client socket
        /* this client_socket is how we send data back to the connected client */
        // bzero(&clientaddr, sizeof(clientaddr));

        //    Man Page - accept():
        //    It extracts the first
        //    connection request on the queue of pending connections for the
        //    listening socket, sockfd, creates a new connected socket, and
        //    returns a new file descriptor referring to that socket.  The
        //    newly created socket is not in the listening state.  The original
        //    socket sockfd is unaffected by this call.

        client_socket = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen);
        /* Parent basically updates the client socket address for every iteration, with a new socket for that client*/
        // printf("Accept returned: %d\n", client_socket);

        if (client_socket < 0)
        {
            error("ERROR accepting client connection");
        }
        char buf[BUFSIZE];
        memset(buf, 0, BUFSIZE);
        /* Write logic for multiple connections - fork() or threads */

        /*TODO: graceful exit*/
        signal(SIGINT, sigint_handler);

        /* Parent spawns child processes to handle incoming requests*/
        pid_t client_connection = fork();

        /*Child Code*/
        if (client_connection == 0)
        {
            // child - closes the LISTENING socket, this sockfd doesnt matter to the child, only the parent listening for incoming requests
            close(sockfd);
            // printf("child process\n");
            /* The same client can have as many sequential requests as it wants*/
            /* Meanwhile, parent will be executing more fork calls*/
            /* The fork() was for different clients that may also have sequestial requests*/
            /* The child processes all have their own client addresses*/
            /* !!! Huge question: how do we only need one accept() - how can one accept() support multiple clients that have multiple requests? */
            /* this while loop will work for connection keep-alive*/
            // while (1)
            // {

            // client can keep sending requests
            n = recv(client_socket, buf, BUFSIZE, 0);
            /* Parse request: GET /Protocols/rfc1945/rfc1945 HTTP/1.1 */
            if (n < 0)
            {
                printf("timout\n");
                close(client_socket);
                exit(1);
                // error("ERROR receiving from client");
            }

            int handle_result = parse_request(client_socket, buf);
            // printf("Client request serviced...\n");
            memset(buf, 0, BUFSIZE);

            close(client_socket);
            exit(0);
            /* After parsing need to send response, this is handled in parse_request() as well*/
            /* meanwhile parent is creating more forks() for incoming requests*/
            //}
        }
        /* Parent needs to close client socket*/
        close(client_socket);

        /*Parent goes back up to the loop to handle more clients, child may be running multiple requests*/
    }

    /*parent shuts down the socket*/
    close(sockfd);
    return 0;
}

/* When do we close the client for keep-alive?*/