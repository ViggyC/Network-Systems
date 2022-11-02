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
    char *hostname; // if included in request header
    char *ip;
    char *file;
    char *portNo;
    char *messageBody;
    char *hash;
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

    /* Closes global listening sock*/
    close(sockfd);
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
    exit(0);
}

int BadRequest(int client, void *client_args)
{
    char BAD_REQUEST[BUFSIZE];
    bzero(BAD_REQUEST, sizeof(BAD_REQUEST));
    HTTP_REQUEST *client_request = (HTTP_REQUEST *)client_args;
    // snprintf(response);
    /* connection header?*/
    sprintf(BAD_REQUEST, "HTTP/1.0 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n");
    printf("%s\n", BAD_REQUEST);
    send(client, BAD_REQUEST, sizeof(BAD_REQUEST), 0);
    exit(0);
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
    exit(0);
}

int Forbidden(int client, void *client_args)
{
    /* what headers should I include*/
    char response[BUFSIZE];
    bzero(response, sizeof(response));
    HTTP_REQUEST *client_request = (HTTP_REQUEST *)client_args;
    sprintf(response, "%s 403 Forbidden\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n", client_request->version);
    printf("%s\n", response);
    send(client, response, sizeof(response), 0);
    exit(0);
}

int HTTPVersionNotSupported(int client, void *client_args)
{
    char response[BUFSIZE];
    bzero(response, sizeof(response));
    HTTP_REQUEST *client_request = (HTTP_REQUEST *)client_args;
    sprintf(response, "HTTP/1.0 505 HTTP Version Not Supported\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n");
    printf("%s\n", response);
    send(client, response, sizeof(response), 0);
    exit(0);
}

int isDirectory(const char *path)
{
    struct stat statbuf;
    if (stat(path, &statbuf) != 0)
        return 0;
    return S_ISDIR(statbuf.st_mode);
}

void md5_generator(char *original, char *md5_hash)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *original++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    sprintf(md5_hash, "%lu", hash);
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
int relay(int client, void *client_args, char *buf)
{
    HTTP_REQUEST *client_request = (HTTP_REQUEST *)client_args;
    // remote server we are acting as a client towards
    struct sockaddr_in httpserver;
    struct in_addr **addr_list;
    struct hostent *server;
    int connfd;
    char IP[100];
    bzero(IP, sizeof(IP));
    int n;
    FILE *cache_fd;

    printf("Get host by name: %s\n", client_request->hostname);
    server = gethostbyname(client_request->hostname);
    /* If above fails, send 404*/
    if (server)
    {
        printf("Found server: %s \n", server->h_name);
    }
    else
    {
        printf("404 Server Not Found\n");
        NotFound(client, &client_request);
    }

    /* need to convert hostname to IP address*/
    addr_list = (struct in_addr **)server->h_addr_list;
    for (int i = 0; addr_list[i] != NULL; i++)
    {
        strcpy(IP, inet_ntoa(*addr_list[i]));
        // break after first find
        break;
    }
    printf("%s resolved to %s\n", client_request->hostname, IP);
    client_request->ip = IP;

    /*Blocklost*/
    FILE *bl = fopen("./blocklist", "r");
    char hostname[254];
    while (fgets(hostname, sizeof(hostname), bl))
    {
        // printf("%s\n", hostname);
        if ((hostname[0] != '\n'))
        {
            hostname[strlen(hostname) - 1] = '\0';
            if (strcmp(client_request->ip, hostname) == 0)
            {
                /*This domain has been blocked*/
                printf("%s is blocked\n", hostname);
                Forbidden(client, &client_request);
            }
        }
    }
    fclose(bl);

    connfd = socket(AF_INET, SOCK_STREAM, 0);
    if (connfd == -1)
    {
        printf("socket creation failed...\n");
        exit(0);
    }

    bzero((char *)&httpserver, sizeof(httpserver));
    httpserver.sin_family = AF_INET;
    httpserver.sin_addr.s_addr = inet_addr(IP);
    httpserver.sin_port = htons(80); // assuming port 80 for now

    /* Now we can make a connection to the resolved host*/
    int connection_status = connect(connfd, (struct sockaddr *)&httpserver, sizeof(httpserver));
    if (connection_status == -1)
    {
        printf("Connection failed\n");
    }

    /* Now we relay the client request to the server*/
    memset(buf, 0, BUFSIZE);
    sprintf(buf, "GET /%s %s\r\nHost: %s\r\n\r\n", client_request->file, client_request->version, client_request->hostname);
    printf("Forwarding request:\n%s\n", buf);

    /*need to check this!*/
    int bytes_sent = send(connfd, buf, strlen(buf), 0);
    // printf("bytes sent: %d\n", bytes_sent);
    memset(buf, 0, BUFSIZE);

    /*This is where we store file in cache*/
    /* We can also send it to the client at the same time!*/
    int store;
    char cache_file[strlen("cache/") + strlen(client_request->hash)]; // Buffer to store relative directory of file in cache
    strcpy(cache_file, "cache/");
    strcat(cache_file, client_request->hash);
    cache_fd = fopen(cache_file, "wb");

    /* this loop is not exiting*/
    /* need a different condition? carriage return?*/
    while ((store = recv(connfd, buf, BUFSIZE, 0)) > 0)
    {
        if (store < 0)
        {
            printf("ERROR in recvfrom\n");
            break;
        }

        // need to also store in cache
        //  fprintf(fp, "%s", buf);
        printf("buf: %s", buf);
        send(client, buf, store, 0);
        int bytes_written = fwrite(buf, 1, store, cache_fd);
        // reset buf for next read
        printf("wrote %d bytes\n", bytes_written);
        memset(buf, 0, BUFSIZE);
    }

    printf("out here\n");
    fclose(cache_fd);

    // /* So now we have sent the clients request to the resolved host, now we can get its response*/
    // bzero(buf, sizeof(buf));
    // while ((n = recv(connfd, buf, BUFSIZE, 0)) > 0)
    // {                            // read data from input socket
    //     send(client, buf, n, 0); // send data to output socket
    //     bzero(buf, sizeof(buf));
    // }

    return 0;
}

