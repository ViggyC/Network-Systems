/*
    PA3 Network Systems
    HTTP Web Caching Proxy

    An extension of PA2: HTTP Web Server

    Author: Vignesh Chandrasekhar
    Collaborators: Freddy Perez and Cameron Brown
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
int sockfd;  /* server socket file descriptor*/
int check;   /*global flag for children to terminate on graceful exit*/
int timeout; /* global timeout value*/

pthread_mutex_t file_lock;
pthread_mutex_t cache_lock;
pthread_mutex_t exit_lock;

/*https://www.stackpath.com/edge-academy/what-is-keep-alive/#:~:text=Overview,connection%20header%20can%20be%20used.*/

/* Major note: all http response headers end in: \r\n\r\n */
/* Source: https://jameshfisher.com/2016/12/20/http-hello-world/ */
/*https://developer.mozilla.org/en-US/docs/Glossary/Response_header */

/* Need to figure out how accept() is handling different clients - it is blocking!!!!*/

/* The accept() call is used by a server to accept a
connection request from a client. When a connection is available,
the socket created is ready for use to read data from the process that requested the connection.
The call accepts the first connection on its queue of pending connections for the given socket socket.*/

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
    char *domain;
    char *file;
    char *portNo;
    char *messageBody;
    char *hash;
    int blocklist;
    int keepalive;
} HTTP_REQUEST;

typedef struct
{
    char version[TEMP_SIZE];
    char status[TEMP_SIZE];
    char contentType[TEMP_SIZE];
    char connection[TEMP_SIZE];
    int length;
} HTTPResponseHeader;

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
    // pthread_mutex_lock(&exit_lock);
    close(sockfd);
    check = 0;
    // pthread_mutex_unlock(&exit_lock);
}

/* If quering the cache*/
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

/********************************** Invalid Requests****************************************/
void *NotFound(int client, void *client_args)
{
    /* what should this response actually look like*/
    HTTP_REQUEST *client_request = (HTTP_REQUEST *)client_args;
    // printf("Not found connection: %s\n", client_request->connection);
    char Not_Found[BUFSIZE];
    bzero(Not_Found, sizeof(Not_Found));
    /* review this method of sending to the client*/
    sprintf(Not_Found, "%s 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nConnection: %s\r\n\r\n", client_request->version, client_request->connection);
    printf("%s\n", Not_Found);
    send(client, Not_Found, sizeof(Not_Found), 0);
    if (strcmp(client_request->connection, "close") == 0)
    {
        close(client);
        return NULL;
    }
    return NULL;
}

void *BadRequest(int client, void *client_args)
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
        return NULL;
    }
    return NULL;
}

void *MethodNotAllowed(int client, void *client_args)
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
        return NULL;
    }
    return NULL;
}

void *Forbidden(int client, void *client_args)
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
        return NULL;
    }
    return NULL;
}

