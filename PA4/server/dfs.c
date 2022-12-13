/*
 * dfs.c DFS TCP server
 * usage: ./dfs ./dfs<n> <port> 
 * Author: Vignesh Chandrasekhar
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
char dir[6]; /* File system directory for dfs<n>*/

pthread_mutex_t file_lock;

void error(char *msg)
{
    perror(msg);
    exit(0);
}

void *service_request(void *socket_desc)
{
    //threads have their own stack
    pthread_detach(pthread_self());
    int sock = *(int *)socket_desc;
    char buf[BUFSIZE];
    char temp_buf[BUFSIZE];
    bzero(temp_buf, sizeof(temp_buf));
    bzero(buf, sizeof(buf));

    int left_to_read;
    int payload_bytes_recevied;
    int header_length;
    char *header_overflow;
    char *check;
    char *fname;
    while(1){
        memset(buf, 0, BUFSIZE);
        int n;
        int is_header = 0;
        int num_files = 0;
        char *filename;
        char *command;
        char *chunk;
        int chunk_size;
        char * client; //for get for same filename different content
        int timestamp;
        char timestamp_buf[255];
        int done = 0;
        /* Server can continually receive mul*/
        n = recv(sock, buf, BUFSIZE, 0);
        printf("Received %d bytes\n", n);
        //printf("Client Request:\n%s\n", buf);
        if(n<0){
            error("Error in recv: ");
        }

        if(n==0){
            break;
        }

        strncpy(temp_buf, buf, n);
        /* If client has PUT all files then it will send an ACK*/
        if(strcmp(buf, "put done")==0 || strstr(buf, "put done")!=NULL){
            printf("Client PUT successful\n");
            close(sock);
            return NULL;
        }

        /* If client has PUT all files then it will send an ACK*/
        if(strcmp(buf, "get done")==0 || strstr(buf, "get done")!=NULL){
            printf("Client GET successful\n");
            close(sock);
            return NULL;
        }

        if(strcmp(buf, "ls done")==0 || strstr(buf, "ls done")!=NULL){
            printf("Client LS successful\n");
            close(sock);
            return NULL;
        }

        if(strcmp(buf , "ls")!=0 && strcmp(buf , "list")!=0){
            strtok(temp_buf, " " );
            command = strtok(NULL, "\r\n");
            printf("cmd: %s\n", command);
        }else{
            command = "ls";
            //printf("cmd: %s\n", command);
        }
        
        if(strcmp(buf, "list")!=0 && strcmp(buf, "ls")!=0){
            fname = strstr(temp_buf, "Filename: ");
            strtok(fname, " ");
            filename = strtok(NULL, "\r\n");
            //printf("fname: %s\n", filename);

            char *c = strstr(temp_buf, "Chunk: ");
            strtok(c, " ");
            chunk = strtok(NULL, "\r\n");
            //printf("chonk: %s\n", chunk);
        }
        
        if(strcmp(command, "put")==0 || strcmp(command, "PUT") ==0){
            char *content_length = strstr(buf, "Size: ");
            char *length = strstr(content_length, " ");
            length = length + 1;
            chunk_size = atoi(length);
            //printf("chunk size: %d\n", chunk_size);
            char bruh[BUFSIZE];
            bzero(bruh, BUFSIZE);
            strcpy(bruh, buf);
            char *ts = strstr(bruh, "Timestamp: ");
            char *time = strstr(ts, " ");
            time = time + 1;
            strtok(time, ".");
            //printf("milliseconds: %s\n", time);
            strcpy(timestamp_buf, time);
            //printf("timestamp buf: %s\n", timestamp_buf);

        }
        
        /* Parsing for partial writes!!!!!*/
        header_overflow = buf;
        check = strstr(buf, "\r\n\r\n");
        check = check + 4;
        header_length = check - header_overflow;
        //printf("Header length: %d\n", header_length);
        

        if(strcmp(command, "put")==0){
            char relative_path[BUFSIZE]; // ./dfs#/filename.partition
            /* We will need to recv again to recieve the chunk*/
            strcpy(relative_path, dir);
            strcat(relative_path, "/");
            strcat(relative_path, filename);
            strcat(relative_path, "-");
            strcat(relative_path, chunk);
            strcat(relative_path, "-");
            strcat(relative_path, timestamp_buf);
            printf("Relative path: %s\n", relative_path);

            pthread_mutex_lock(&file_lock);
            FILE * fp = fopen(relative_path, "wb");
            int bytes_written;
            //printf("n: %d, header length: %d, chunk size: %d\n", n, header_length, chunk_size);
            /*Write what we have*/
            payload_bytes_recevied = n - header_length; 
            bytes_written = fwrite(check, 1, payload_bytes_recevied, fp);
            //printf("Partial payload: wrote %d bytes\n", bytes_written);
            /* We need to keep reading*/
            left_to_read = chunk_size - payload_bytes_recevied;
            //printf("left to read %d bytes more bytes\n", left_to_read);
            int total_read = 0;
            while (total_read < left_to_read)
            {
                int receive_size = BUFSIZE;
                if (left_to_read - total_read < BUFSIZE)
                {
                    receive_size = left_to_read - total_read;
                }
                bzero(buf, BUFSIZE);
                n = recv(sock, buf, receive_size, 0);
                bytes_written = fwrite(buf, 1, n, fp);
                if (n != bytes_written)
                    printf("---\n---\n---\n---\n---\n");
                total_read += n;
            }
            //printf("total written to file: %d\n", total_read + payload_bytes_recevied);
            fclose(fp);
            pthread_mutex_unlock(&file_lock);


            /* Send an acknowledement before client can send next chunk so we dont cram the buffer*/
            int send_bytes;
            send_bytes = send(sock, "PUT successful", sizeof("PUT successful"), 0);
            //printf("Bytes sent: %d\n", send_bytes);

        }else if(strcmp(command, "get")==0){
            char file_chunk[BUFSIZE]; // ./dfs#/filename.partition
            bzero(file_chunk, BUFSIZE);
            size_t fsize;
            int bytes_sent;
            int bytes_receieved;
            char recv_buf[BUFSIZE];
            bzero(recv_buf, BUFSIZE);

            /* We grep on {filename-chunk#} to pull the most recent one across the directories*/
            /* This is for the case where two clients put the same filename but different content*/
            strcat(file_chunk, filename);
            strcat(file_chunk, "-");
            strcat(file_chunk, chunk);   

            /* Setting up Linux command for getting most recent filename-chunk#*/
            char file_bullshit[BUFSIZE];
            bzero(file_bullshit, BUFSIZE);
            strcat(file_bullshit, "ls ");
            strcat(file_bullshit, dir);
            strcat(file_bullshit, " | grep ");
            strcat(file_bullshit, file_chunk);
            strcat(file_bullshit, " | sort -r");

            /* popen() returns a pipe with the data we requested: last writer filename-chunk#*/
            FILE * fp  = popen(file_bullshit,"r");
            char buf[BUFSIZE];
            bzero(buf, BUFSIZE);
            fscanf(fp, "%[^\n]", buf);
            printf("File: %s\n", buf);
         
            if(strstr(buf, file_chunk)==NULL){
                //printf("%s does not exist.....ping a different server\n", relative_path);
                /* client will see that the chunk was not found so it will hit up a different dfs server*/
                bytes_sent = send(sock, "Not Found", sizeof("Not Found"), 0);
                /*Server will go back up to while loop to get next chunk request*/
            }else{
                printf("%s exists\n", file_chunk);
                char relative_path[BUFSIZE];
                bzero(relative_path, BUFSIZE);
                strcat(relative_path, dir);
                strcat(relative_path, "/");
                strcat(relative_path, buf);
                printf("Relative path: %s\n", relative_path);
                pthread_mutex_lock(&file_lock);
                FILE * file = fopen(relative_path, "rb");
                fseek(file, 0, SEEK_END);
                fsize = ftell(file);
                fseek(file, 0, SEEK_SET);
                char size_buf[1024];
                sprintf(size_buf, "%lu", fsize);
                bytes_sent = send(sock, size_buf, sizeof(size_buf), 0);
                /* If dfs has the chunk, we can receive another request from the client for the actual chunk*/
                bytes_receieved = recv(sock, recv_buf, sizeof(recv_buf), 0); //"ready"
                //printf("bytes recieved: %d\n", bytes_receieved);
                //printf("received: %s\n", recv_buf);
                /* Read chunk into buffer*/
                char * chunk = malloc(fsize);
                bzero(chunk, fsize);
                fread(chunk, fsize, 1, file);
                fclose(file);
                pthread_mutex_unlock(&file_lock);
                bytes_sent = send(sock, chunk, fsize, 0);
                free(chunk);
            }

        }else if(strcmp(command, "ls")==0){
            //printf("Client wants to list\n");
            DIR *directory;
            struct dirent *entry;
            char full_ls[BUFSIZE];
            bzero(full_ls, sizeof(full_ls));
            directory = opendir(dir);
            if (directory == NULL)
            {
                return NULL;
            }

            /* Keep reading directory entries */
            while ((entry = readdir(directory)) != NULL)
            {
                // printf("%s\n", entry->d_name);
                /* Source for concatenating strings */
                // https://stackoverflow.com/questions/2218290/concatenate-char-array-in-c
                if(strcmp(entry->d_name, ".")!=0 && strcmp(entry->d_name, "..")!=0){
                    strcat(full_ls, entry->d_name);
                    strcat(full_ls, "\n");
                }
                
            }
            //printf("length of dir: %lu\n", strlen(full_ls));
            full_ls[strlen(full_ls)] = '\0';
            printf("%s\n", full_ls);
            n = send(sock, full_ls, strlen(full_ls), 0);
        }
    }

    return NULL;

}

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
    char *command;
    char *file_requested;
    int server_num;
    char server_dir[5];
    int client_socket;             /* each process will have its own*/
    int *new_sock;

    if(pthread_mutex_init(&file_lock, NULL) != 0){
      printf("Cannot init mutex\n");
      return 1;
    }
    

    if (argc != 3)
    {
        fprintf(stderr, "missing local file directory and or port\n");
        exit(1);
    }

    portno = atoi(argv[2]);
    strcpy(dir, argv[1]);
    /* Create local directory for dfs server based on argv[1]*/
    mkdir(dir, 0777);


    /*
     * socket: create the parent socket
     */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets
     * us rerun the server immediately after we kill it;
     * otherwise we have to wait about 20 secs.
     * Eliminates "ERROR on binding: Address already in use" error.
     */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
               (const void *)&optval, sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); // This is an IP address that is used when we don't want to bind a socket to any specific IP. - google
    serveraddr.sin_port = htons((unsigned short)portno);

    /*
     * bind: associate the parent socket with a port
     */
    if (bind(sockfd, (struct sockaddr *)&serveraddr,
             sizeof(serveraddr)) < 0)
        error("ERROR on binding");

    clientlen = sizeof(clientaddr);

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

    while ((client_socket = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen)) > 0)
    {

        //printf("Got a connection\n");
        /* Write logic for multiple connections - fork() or threads */
        /*TODO: graceful exit*/
        /* Parent spawns child processes to handle incoming requests*/
        pthread_t sniffer_thread;

        new_sock = malloc(sizeof(int));
        *new_sock = client_socket;
        if (pthread_create(&sniffer_thread, NULL, service_request, (void *)new_sock) < 0)
        {
            perror("could not create thread");
            return 1;
        }
    }
    return 0;
}