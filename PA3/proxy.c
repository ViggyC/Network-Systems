/*
    PA3 Network Systems
    HTTP Cache Proxy

    Author: Vignesh Chandrasekhar
    Collaborators: Freddy Perez and James Vogenthaler
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <signal.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#define BUFSIZE 8192
#define TEMP_SIZE 1024
#define CACHED 1
#define NOT_CACHED 2
/*https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Connection*/

/* GLOBAL listen socket descriptor*/
int sockfd; /* server socket file descriptor*/
int check;  /*global flag for children to terminate on graceful exit*/

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
    char *origin;   // if not included in request header, but in complete URI
    char *hostname; // if included in request header
    int http_connection;
    char *file;
    char *portNo;
    char *messageBody;
    char hash[BUFSIZE];
} HTTP_REQUEST;

/* This request goes from the proxy (this) to the http server*/
typedef struct
{
    char *method;
    char *URI;
    char *version;
    char *connection;
    char *host;
    int portNo;
    char *messageBody;
} RELAY_REQUEST;

/* This request goes from the proxy (this) to the http server*/
typedef struct
{

} SERVER_RESPONSE;

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
    /* close main listening socket - sockfd*/
    // child processes may continue to finish servicing a request
    // printf("sig: %d\n", sig);
    close(sockfd);
    // pid_t child_pid;
    // while ((child_pid = wait(NULL)) > 0)
    //     ;
    // // {
    //     printf("Parent: %d\n", getpid());
    //     printf("CHild: %d\n", child_pid);
    // }
    check = 0;
}

/********************************** Invalid Requests****************************************/
int NotFound(int client, void *client_args)
{
    /* what should this response actually look like*/
    HTTP_REQUEST *client_request = (HTTP_REQUEST *)client_args;
    // printf("Not found connection: %s\n", client_request->connection);
    char Not_Found[BUFSIZE];
    bzero(Not_Found, sizeof(Not_Found));
    /* review this method of sending to the client*/
    sprintf(Not_Found, "%s 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n", client_request->version);
    printf("%s\n", Not_Found);
    send(client, Not_Found, sizeof(Not_Found), 0);
    if (strcmp(client_request->connection, "close") == 0)
    {
        close(client);
    }
    return 0;
}

int BadRequest(int client, void *client_args)
{
    char BAD_REQUEST[BUFSIZE];
    bzero(BAD_REQUEST, sizeof(BAD_REQUEST));
    HTTP_REQUEST *client_request = (HTTP_REQUEST *)client_args;
    // snprintf(response);
    /* connection header?*/
    sprintf(BAD_REQUEST, "HTTP/1.0 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nConnection: %s\r\n\r\n", client_request->connection);
    printf("%s\n", BAD_REQUEST);
    send(client, BAD_REQUEST, sizeof(BAD_REQUEST), 0);
    if (strcmp(client_request->connection, "close") == 0)
    {
        close(client);
    }
    return 0;
}

int MethodNotAllowed(int client, void *client_args)
{
    /* what headers should I include*/
    char response[BUFSIZE];
    bzero(response, sizeof(response));
    HTTP_REQUEST *client_request = (HTTP_REQUEST *)client_args;
    sprintf(response, "%s 405 Method Not Allowed\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nConnection: %s\r\n\r\n", client_request->version, client_request->connection);
    printf("%s\n", response);
    send(client, response, sizeof(response), 0);
    if (strcmp(client_request->connection, "close") == 0)
    {
        close(client);
    }
    return 0;
}

int Forbidden(int client, void *client_args)
{
    /* what headers should I include*/
    char response[BUFSIZE];
    bzero(response, sizeof(response));
    HTTP_REQUEST *client_request = (HTTP_REQUEST *)client_args;
    sprintf(response, "%s 403 Forbidden\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nConnection: %s\r\n\r\n", client_request->version, client_request->connection);
    printf("%s\n", response);
    send(client, response, sizeof(response), 0);
    if (strcmp(client_request->connection, "close") == 0)
    {
        close(client);
    }
    return 0;
}