void *HTTPVersionNotSupported(int client, void *client_args)
{
    char response[BUFSIZE];
    bzero(response, sizeof(response));
    HTTP_REQUEST *client_request = (HTTP_REQUEST *)client_args;
    sprintf(response, "HTTP/1.0 505 HTTP Version Not Supported\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n");
    printf("%s\n", response);
    send(client, response, sizeof(response), 0);
    if (strcmp(client_request->connection, "close") == 0)
    {
        close(client);
        return NULL;
    }
    return NULL; //?
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

/* If requested file is not in cache, ping the server for it*/
/* This function does not send anything to the client*/
/* It just stores the payload in the cache and then returns and then the program will send from cache like PA2*/
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
    ssize_t content_length_size;

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
    // printf("%s resolved to %s\n", client_request->hostname, IP);
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
            if (strcmp(client_request->ip, hostname) == 0 || strcmp(client_request->hostname, hostname) == 0)
            {
                /*This domain has been blocked*/
                printf("%s is blocked\n", hostname);
                // Forbidden(client, &client_request);
                client_request->blocklist = 1;
                return 0;
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

    int port;
    if (client_request->portNo == NULL)
    {
        port = 80;
    }
    else
    {
        port = atoi(client_request->portNo);
    }

    // printf("Port is %d\n", port);

    bzero((char *)&httpserver, sizeof(httpserver));
    httpserver.sin_family = AF_INET;
    httpserver.sin_addr.s_addr = inet_addr(IP);
    httpserver.sin_port = htons(port); // assuming port 80 for now

    /* Now we can make a connection to the resolved host*/
    int connection_status = connect(connfd, (struct sockaddr *)&httpserver, sizeof(httpserver));
    if (connection_status == -1)
    {
        printf("Connection failed\n");
        exit(1);
    }

    /* Now we relay the client request to the server*/
    memset(buf, 0, BUFSIZE);
    sprintf(buf, "GET /%s %s\r\nHost: %s\r\nConnection: %s\r\n\r\n", client_request->file, client_request->version, client_request->hostname, client_request->connection);
    printf("Forwarding request:\n%s\n", buf);

    /*need to check this!*/
    int bytes_sent = send(connfd, buf, strlen(buf), 0);
    // printf("bytes sent: %d\n", bytes_sent);
    /*This is where we store file in cache*/
    /* We can also send it to the client at the same time!*/
    int bytes_read;
    memset(buf, 0, BUFSIZE);
    char cache_file[strlen("cache/") + strlen(client_request->hash)]; // Buffer to store relative directory of file in cache
    strcpy(cache_file, "cache/");
    strcat(cache_file, client_request->hash);

    cache_fd = fopen(cache_file, "wb");

    /* Need to seperate http reponse header and payload*/
    char httpResponseHeader[BUFSIZE];
    bzero(httpResponseHeader, sizeof(httpResponseHeader));
    /* Ok this is the hard part*/
    /* I only want to store the payload*/
    /* First get the header*/
    // while we have not found our clrf

    /* GETTING SERVER RESPONSE*/
    bytes_read = recv(connfd, buf, BUFSIZE, 0);
    strcpy(httpResponseHeader, buf);
    /* Now we have the header and some of the payload: or all of the payload*/

    if (strstr(httpResponseHeader, "Content-Length:") != NULL)
    {
        char *content_length = strstr(httpResponseHeader, "Content-Length: ");
        char *length = strstr(content_length, " ");
        length = length + 1;
        content_length_size = atoi(length);
    }

    printf("Content length: %lu\n", content_length_size);
    char *header_overflow = buf;
    char *check = strstr(buf, "\r\n\r\n");
    check = check + 4;

    int header_length = check - header_overflow;
    int left_to_read;
    // printf("Header length: %d\n", header_length);
    //  printf("payload: %s\n", check);

    int bytes_written;
    int payload_bytes_recevied;

    /* We have the full payload in the buffer!!*/
    if (content_length_size != 0)
    {

        if (BUFSIZE - header_length > content_length_size)
        {
            // we have the full payload, check is the full payload
            bytes_written = fwrite(check, 1, content_length_size, cache_fd);
            printf("Full payload: wrote %d bytes\n", bytes_written);
        }
        else
        {
            /* Still write what we have*/
            payload_bytes_recevied = BUFSIZE - header_length;
            bytes_written = fwrite(check, 1, payload_bytes_recevied, cache_fd);
            printf("Partial payload: wrote %d bytes\n", bytes_written);
            /* We need to keep reading*/
            left_to_read = content_length_size - payload_bytes_recevied;
            printf("left to read %d bytes more bytes\n", left_to_read);
            int total_read = 0;
            while (total_read < left_to_read)
            {
                memset(buf, 0, BUFSIZE);
                bytes_read = recv(connfd, buf, BUFSIZE, 0);
                printf("read: %d\n", bytes_read);
                total_read += bytes_read;
                bytes_written = fwrite(buf, 1, bytes_read, cache_fd);
                printf("wrote %d bytes\n", bytes_written);
            }
        }
    }

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
/* Need to implement expiration!!!!!!!!!!!!!!!!!!*/
int check_cache(char *buf)
{

    FILE *fp;

    char relative_path[BUFSIZE];
    bzero(relative_path, sizeof(relative_path));
    strcat(relative_path, "./cache/");
    strcat(relative_path, buf);
    /* need to convert to md5 hash*/
    printf("relative path: %s\n", relative_path);

    /* for checking time modifed*/
    struct stat file_stat;
    DIR *dir = opendir("./cache");

    if (access(relative_path, F_OK) == 0)
    {
        printf("Found %s in cache!\n", relative_path);
        /* check time modified*/
        /* Source: https://www.ibm.com/docs/en/i/7.3?topic=ssw_ibm_i_73/apis/stat.html*/
        if (stat(relative_path, &file_stat) == 0)
        {
            time_t file_modified = file_stat.st_mtime; // The most recent time the contents of the file were changed.
            time_t now = time(NULL);                   // https://stackoverflow.com/questions/7550269/what-is-timenull-in-c#:~:text=The%20call%20to%20time(NULL,point%20to%20the%20current%20time.
            double time_left = difftime(now, file_modified);
            printf("This many seconds have passed since last cache: %f\n", time_left);
            if (time_left > timeout)
            {
                printf("%s expired and in cache, need to ping server again\n", relative_path);
                return NOT_CACHED;
            }
        }

        printf("Not expired and in Cache!\n");
        return CACHED;
    }
    else
    {
        printf("%s NOT FOUND, need to ping server!\n", relative_path);
        return NOT_CACHED;
    }
}

/* This is basically service_request from PA2*/
int send_from_cache(int client, void *client_args)
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

    char relative_path[TEMP_SIZE];
    bzero(relative_path, sizeof(relative_path)); // clear it!!!!!!!!!!

    /* hard code root directory www*/
    strcat(relative_path, "cache/");
    strcat(relative_path, client_request->hash);

    printf("sending from cache: %s\n", relative_path);
    fp = fopen(relative_path, "rb");

    /* This shouldn't happen but check anyways*/
    if (fp == NULL)
    {
        // printf("%s does not exist!\n", relative_path);
        NotFound(client, client_request);
        return 0;
    }

    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* GET CONTENT TYPE!!!!!!*/
    char *file_extention = strchr(client_request->file, '.');
    // printf("File extension: %s\n", file_extention);
    if (file_extention == NULL)
    {
        NotFound(client, client_request);
        return 0;
    }

    getContentType(http_response.contentType, file_extention);
    // printf("Reponse Content Type: %s\n", http_response.contentType);

    /* Generate actual payload to send with header status*/
    char payload[fsize];
    bzero(payload, fsize);
    fread(payload, 1, fsize, fp);
    printf("Buffer overflow?\n");

    /* Graceful exit check?*/
    // pthread_mutex_lock(&exit_lock);
    // if (check == 0)
    // {
    //     // pthread_mutex_unlock(&exit_lock);

    //     // printf("This is the last request\n");
    //     bzero(http_response.connection, sizeof(http_response.connection));
    //     strcpy(http_response.connection, "close");
    // }

    char response_header[BUFSIZE];
    if (strcmp(client_request->connection, "keep-alive") == 0 || strcmp(client_request->connection, "Keep-alive") == 0)
    {
        // printf("keep alive!\n");
        sprintf(response_header, "%s 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: %s\r\n\r\n", http_response.version, http_response.contentType, fsize, http_response.connection);
    }
    else
    {
        // printf("sending last request: %s\n", http_response.connection);
        sprintf(response_header, "%s 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: %s\r\n\r\n", http_response.version, http_response.contentType, fsize, http_response.connection);
    }

    /* This is the header that our proxy generates: same as PA2*/
    printf("%s\n", response_header);
    /* Now attach the payload*/
    char full_response[fsize + strlen(response_header)];
    strcpy(full_response, response_header);
    // use memcpy() to attach payload to header
    memcpy(full_response + strlen(full_response), payload, fsize);
    /* AND we got it! */
    /* the child processes will all be sending to different {client} addresses, per parent accept() */

    send(client, full_response, sizeof(full_response), 0);
    // printf("Full response size : %lu\n", sizeof(full_response));

    /* Do I need this or is connection close in the header enough?*/
    if ((strcmp(http_response.connection, "close") == 0) || (strcmp(http_response.connection, "Close") == 0))
    {
        // printf("Closing client connection....\n");
        close(client);
        return 0;
    }

    return 0;
}

/*
parse_request
Workflow:

1. Proxy will receive request from client
2. Proxy will parse the request into METHOD, URL, VERSION, HOSTNAME, and CONNECTION
    Store these values in the client request struct (same as PA2)
3. Proxy will use the hostname and the URL/filename to hash the requested file
4. If the generated hash is not in our cache => ping the server
5. Else just send from the cache

*/
int parse_request(int sock, char *buf)
{

    pthread_detach(pthread_self());
    HTTP_REQUEST client_request;
    // int n;
    /* This is the client socket*/
    // int sock = *(int *)socket_desc;
    // char buf[BUFSIZE];
    // memset(buf, 0, BUFSIZE);

    /* Add a loop for keep alive*/
    // n = recv(sock, buf, BUFSIZE, 0);
    /* KEEP ALIVE BABY*/
    // n = recv(sock, buf, BUFSIZE, 0);

    printf("Client request:\n%s\n", buf);
    /*Parse client request*/
    char temp_buf[BUFSIZE];
    char conn_buf[BUFSIZE];
    char host_buf[BUFSIZE];

    strcpy(temp_buf, buf);
    strcpy(conn_buf, buf);
    client_request.method = strtok(temp_buf, " "); // GET
    client_request.URI = strtok(NULL, " ");        // route/URI - relative path
    client_request.version = strtok(NULL, "\r\n"); // version, end in \r

    /* Parsing the Host: header*/
    char *domain = strstr(host_buf, "Host: ");
    strtok(domain, " ");
    client_request.hostname = strtok(NULL, "\r\n");

    /* If a port is included, we dont want to include it in DNS resolution that is done in relay()*/
    char *host = strtok(client_request.hostname, ":");

    char *connection_type = strstr(conn_buf, "Connection: ");
    strtok(connection_type, " ");
    client_request.connection = strtok(NULL, "\r\n");

    /* null terminating for safety*/
    client_request.method[strlen(client_request.method)] = '\0';
    client_request.version[strlen(client_request.version)] = '\0';

    /*This sleep is a debugging method to see the children during the graceful exit*/
    // sleep(3);

    if (client_request.method == NULL || client_request.URI == NULL || client_request.version == NULL)
    {
        // bad request?
        client_request.version = "HTTP/1.0";
        BadRequest(sock, &client_request);
        return 0;
    }
    if (strcmp(client_request.version, "HTTP/1.1") == 0 && client_request.connection == NULL)
    {
        client_request.connection = "keep-alive";
        client_request.keepalive = 1;
        // printf("REQUEST connection: %s\n\n", client_request.connection);
    }
    if (strcmp(client_request.version, "HTTP/1.0") == 0 && client_request.connection == NULL)
    {
        client_request.connection = "close";
        client_request.keepalive = 0; // initialized to keep alive
        // printf("REQUEST connection: %s\n\n", client_request.connection);
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
        // printf("Invalid HTTP version\n");
        client_request.connection = "keep-alive";
        HTTPVersionNotSupported(sock, &client_request);
        return 0;
    }

    /**********************PARSING AFTER ERROR HANDLING***************************/
    /**********************This is a successful request***************************/
    // need to extract origin server from complete URI
    // this will cut out http://
    // ex: http://www.example.com/testing.txt =>www.example.com/testing.txt
    char *urlSlash = strstr(client_request.URI, "//");
    if (urlSlash != NULL)
    {
        client_request.URI = urlSlash + 2;
    }

    /*Now get the actual file the client is requesting from the server*/
    // www.example.com/testing.txt => testing.txt
    // need to test this more!!!
    char *file = strchr(client_request.URI, '/');
    if (file == NULL || *(file + 1) == '\0')
    {
        client_request.file = "index.html";
        printf("Client requesting %s\n", client_request.file);
    }
    else
    {
        client_request.file = file + 1;
        printf("Client requesting %s\n", client_request.file);
    }

    char *port = strstr(client_request.URI, ":");
    if (port != NULL)
    {
        port = port + 1;
        char *token = strtok(port, "/");
        // printf("Token:%s\n", token);
        client_request.portNo = token;
    }

    printf("REQUEST method: %s\n", client_request.method);
    printf("REQUEST URI: %s\n", client_request.URI);
    printf("REQUEST version: %s\n", client_request.version);
    printf("REQUEST host: %s\n", client_request.hostname);
    printf("REQUEST port: %s\n", client_request.portNo);
    printf("REQUEST connection: %s\n", client_request.connection);
    printf("REQUEST file: %s\n", client_request.file);
    printf("\n\n");

    /* generate hash: all we need is the hostname and the filename*/
    char hash_input[strlen(client_request.hostname) + strlen(client_request.file) + 1];
    bzero(hash_input, sizeof(hash_input));
    strcpy(hash_input, client_request.hostname);
    strcat(hash_input, "/"); // add this to denote separation between domain and filename
    strcat(hash_input, client_request.file);

    char hash_output[strlen(client_request.hostname) + strlen(client_request.file) + 1];
    bzero(hash_output, sizeof(hash_output));

    /* calling hashing function*/
    md5_generator(hash_input, hash_output);
    /* now we can check if the request is already in our cache!*/
    client_request.hash = hash_output;

    /*Check cache here before pinging server*/
    int cache_result = check_cache(client_request.hash);
    /* this will return NOT_CACHED if its not in the cache directory our the timeout hit*/
    if (cache_result == NOT_CACHED)
    {
        /* Not in cache so lets forward to the server!*/
        relay(sock, &client_request, buf);
        if (client_request.blocklist == 1)
        {
            Forbidden(sock, &client_request);
            // return back to received_request()
            client_request.blocklist = 0;
            return 0;
        }
        else
        {
            /* What I could do here is just store the payload in the cache and then call send from cache immediately after*/
            printf("File has been cached..... now sending it from cache\n");
            send_from_cache(sock, &client_request);
            printf("sent from cache\n");
        }
    }
    else if (cache_result == CACHED)
    {
        send_from_cache(sock, &client_request);
    }

    // free(socket_desc);
    // printf("Done!!!\n");
    // return NULL;
    return 0;
}

/*********THREAD ENTRY ROUTINE**********/
void *received_request(void *socket_desc)
{
    pthread_detach(pthread_self());
    int sock = *(int *)socket_desc;
    char buf[BUFSIZE];
    memset(buf, 0, BUFSIZE);
    int n;

    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        perror("Error in socket timeout setting: ");
    }

    /* KEEP ALIVE BABY*/
    while ((n = recv(sock, buf, BUFSIZE, 0)) > 0)
    {

        /* reset timeout value*/
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        {
            perror("Error in socket timeout setting: ");
        }

        int handle_result = parse_request(sock, buf);
        memset(buf, 0, BUFSIZE);
    }

    /*10 seconds have passed so we timeout*/
    char timeout[BUFSIZE];
    bzero(timeout, sizeof(timeout));
    // printf("client that closed: %d\n", client_socket);
    sprintf(timeout, "HTTP/1.1 408 Request Timeout\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    /* This will only send in the case of a timout,
    if the server closes the socket on behalf of the client,
    then this will never send because the client already exited*/
    send(sock, timeout, sizeof(timeout), 0);
    printf("%s\n", timeout);
    free(socket_desc);
    close(sock);
    printf("Done!!!\n");
    // return from entry point routine
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
    /* SIGINT Graceful Exit Handler*/
    signal(SIGINT, sigint_handler);

    if (pthread_mutex_init(&cache_lock, NULL) != 0)
    {
        exit(1);
    }
    if (pthread_mutex_init(&file_lock, NULL) != 0)
    {
        exit(1);
    }

    if (pthread_mutex_init(&exit_lock, NULL) != 0)
    {
        exit(1);
    }

    /* Is timeout optional?*/
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
    /* spawn a thread for every incomming connection*/
    while ((client_socket = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen)) > 0)
    {

        /* Write logic for multiple connections - fork() or threads */
        /*TODO: graceful exit*/
        /* Parent spawns child processes to handle incoming requests*/
        pthread_t sniffer_thread;

        new_sock = malloc(1);
        *new_sock = client_socket;
        if (pthread_create(&sniffer_thread, NULL, received_request, (void *)new_sock) < 0)
        {
            perror("could not create thread");
            return 1;
        }
    }
    /*Accept returns -1 when signal handler closes the socket, so we sleep and let the children finish*/
    // printf("sleeping...\n");
    // /*10 second wait for children to finish before parent exits: CJ/mason office hours*/
    printf("Gracefully exiting...\n");
    sleep(10);
    exit(0);
}