/* Call this at the beginning in case we have what client wants*/
/* cant use fopen to query cache, need to use DIRENT*/
int check_cache(char *buf)
{

    FILE *fp;

    char relative_path[BUFSIZE];
    bzero(relative_path, sizeof(relative_path));
    strcat(relative_path, "./cache/");
    strcat(relative_path, buf);

    /* need to convert to md5 hash*/
    printf("relative path: %s\n", relative_path);

    return NOT_CACHED;
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

    pthread_detach(pthread_self());

    int n;
    int sock = *(int *)socket_desc;
    char buf[BUFSIZE];
    memset(buf, 0, BUFSIZE);

    /* Add a loop for keep alive*/
    n = recv(sock, buf, BUFSIZE, 0);

    printf("Client request:\n%s\n", buf);

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

    char *connection_type = strstr(conn_buf, "Connection: ");
    strtok(connection_type, " ");
    client_request.connection = strtok(NULL, "\r\n");

    /*************************REQUEST INFO********************************/
    /*Blocklost*/
    FILE *bl = fopen("./blocklist", "r");
    char hostname[254];
    while (fgets(hostname, sizeof(hostname), bl))
    {
        // printf("%s\n", hostname);
        if ((hostname[0] != '\n'))
        {
            hostname[strlen(hostname) - 1] = '\0';

            if (strcmp(client_request.hostname, hostname) == 0)
            {
                /*This domain has been blocked*/
                printf("%s is blocked\n", hostname);
                Forbidden(sock, &client_request);
            }
        }
    }

    fclose(bl);

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

    printf("REQUEST method: %s\n", client_request.method);
    printf("REQUEST URI: %s\n", client_request.URI);
    printf("REQUEST version: %s\n", client_request.version);
    printf("REQUEST host: %s\n", client_request.hostname);
    printf("REQUEST connection: %s\n", client_request.connection);
    printf("REQUEST file: %s\n", client_request.file);
    printf("\n\n");

    /* generate hash*/
    char hash_input[strlen(client_request.hostname) + strlen(client_request.file) + 1];
    bzero(hash_input, sizeof(hash_input));
    strcpy(hash_input, client_request.hostname);
    strcat(hash_input, "/");
    strcat(hash_input, client_request.file);

    char hash_output[strlen(client_request.hostname) + strlen(client_request.file) + 1];
    bzero(hash_output, sizeof(hash_output));

    md5_generator(hash_input, hash_output);

    client_request.hash = hash_output;

    /*Check cache here before pinging server*/
    // instead of client_request.file, pass in the md5 hash
    int cache_result = check_cache(client_request.file);
    if (cache_result == NOT_CACHED)
    {
        relay(sock, &client_request, buf);
    }

    /* After parsing and hanlding bad requests, pass routine to service the actual file*/
    /* Pass in client request struct as arg*/
    // int handle = service_request(client, &client_request);
    free(socket_desc);
    close(sock);
    return NULL;
}

/* I think this should remain the same from PA2*/
int main(int argc, char **argv)
{
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
    check = 1;
    int *new_sock;
    int portno; /* port to listen on */
    int timeout;

    /* SIGINT Graceful Exit Handler*/
    signal(SIGINT, sigint_handler);

    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <port> <timeout>\n", argv[0]);
        exit(1);
    }

    portno = atoi(argv[1]);
    timeout = atoi(argv[2]);

    /*listening socket*/
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
