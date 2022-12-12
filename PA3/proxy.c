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
#include <stdbool.h>
#include <fcntl.h>
#include <pthread.h>
#include <assert.h>
#include <sys/mman.h>
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
int connect_count;
FILE *bl;      /* Global blocklist file*/
int blocklist; /* Flag to check if bl exists*/

typedef struct
{
    bool done;
    int writing;
    pthread_mutex_t mutex;
} shared_data;

static shared_data *data = NULL;

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
    int status;
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
    /* close main listening socket - sockfd*/
    // child processes may continue to finish servicing a request
    close(sockfd);
    check = 0;
}

/* https://stackoverflow.com/questions/5309471/getting-file-extension-in-c*/
const char *get_filename_ext(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename)
        return "";
    return dot + 1;
}

/* If quering the cache*/
int getContentType(char *contentType, const char *fileExtension)
{

    if (strcmp(fileExtension, "html") == 0 || strcmp(fileExtension, ".htm") == 0)
    {
        strcpy(contentType, "text/html");
    }
    else if (strcmp(fileExtension, "txt") == 0)
    {
        strcpy(contentType, "text/plain");
    }
    else if (strcmp(fileExtension, "png") == 0)
    {
        strcpy(contentType, "img/png");
    }
    else if (strcmp(fileExtension, "gif") == 0)
    {
        strcpy(contentType, "image/gif");
    }
    else if (strcmp(fileExtension, "jpg") == 0)
    {
        strcpy(contentType, "image/jpg");
    }

    else if (strcmp(fileExtension, "css") == 0)
    {
        strcpy(contentType, "text/css");
    }

    else if (strcmp(fileExtension, "js") == 0)
    {
        strcpy(contentType, "application/javascript");
    }

    else if (strcmp(fileExtension, "ico") == 0)
    {
        strcpy(contentType, "image/x-icon");
    }
    else
    {
        strcpy(contentType, "text/html");
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
    }
    return NULL;
}

