
/*
 * dfs.c DFS TCP server
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
char dir[6];

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
    char header_buf[BUFSIZE];
    char filename_buf[BUFSIZE];
    char chunk_buf[BUFSIZE];
    int left_to_read;
    int payload_bytes_recevied;
    int header_length;
    char *header_overflow;
    char *check;
    while(1){
        memset(buf, 0, BUFSIZE);
        int n;
        int is_header = 0;
        int num_files = 0;
        char *filename;
        char *command;
        char *chunk;
        int chunk_size;
        int done = 0;
        /* Server can continually receive mul*/
        n = recv(sock, buf, BUFSIZE, 0);
        if(n<0){
            error("Error in recv: ");
        }

      
        strcpy(temp_buf, buf);
        strcpy(header_buf, buf);
        printf("Received %d bytes\n", n);
        printf("Received:\n%s\n", buf);

        /* If client has put or gotten all files then it will send an ACK*/
        if(strcmp(buf, "done")==0 || strstr(buf, "done")!=NULL){
            printf("Client PUT successful\n");
            break;
        }

        /* If we see the header carriage return in the buffer, we have to parse it*/
        strtok(temp_buf, " " );
        command = strtok(NULL, "\r\n");
        printf("cmd: %s\n", command);

        char *fname = strstr(temp_buf, "Filename: ");
        strtok(fname, " ");
        filename = strtok(NULL, "\r\n");
        printf("fname: %s\n", filename);

        char *c = strstr(temp_buf, "Chunk: ");
        strtok(c, " ");
        chunk = strtok(NULL, "\r\n");
        printf("chonk: %s\n", chunk);
        
        if(strcmp(command, "put")==0 || strcmp(command, "PUT") ==0){
            char *content_length = strstr(buf, "Size: ");
            char *length = strstr(content_length, " ");
            length = length + 1;
            chunk_size = atoi(length);
            printf("chonk size: %d\n", chunk_size);
        }
        

        header_overflow = buf;
        check = strstr(buf, "\r\n\r\n");
        check = check + 4;
        header_length = check - header_overflow;
        printf("Header length: %d\n", header_length);
        

        if(strcmp(command, "put")==0){
            char relative_path[BUFSIZE]; // ./dfs#/filename.partition
            /* We will need to recv again to recieve the chunk*/
            strcpy(relative_path, dir);
            strcat(relative_path, "/");
            strcat(relative_path, filename);
            strcat(relative_path, "-");
            strcat(relative_path, chunk);
            printf("Relative path: %s\n", relative_path);
            FILE * fp = fopen(relative_path, "wb");
            int bytes_written;
            printf("n: %d, header length: %d, chunk size: %d\n", n, header_length, chunk_size);
            /*Write what we have*/
            payload_bytes_recevied = n - header_length; 
            bytes_written = fwrite(check, 1, payload_bytes_recevied, fp);
            printf("Partial payload: wrote %d bytes\n", bytes_written);
            /* We need to keep reading*/
            left_to_read = chunk_size - payload_bytes_recevied;
            printf("left to read %d bytes more bytes\n", left_to_read);
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

            printf("total written to file: %d\n", total_read + payload_bytes_recevied);
            fclose(fp);

            /* Send an acknowledement before client can send next chunk so we dont cram the buffer*/
            int send_bytes;
            send_bytes = send(sock, "PUT successful", sizeof("PUT successful"), 0);
            printf("Bytes sent: %d\n", send_bytes);


        }else if(strcmp(command, "get")==0){

            char relative_path[BUFSIZE]; // ./dfs#/filename.partition
            size_t fsize;
            int bytes_sent;
            /* We will need to recv again to recieve the chunk*/
            strcpy(relative_path, dir);
            strcat(relative_path, "/");
            strcat(relative_path, filename);
            strcat(relative_path, "-");
            strcat(relative_path, chunk);
            printf("Relative path: %s\n", relative_path);
            if( access(relative_path, F_OK) ==0 ){
                printf("%s exists\n", relative_path);
                FILE * fp = fopen(relative_path, "rb");
                fseek(fp, 0, SEEK_END);
                fsize = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                char size_buf[1024];
                sprintf(size_buf, "%lu", fsize);
                bytes_sent = send(sock, size_buf, sizeof(size_buf), 0);
            }else{
                bytes_sent = send(sock, "Not Found", sizeof("Not Found"), 0);
            }

            


        }else if(strcmp(command, "ls")==0){

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