int HTTPVersionNotSupported(int client, void *client_args)
{
    char response[BUFSIZE];
    bzero(response, sizeof(response));
    HTTP_REQUEST *client_request = (HTTP_REQUEST *)client_args;
    sprintf(response, "HTTP/1.0 505 HTTP Version Not Supported\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nConnection: %s\r\n\r\n", client_request->connection);
    printf("%s\n", response);
    send(client, response, sizeof(response), 0);
    if (strcmp(client_request->connection, "close") == 0)
    {
        close(client);
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

/* Send Back to CLIENT - exactly what server sends back to us*/
int service_request(int client, void *client_args)
{

    /* Graceful exit*/
    if (check == 0)
    {
    }

    return 0;
}

/* If requested file is not in cache, ping the server for it*/
int ping_server(int client, void *client_args, char *buf)
{
    printf("Client Request: %s\n", buf);
    HTTP_REQUEST *client_request = (HTTP_REQUEST *)client_args;

    // remote server we are acting as a client towards
    struct sockaddr_in httpserver;
    struct hostent *server;

    server = gethostbyname(client_request->hostname);
    /* If above fails, send 404*/
    if (server)
    {
        printf("Found server\n");
        printf("%s: \n", server->h_name);
    }
    else
    {
        NotFound(client, &client_request);
    }

    client_request->http_connection = socket(AF_INET, SOCK_STREAM, 0);
    bzero((char *)&httpserver, sizeof(httpserver));
    httpserver.sin_family = AF_INET;
    httpserver.sin_port = htons(80); // assuming port 80 for now

        return 0;
}

/* Call this at the beginning in case we have what client wants*/
int check_cache(char *buf)
{

    FILE *fp;

    char relative_path[BUFSIZE];
    bzero(relative_path, sizeof(relative_path));
    strcat(relative_path, "./cache/");
    strcat(relative_path, buf);
    printf("relative path: %s\n", relative_path);

    fp = fopen(relative_path, "rb");
    if (fp == NULL)
    {
        printf("File not found in cache, need to hit up server\n");
        return NOT_CACHED;
    }
    else
    {
        /* Send file from cache*/
        printf("Found file in cache!\n");
    }

    return 0;
}

/* Only AFTER HITTING UP SERVER, store the file in the cache*/
int store_in_cashe(char *buf)
{
    return 0;
}

/* different processes for different requests will be handling this routine */
/* Once a valid request is received, the proxy will parse the requested URL into the following 3 parts:*/
void *parse_request(void *socket_desc)
{
    // printf("Parsing the HTTP request in this routine \n");
    // printf("Parsing the HTTP request in this routine \n");
    pthread_detach(pthread_self());
    free(socket_desc);

    int n;
    int sock = *(int *)socket_desc;
    char buf[BUFSIZE];
    memset(buf, 0, BUFSIZE);
    n = recv(sock, buf, BUFSIZE, 0);
    buf[strlen(buf) - 1] = '\0';
    printf("Client Request:\n%s\n", buf);

    // printf("Client Request:\n%s\n", buf);

    /*Parse client request*/
    HTTP_REQUEST client_request;
    char temp_buf[BUFSIZE];
    char conn_buf[BUFSIZE];
    char host_buf[BUFSIZE];

    strcpy(temp_buf, buf);
    strcpy(conn_buf, buf);
    client_request.method = strtok(temp_buf, " "); // GET
    client_request.URI = strtok(NULL, " ");        // route/URI - relative path
    client_request.version = strtok(NULL, "\r\n"); // version, end in \r

    // You can do this right after a gethostbyname() call as this will return an IP address only for valid hostnames.

    char *host = strstr(host_buf, "Host: ");
    strtok(host, " ");
    client_request.hostname = strtok(NULL, "\r\n");

    // char *connection_type = strstr(conn_buf, "Connection: ");
    // strtok(connection_type, " ");
    // client_request.connection = strtok(NULL, "\r\n");

    /*************************REQUEST INFO********************************/
    printf("REQUEST method: %s\n", client_request.method);
    printf("REQUEST URI: %s\n", client_request.URI);
    printf("REQUEST version: %s\n", client_request.version);
    printf("REQUEST host: %s\n", client_request.hostname);
    // printf("REQUEST connection: %s\n\n", client_request.connection);

    /*This sleep is a debugging method to see the children during the graceful exit*/
    // sleep(3);
    // printf("Children slept for 5 ms\n");

    if (client_request.method == NULL || client_request.URI == NULL || client_request.version == NULL)
    {
        // bad request?
        client_request.version = "HTTP/1.0";
        BadRequest(sock, &client_request);
        return 0;
    }

    if (strcmp(client_request.method, "GET") != 0)
    {
        /* Bad request according to writeup*/
        BadRequest(sock, &client_request);
        return 0;
    }

    if (strcmp(client_request.version, "HTTP/1.1") == 0 || strcmp(client_request.version, "HTTP/1.0") == 0)
    {
        // do nothing
    }
    else
    {
        printf("Invalid HTTP version\n");
        HTTPVersionNotSupported(sock, &client_request);
        return 0;
    }

    /* null terminating for safety*/
    client_request.method[strlen(client_request.method)] = '\0';
    client_request.version[strlen(client_request.version)] = '\0';

    /*********************************************PARSING**********************************************/
    // need to extract origin server from complete URI

    char *urlSlash = strstr(client_request.URI, "//");
    if (urlSlash != NULL)
    {
        client_request.URI = urlSlash + 2; // Strip http:// prefix as it will fail gethostbyname otherwise
    }

    printf("REQUEST origin: %s\n", client_request.origin);
    printf("REQUEST URI: %s\n", client_request.URI);

    /*Now get the actual file the client is requesting from the server*/
    char *file = strchr(client_request.URI, '/');
    if (file == NULL || *(file + 1) == '\0')
    {
        printf("Client requesting index.html\n");
        client_request.file = "index.html";
    }
    else
    {
        client_request.file = file + 1;
    }

    printf("REQUEST file: %s\n", client_request.file);

    /*Check cache here before pinging server*/
    int cache_result = check_cache(client_request.file);
    if (cache_result == NOT_CACHED)
    {
        printf("Ping server\n");
        ping_server(sock, &client_request, buf);
    }

    /* After parsing and hanlding bad requests, pass routine to service the actual file*/
    /* Pass in client request struct as arg*/
    // int handle = service_request(client, &client_request);

    close(sock);
    return NULL;
}

/* I think this should remain the same from PA2*/
int main(int argc, char **argv)
{
    int portno;                    /* port to listen on */
    socklen_t clientlen;           /* byte size of client's address -- need to look into this, seems this is filled in by recvfrom */
    struct sockaddr_in serveraddr; /* PROXY addr */
    struct sockaddr_in clientaddr; /* client addr */
    struct hostent *hostp;         /* client host info */
    char temp_buf[BUFSIZE];        // for parsing user input
    char *hostaddrp;               /* dotted decimal host addr string */
    int optval;                    /* flag value for setsockopt */
    int n;                         /* message byte size */
    int client_socket;             /* each process will have its own*/
    int child_socket;
    signal(SIGINT, sigint_handler);
    check = 1;
    int *new_sock;

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

    /* Creating Proxy's local cache/file system off the bat*/
    mkdir("cache", 0777);

    /* continously handle client requests */
    while ((client_socket = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen)) > 0)
    {

        /* Write logic for multiple connections - fork() or threads */
        /*TODO: graceful exit*/
        /* Parent spawns child processes to handle incoming requests*/
        pthread_t sniffer_thread;

        new_sock = malloc(1);
        *new_sock = client_socket;
        if (pthread_create(&sniffer_thread, NULL, parse_request, (void *)new_sock) < 0)
        {
            perror("could not create thread");
            return 1;
        }
    }
    /*Accept returns -1 when signal handler closes the socket, so we sleep and let the children finish*/
    // printf("sleeping...\n");
    // /*10 second wait for children to finish before parent exits: CJ/mason office hours*/
    // sleep(10);
    printf("Gracefully exiting...\n");
    /*waiting for child processes to finish*/
    pid_t child_p;
    while ((child_p = wait(NULL)) > 0)
    {
        // printf("waiting on: %d\n", child_p);
    }

    printf("Done\n");
    // close(sockfd);
    return 0;
}

//  while ((n = recv(source_sock, buffer, BUF_SIZE, 0)) > 0) { // read data from input socket
//         send(destination_sock, buffer, n, 0); // send data to output socket
//     }