void *Forbidden(int client, void *client_args)
{
    /* what headers should I include*/
    char response[BUFSIZE];
    bzero(response, sizeof(response));
    HTTP_REQUEST *client_request = (HTTP_REQUEST *)client_args;
    sprintf(response, "%s 403 Forbidden\r\nContent-Type: text/plain\r\nContent-Length: 20\r\nConnection: %s\r\n\r\nThis site is blocked", client_request->version, client_request->connection);
    printf("%s\n", response);
    send(client, response, sizeof(response), 0);
    if (strcmp(client_request->connection, "close") == 0)
    {
        close(client);
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
    }
    return NULL;
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
    HTTPResponseHeader http_response; /* to send back to client*/

    bzero(http_response.version, TEMP_SIZE);
    bzero(http_response.status, TEMP_SIZE);
    bzero(http_response.contentType, TEMP_SIZE);
    bzero(http_response.connection, TEMP_SIZE);

    // remote server we are acting as a client towards
    struct sockaddr_in httpserver;
    struct in_addr **addr_list = NULL;
    struct hostent *server = NULL;
    int connfd = 0;
    char IP[100];
    bzero(IP, sizeof(IP));
    int n = 0;
    FILE *cache_fd = NULL;
    ssize_t content_length_size = 0;
    int ok = 0; // flag for 200 status codes

    /* Just get version right away, part of server response*/
    strcpy(http_response.version, client_request->version);
    // printf("HTTP version response: %s\n", http_response.version);

    /* Some client may not send a connection type - dumb*/
    if (client_request->connection != NULL)
    {
        strcpy(http_response.connection, client_request->connection);
    }

    /* GET CONTENT TYPE!!!!!!*/
    const char *file_extention = get_filename_ext(client_request->file);
    getContentType(http_response.contentType, file_extention);

    printf("Get host by name: %s\n", client_request->hostname);
    server = gethostbyname(client_request->hostname);
    /* If above fails, send 404*/
    if (server)
    {
        // printf("Found server: %s \n", server->h_name);
    }
    else
    {
        // printf("404 Server Not Found\n");
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

    /* If we have a blocklist file in the CWD, otherwise nothing is blocklisted*/
    if (blocklist == 1)
    {
        char hostname[254];
        /* I open the blocklist in the main process so we need to reset it to the top*/
        fseek(bl, 0, SEEK_SET);
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
    }

    connfd = socket(AF_INET, SOCK_STREAM, 0);
    // printf("created socket\n");
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

    if (port == 0)
    {
        // printf("BRUHJ!\n");
        port = 80;
    }

    // printf("Port is %d  for %s\n", port, client_request->hostname);

    bzero((char *)&httpserver, sizeof(httpserver));
    httpserver.sin_family = AF_INET;
    httpserver.sin_addr.s_addr = inet_addr(IP);
    httpserver.sin_port = htons(port); // assuming port 80 for now

    /* Now we can make a connection to the resolved host*/

    int connection_status = connect(connfd, (struct sockaddr *)&httpserver, sizeof(httpserver));

    if (connection_status < 0)
    {
        error("Connect() failed:");
        client_request->status = 400; // need to verify if this is good to do
        return 4;
    }

    /* Now we relay the client request to the server*/
    memset(buf, 0, BUFSIZE);
    sprintf(buf, "GET /%s %s\r\nHost: %s\r\nConnection: close\r\n\r\n", client_request->file, client_request->version, client_request->hostname);
    printf("\n\n");
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

    /* Need to seperate http reponse header and payload*/
    char httpResponseHeader[BUFSIZE];
    bzero(httpResponseHeader, sizeof(httpResponseHeader));
    char *non_successful_status;
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

    printf("Content length for %s: %lu\n", client_request->hostname, content_length_size);

    /* If we dont get a successful response, we should NOT cache and send the entire response directly in this function*/
    if (strstr(httpResponseHeader, "200 OK") == NULL)
    {
        // printf("GOT NON 200 STATUS!\n");
        ok = -1;
    }

    // printf("ORIGIN RESPONSE HEADER:\n%s\n", httpResponseHeader);

    char *header_overflow = buf;
    char *check = strstr(buf, "\r\n\r\n");
    check = check + 4;

    int header_length = check - header_overflow;
    int left_to_read;
    // printf("Header length for %s: %d\n", client_request->file, header_length);

    /* Again, if we didnt get a successful response*/
    if (ok == -1)
    {
        ok = 0; // reset for keep alive
        // printf("PROXY sending:\n%s\n", httpResponseHeader);
        send(client, httpResponseHeader, sizeof(httpResponseHeader), 0);
        return 2; // 2 means dont send from cache
    }

    int bytes_written;
    int payload_bytes_recevied;

    /* FILE LOCKING*/
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_WRLCK;    /* F_RDLCK, F_WRLCK, F_UNLCK    */
    fl.l_whence = SEEK_SET; /* SEEK_SET, SEEK_CUR, SEEK_END */
    fl.l_start = 0;         /* Offset from l_whence         */
    fl.l_len = 0;           /* length, 0 = to EOF           */
    fl.l_pid = getpid();    /* our PID                      */
    cache_fd = fopen(cache_file, "wb");
    printf("writer %d has the lock for %s\n", fl.l_pid, client_request->file);
    fcntl(fileno(cache_fd), F_SETLKW, &fl); /* F_GETLK, F_SETLK, F_SETLKW */

    // pthread_mutex_lock(&data->mutex);

    if (content_length_size != 0)
    {
        if (bytes_read - header_length > content_length_size)
        {
            // we have the full payload, check is the full payload
            bytes_written = fwrite(check, 1, content_length_size, cache_fd);
            // printf("Full payload: wrote %d bytes\n", bytes_written);
        }
        else
        {
            /* Still write what we have*/
            payload_bytes_recevied = bytes_read - header_length; // PROBLEM HERE BRUHHHHHHHHH
            bytes_written = fwrite(check, 1, payload_bytes_recevied, cache_fd);
            // printf("Partial payload: wrote %d bytes\n", bytes_written);
            /* We need to keep reading*/
            left_to_read = content_length_size - payload_bytes_recevied;
            // printf("left to read %d bytes more bytes\n", left_to_read);
            int total_read = 0;
            while (total_read < left_to_read)
            {
                int receive_size = BUFSIZE;
                if (left_to_read - total_read < BUFSIZE)
                {
                    receive_size = left_to_read - total_read;
                }
                bzero(buf, BUFSIZE);
                bytes_read = recv(connfd, buf, receive_size, 0);
                bytes_written = fwrite(buf, 1, bytes_read, cache_fd);
                if (bytes_read != bytes_written)
                    printf("---\n---\n---\n---\n---\n");
                total_read += bytes_read;
            }

            printf("total written to file: %d\n", total_read + payload_bytes_recevied);
        }
    }

    fl.l_type = F_UNLCK; /* set to unlock same region */
    fcntl(fileno(cache_fd), F_SETLKW, &fl);
    printf("writer %d released the lock for %s\n", fl.l_pid, client_request->file);

    bzero(httpResponseHeader, BUFSIZE);
    memset(buf, 0, BUFSIZE);

    fclose(cache_fd);
    // pthread_mutex_unlock(&data->mutex);
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
    // printf("relative path: %s\n", relative_path);
    /* for checking time modifed*/
    struct stat file_stat;
    DIR *dir = opendir("./cache");
    if (access(relative_path, F_OK) == 0)
    {
        // printf("Found %s in cache!\n", relative_path);
        /* check time modified*/
        /* Source: https://www.ibm.com/docs/en/i/7.3?topic=ssw_ibm_i_73/apis/stat.html*/
        if (stat(relative_path, &file_stat) == 0)
        {
            time_t file_modified = file_stat.st_mtime; // The most recent time the contents of the file were changed.
            time_t now = time(NULL);                   // https://stackoverflow.com/questions/7550269/what-is-timenull-in-c#:~:text=The%20call%20to%20time(NULL,point%20to%20the%20current%20time.
            double time_left = difftime(now, file_modified);
            // printf("This many seconds have passed since last cache: %f\n", time_left);
            if (time_left > timeout)
            {
                // printf("%s expired and in cache, need to ping server again\n", relative_path);
                return NOT_CACHED;
            }
        }
        // printf("Not expired and in Cache!\n");
        return CACHED;
    }
    else
    {
        // printf("%s NOT FOUND, need to ping server!\n", relative_path);
        return NOT_CACHED;
    }
}

/* This is basically service_request from PA2*/
int send_from_cache(int client, void *client_args)
{

    HTTP_REQUEST *client_request = (HTTP_REQUEST *)client_args;
    HTTPResponseHeader http_response; /* to send back to client*/
    FILE *fp = NULL;                  // file descriptor for page to send to client
    ssize_t fsize;

    /* Just get version right away, part of server response*/
    strcpy(http_response.version, client_request->version);
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

    /* FILE LOCKING*/
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_RDLCK; /* F_RDLCK, F_WRLCK, F_UNLCK    */
    fl.l_pid = getpid(); /* our PID                      */
    fp = fopen(relative_path, "rb");
    fcntl(fileno(fp), F_SETLKW, &fl);
    printf("reader %d has the lock for %s\n", fl.l_pid, client_request->file);

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
    const char *file_extention = get_filename_ext(client_request->file);
    // printf("File extension: %s\n", file_extention);
    if (file_extention == NULL)
    {
        NotFound(client, client_request);
        return 0;
    }
    getContentType(http_response.contentType, file_extention);
    // printf("Reponse Content Type: %s\n", http_response.contentType);
    /* Graceful exit check?*/
    if (check == 0)
    {
        bzero(http_response.connection, sizeof(http_response.connection));
        strcpy(http_response.connection, "close");
    }

    char response_header[BUFSIZE];
    bzero(response_header, BUFSIZE);
    if (strcmp(client_request->connection, "keep-alive") == 0 || strcmp(client_request->connection, "Keep-alive") == 0)
    {
        sprintf(response_header, "%s 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: %s\r\n\r\n", http_response.version, http_response.contentType, fsize, http_response.connection);
    }
    else
    {
        sprintf(response_header, "%s 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: %s\r\n\r\n", http_response.version, http_response.contentType, fsize, http_response.connection);
    }

    /* This is the header that our proxy generates: same as PA2*/
    printf("PROXY RESPONSE HEADER:\n%s\n", response_header);
    send(client, response_header, strlen(response_header), 0);
    /* Now attach the payload*/
    int total_payload_sent = 0;
    int bytes_read;
    int bytes_sent;
    /* Generate actual payload to send with header status*/
    char payload[BUFSIZE];
    memset(payload, 0, BUFSIZE);
    while (total_payload_sent < fsize)
    {
        bytes_read = fread(payload, 1, BUFSIZE, fp);
        bytes_sent = send(client, payload, bytes_read, 0);
        memset(payload, 0, BUFSIZE);
        total_payload_sent += bytes_sent;
    }

    fl.l_type = F_UNLCK; /* set to unlock same region */
    fcntl(fileno(fp), F_SETLKW, &fl);
    fclose(fp);

    /* Do I need this or is connection close in the header enough?*/
    if ((strcmp(http_response.connection, "close") == 0) || (strcmp(http_response.connection, "Close") == 0))
    {
        printf("Closing client connection....\n");
        close(client);
        // exit(0);
    }

    // process needs to return back to unlock the mutex!
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

    HTTP_REQUEST client_request;
    client_request.method = NULL;
    client_request.URI = NULL;
    client_request.version = NULL;
    client_request.connection = NULL;
    client_request.hostname = NULL; // if included in request header
    client_request.ip = NULL;
    client_request.domain = NULL;
    client_request.file = NULL;
    client_request.portNo = NULL;
    client_request.messageBody = NULL;
    client_request.hash = NULL;
    client_request.blocklist = 0;
    client_request.keepalive = 0;
    client_request.status = 0;

    printf("Client request:\n%s\n", buf);
    /*Parse client request*/
    char temp_buf[BUFSIZE];
    char conn_buf[BUFSIZE];
    char host_buf[BUFSIZE];

    bzero(temp_buf, BUFSIZE);
    bzero(conn_buf, BUFSIZE);
    bzero(host_buf, BUFSIZE);

    strcpy(temp_buf, buf);
    strcpy(conn_buf, buf);
    strcpy(host_buf, buf);

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
    // sleep(2);

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
        // printf("REQUEST connection: %s\n\n", client_request.connection);
    }
    if (strcmp(client_request.version, "HTTP/1.0") == 0 && client_request.connection == NULL)
    {
        client_request.connection = "close";
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
        client_request.file = "";
        // printf("Client requesting %s\n", client_request.file);
    }
    else
    {
        client_request.file = file + 1;
        // printf("Client requesting %s\n", client_request.file);
    }

    char *port = strchr(client_request.URI, ':');
    if (port != NULL)
    {
        port = port + 1;
        char *token = strtok(port, "/");
        // printf("Token:%s\n", token);
        client_request.portNo = token;
    }
    else
    {
        client_request.portNo = '\0';
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
        printf("%s NOT IN CACHE\n", client_request.file);
        /* Not in cache so lets forward to the server!*/
        int server_response = relay(sock, &client_request, buf);
        /* Relay may return in the case of 403 error, we catch them here*/
        if (server_response == 4)
        {
            BadRequest(sock, &client_request);
            return 0;
        }
        if (client_request.blocklist == 1)
        {
            Forbidden(sock, &client_request);
            client_request.blocklist = 0;
            client_request.status = 0;
            // return back to received_request()
            return 0;
        }
        if (server_response == 2) // non 200 status code
        {
            return 0;
        }
        /* What I could do here is just store the payload in the cache and then call send from cache immediately after*/
        // printf("File has been cached..... now sending it from cache\n");
        // pthread_mutex_lock(&data->mutex);
        send_from_cache(sock, &client_request);
        // pthread_mutex_unlock(&data->mutex);
    }
    else if (cache_result == CACHED)
    {
        printf("%s IS CACHED\n", client_request.file);
        // pthread_mutex_lock(&data->mutex);
        send_from_cache(sock, &client_request);
        // pthread_mutex_unlock(&data->mutex);
    }

    return 0;
}

/*********THREAD/Process ENTRY ROUTINE**********/
int received_request(int sock)
{
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
    /* while loop breaks when client socket is closed*/

    /* If SIGINT is hit but we didnt timeout we should just close and exit*/
    if (check == 0)
    {
        close(sock);
        exit(0);
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
    // printf("%s\n", timeout);
    close(sock);
    // printf("Done!!!\n");
    //  return from entry point routine
    exit(0);
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
    connect_count = 0;

    /*Blocklost*/
    bl = fopen("./blocklist", "r");
    if (bl == NULL)
    {
        printf("No blocklist provided\n");
        blocklist = 0;
    }
    else
    {
        blocklist = 1;
    }

    /* Source: https://stackoverflow.com/questions/19172541/procs-fork-and-mutexes*/
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_SHARED | MAP_ANONYMOUS;
    data = mmap(NULL, sizeof(shared_data), prot, flags, -1, 0);
    assert(data);

    data->done = false;

    // initialise mutex so it works properly in shared memory
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&data->mutex, &attr);

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
    /* continously handle client requests */
    while ((client_socket = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen)) > 0)
    {
        // accepting a connection means creating a client socket
        /* this client_socket is how we send data back to the connected client */
        bzero(&clientaddr, sizeof(clientaddr));
        /*
            accept(): extracts the first
            connection request on the queue of pending connections for the
            listening socket, sockfd, creates a new connected socket, and
            returns a new file descriptor referring to that socket.  The
            newly created socket is not in the listening state.  The original
            socket sockfd is unaffected by this call.
        */
        /* Parent basically updates the client socket address for every iteration, with a new socket for that client*/
        // printf("Accept returned: %d\n", client_socket);
        if (client_socket < 0)
        {
            break;
        }
        /* Write logic for multiple connections - fork() or threads */
        /*TODO: graceful exit*/
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
            /* set timeout for child sockets - keep alive*/
            int handle_result = received_request(client_socket);
            // printf("Client request serviced...\n");
            /* Dont close socket until receive connection close or timeout of 10s*/
            // close(client_socket);
            // exit(0);
            /* After parsing need to send response, this is handled in parse_request() as well*/
            /* meanwhile parent is creating more forks() for incoming requests*/
            /* If SIGINT is hit but we didnt timeout we should just close and exit*/
            if (check == 0)
            {
                close(client_socket);
                exit(0);
            }
        }
        /* Parent needs to close client socket*/
        close(client_socket);
        /*Parent goes back up to the loop to handle more clients, child may be running multiple requests*/
    }
    /*Accept returns -1 when signal handler closes the socket, so we sleep and let the children finish*/
    printf("Gracefully exiting...\n");
    /*waiting for child processes to finish*/
    pid_t child_p;
    while ((child_p = wait(NULL)) > 0)
    {
        // printf("waiting on: %d\n", child_p);
    }

    /* If the blocklist file exists we close it at the end*/
    if (blocklist == 1)
    {
        fclose(bl);
    }
    munmap(data, sizeof(data));
    printf("Done\n");
    return 0;
}
