/*
 * dfc.c DFS TCP client
 * Author: Vignesh Chandrasekhar
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <openssl/md5.h>

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
#define CONF_SIZE 1024

#define DFS1 1
#define DFS2 2
#define DFS3 3
#define DFS4 4


int socket_array[1];
int live_servers[1];//global array to see which servers we can retrieve/put files to 
int y; //global num for number of servers in conf file
int num_servers = 0;

void error(char *msg)
{
    perror(msg);
    exit(0);
}

/* Source: https://www.geeksforgeeks.org/c-program-count-number-lines-file/*/
int get_num_servers(){
    FILE * conf_fd;
    conf_fd = fopen("./dfc.conf", "r");
    int count = 0;
    char c; 
    if (conf_fd == NULL)
    {
        printf("config file does not exist!\n");
        return 1;
    }
    for (c = getc(conf_fd); c != EOF; c = getc(conf_fd))
        if (c == '\n') // Increment count if this character is newline
            count = count + 1;
    // Close the file
    fclose(conf_fd);
    //printf("The file ./dfc.conf has %d lines\n ", count);
    return count;
}

/* Source: https://stackoverflow.com/questions/10324611/how-to-calculate-the-md5-hash-of-a-large-file-in-c */
int md5_hash(char * filename){
    unsigned char c[MD5_DIGEST_LENGTH];
    int i;
    FILE *inFile = fopen (filename, "rb");
    MD5_CTX mdContext;
    int bytes;
    unsigned char data[1024];
    int x = 0;

    if (inFile == NULL) {
        printf ("%s can't be opened.\n", filename);
        return 0;
    }

    MD5_Init (&mdContext);
    while ((bytes = fread (data, 1, 1024, inFile)) != 0)
        MD5_Update (&mdContext, data, bytes);
    MD5_Final (c,&mdContext);
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        int md5Char = (int)c[i];
        x = (x*16 + md5Char) % y;
    }
    fclose (inFile);
    printf("Got hash\n");
    return x;
}

/* This function makes a connect() to every server in the dfc.conf file*/
/* This should be called in a loop with some int array holding all the server indexes*/
int dfs_connect(int port, int server_num){
    int serverlen;
    socket_array[server_num]; //i +1
    struct sockaddr_in serveraddr; // we cast this to struct sockaddr
    struct hostent *server;
    char *hostname;         // from user input
    socket_array[server_num] = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_array[server_num] < 0)
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname("127.0.0.1"); 
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;                              // internet domains
    bcopy((char *)server->h_addr_list[0],                         // note: h_addr = h_addrlist[0]
          (char *)&serveraddr.sin_addr.s_addr, server->h_length); // need to figureout what this is doing
    serveraddr.sin_port = htons(port);
    // printf("Server h_name: %s\n", server->h_name);

    /* send the message to the server */
    serverlen = sizeof(serveraddr);
    // connect the client socket to server socket
    if (connect(socket_array[server_num], (struct sockaddr*)&serveraddr, serverlen)
        != 0) {
        printf("connection with the server dfs%d failed...\n", server_num+1);
        live_servers[server_num] = 0;
        return 1;
        
    }
    else
        printf("connected to the server dfs%d\n", server_num+1);
        live_servers[server_num] = 1; //if 1, server is available

    return 0;
}

/* For PUT*/
void send_chunk(char *chunk, char*filename, char chunk_num, int dfs_num, int chunck_length){
    int n;
    char header[BUFSIZE];
    bzero(header, sizeof(header));
    sprintf(header, "Command: put\r\nFilename: %s\r\nChunk: %c\r\nSize: %d\r\n\r\n", filename, chunk_num, chunck_length);
    printf("%s\n", header);
    char packet[chunck_length + strlen(header)];
    strcpy(packet, header);
    // use memcpy() to attach chunk to header
    memcpy(packet + strlen(packet), chunk, chunck_length);
    n = send(socket_array[dfs_num-1], packet, sizeof(packet), 0);
    printf("Sent %d header bytes\n", n);

}

void write_chunk(int dfs_server, FILE * fp){

}

int chunk_query(char *filename, char chunk_num, int dfs_server){
    int sent;
    int received;
    char buf[BUFSIZE];
    printf("Querying  %s-%c from dfs%d\n", filename, chunk_num, dfs_server+1);
    char get_request[BUFSIZE];
    bzero(get_request, BUFSIZE);
    sprintf(get_request, "Command: get\r\nFilename: %s\r\nChunk: %c\r\n\r\n", filename, chunk_num);
    sent= send(socket_array[dfs_server-1], get_request, sizeof(get_request), 0);
    received = recv(socket_array[dfs_server-1], buf, BUFSIZE, 0 );
    //printf("Got %d bytes from client\n", received);
    printf("Got %s  from client\n", buf);
    if(strcmp(buf, "Not Found")==0){
        return 0;
    }
    return atoi(buf);

}

int main(int argc, char **argv)
{

    int sockfd, portno, n; // socket file descriptor, portnumber for local connection to server, n for result of operations
    int serverlen;
    char buf[BUFSIZE];      // buffer we will send to server for ftp services
    bzero(buf, sizeof(buf));
    char temp_buf[BUFSIZE]; // for parsing user input
    char file_from_server[BUFSIZE];
    char *command; // One of => GET PUT DELETE ls EXIT
    char *file_requested;
    long size_of_file;
    int server_num;
    char server_dir[5];
    char * conf_file;
    char conf_buff[CONF_SIZE];
    int num_files;
    char* line = NULL;
    ssize_t len = 0; 
    char request_info[BUFSIZE]; //command line arguments

    /* check command line arguments */
    if (argc < 2)
    {
        fprintf(stderr, "usage: config file\n");
        return 1;
    }

    command  = argv[1];
    if( strcmp(command, "get")==0 ||strcmp(command, "put") ==0 || strcmp(command, "ls")==0 ){
        //do nothing
    }else{
        printf("Invalid request\n");
        return 0;
    }

    num_files = argc -2; //1 for executable, 1 for command (GET, PUT, LS)
    int num_servers = get_num_servers();
    printf("Number of servers in config file: %d\n", num_servers);

    /* Open config file and store port numbers of the DFS servers*/
    /* Pass these port numbers to connect()*/
    FILE * conf_fd;
    conf_fd = fopen("./dfc.conf", "r");
    fseek(conf_fd, 0, SEEK_END);
    fseek(conf_fd, 0, SEEK_SET);
    if (conf_fd == NULL)
    {
        printf("config file does not exist!\n");
        return 1;
    }

    int dfs_port;
    int connections_made = 0;
    for(int i=0; i<num_servers; i++){
        getline(&line, &len, conf_fd);
        line[strlen(line) - 1] = '\0';
        dfs_port = atoi(strchr(line, ':') + 1);
        printf("Connecting to port: %d\n", dfs_port);
        if(dfs_connect(dfs_port, i)==0){
            connections_made++;
        } 
    }
    y = connections_made; //available servers


    /***********************CONNECTIONS MADE*************************/

    printf("Connections made to required servers: %d\n", connections_made);
    for(int i=0; i<num_servers; i++){
        printf("DFS%d status: %d\n", i+1, live_servers[i]);
    }

    /* If we cant connect to the number of servers in the config file minus 1, we cant do the PUT operation*/
    /* Ex: 4 serverss in conf file but we can only connect to 2 -> cannot PUT*/
    if( (connections_made <num_servers-1) && strcmp(command, "put")==0){
        for(int i=0; i<argc-2; i++){
            printf("%s put failed\n", argv[i+2]);
        }
        return 1;
    }

    
    if(strcmp(command, "get")==0 ||strcmp(command, "GET") ==0){
        char * filename;
        for(int i=0; i<argc-2; i++){
            filename = argv[i+2];
            printf("get %s\n", filename);   
            /* get length of file so we can split it up*/
            FILE *fp = fopen(filename, "wb");
            int x = md5_hash(filename);
            printf("x: %d\n", x);

            /* Logic: for each chunk, go through every live dfs server and see if it has the chunk, get its size*/
            for(int i=1; i<2; i++){
                for(int dfs=1; dfs<5; dfs++){
                    int chunk_size = chunk_query(filename,i +'0', dfs);
                    printf("Chunk size: %d\n", chunk_size);
                    if(chunk_size>0){
                        /* Now we can get the chunk and write to the file*/
                        write_chunk(dfs, fp);
                        break;
                    }
                }
            }

        }

    }else if(strcmp(command, "put")==0 ||strcmp(command, "PUT") ==0){
        char * filename;
        /* For every file requested by cmd*/
        for(int i=0; i<argc-2; i++){
            filename  = argv[i+2];
            printf("put %s\n", filename);
            /* get length of file so we can split it up*/
            FILE* f = fopen(filename, "rb");
            if(f==NULL){
                printf("%s does not exist\n", filename);
                continue; //continue onto next file 
            }
            fseek(f, 0, SEEK_END);
            size_t fsize = ftell(f);
            fseek(f, 0, SEEK_SET);
            printf("%s is %lu bytes long\n", filename, fsize);
            int x = md5_hash(filename);
            printf("x: %d\n", x);

            /* Now split up the file and send to live_servers[] based on x hash value*/
            /* Assuming just 4 parts for testing*/
            char p1[fsize/4];
            char p2[fsize/4];
            char p3[fsize/4];
            char p4[fsize - 3*(fsize/4)]; //remainder if not divisible by 4
            fread(p1, sizeof(p1), 1, f);
            fread(p2, sizeof(p2), 1, f);
            fread(p3, sizeof(p3), 1, f);
            fread(p4, sizeof(p4), 1, f);
            fclose(f);

            if(x==0){

                if(live_servers[DFS1-1]==1){
                    send_chunk(p1, filename, '1', DFS1, sizeof(p1) );
                    recv(socket_array[DFS1-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                    send_chunk(p2, filename,'2',  DFS1, sizeof(p2) );
                    recv(socket_array[DFS1-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                }

                if(live_servers[DFS2-1]){
                    send_chunk(p2, filename,'2',  DFS2, sizeof(p2) );
                    recv(socket_array[DFS2 - 1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                    send_chunk(p3, filename, '3', DFS2, sizeof(p3) );
                    recv(socket_array[DFS2 - 1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                }

                if(live_servers[DFS3-1]==1){
                    send_chunk(p3, filename, '3', DFS3, sizeof(p3) );
                    recv(socket_array[DFS3 - 1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                    send_chunk(p4, filename, '4', DFS3, sizeof(p4) );
                    recv(socket_array[DFS3 - 1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                }
                

                if(live_servers[DFS4-1]==1){
                    send_chunk(p4, filename, '4', DFS4, sizeof(p4) );
                    recv(socket_array[DFS4 - 1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                    send_chunk(p1, filename, '1', DFS4, sizeof(p1) );
                    recv(socket_array[DFS4 - 1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                }

            }else if(x==1){

                if(live_servers[DFS1-1]==1){
                    send_chunk(p4, filename, '4', DFS1, sizeof(p4) );
                    recv(socket_array[DFS1-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                    send_chunk(p1, filename, '1', DFS1, sizeof(p1) );
                    recv(socket_array[DFS1-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                }
                
                if(live_servers[DFS2-1]==1){
                    send_chunk(p1, filename, '1', DFS2, sizeof(p1) );
                    recv(socket_array[DFS2-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                    send_chunk(p2, filename, '2', DFS2, sizeof(p2) );
                    recv(socket_array[DFS2-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                }
                

                if(live_servers[DFS3-1]==1){
                    send_chunk(p2, filename, '2', DFS3, sizeof(p2) );
                    recv(socket_array[DFS3-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                    send_chunk(p3, filename, '3', DFS3, sizeof(p3) );
                    recv(socket_array[DFS3-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                }
                

                if(live_servers[DFS4-1] ==1){
                    send_chunk(p3, filename, '3', DFS4, sizeof(p3) );
                    recv(socket_array[DFS4-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                    send_chunk(p4, filename, '4', DFS4, sizeof(p4) );
                    recv(socket_array[DFS4-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                }
                
            }else if(x==2){

                if(live_servers[DFS1-1]==1){
                    send_chunk(p3, filename,'3',  DFS1, sizeof(p3) );
                    recv(socket_array[DFS1-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                    send_chunk(p4, filename, '4', DFS1, sizeof(p4) );
                    recv(socket_array[DFS1-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                }
                

                if(live_servers[DFS2-1]==1){
                    send_chunk(p4, filename,'4',  DFS2, sizeof(p4) );
                    recv(socket_array[DFS2-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                    send_chunk(p1, filename,'1', DFS2, sizeof(p1) );
                    recv(socket_array[DFS2-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                }
               

                if(live_servers[DFS3-1]==1){
                    send_chunk(p1, filename, '1',DFS3, sizeof(p1) );
                    recv(socket_array[DFS3-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                    send_chunk(p2, filename,'2', DFS3, sizeof(p2) );
                    recv(socket_array[DFS3-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                }
                
                if(live_servers[DFS4-1]==1){
                    send_chunk(p2, filename,'2', DFS4, sizeof(p2) );
                    recv(socket_array[DFS4-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                    send_chunk(p3, filename, '3',DFS4, sizeof(p3) );
                    recv(socket_array[DFS4-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                }

                
            }else if(x==3){
                if(live_servers[DFS1-1]==1){
                    send_chunk(p2, filename,'2', DFS1, sizeof(p2) );
                    recv(socket_array[DFS1-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                    send_chunk(p3, filename,'3',DFS1, sizeof(p3) );
                    recv(socket_array[DFS1-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                }

                if(live_servers[DFS2-1]==1){
                    send_chunk(p3, filename, '3',DFS2, sizeof(p3) );
                    recv(socket_array[DFS2-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                    send_chunk(p4, filename,'4',DFS2, sizeof(p4) );
                    recv(socket_array[DFS2-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                }


                if(live_servers[DFS3-1]==1){
                    send_chunk(p4, filename,'4', DFS3, sizeof(p4) );
                    recv(socket_array[DFS3-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                    send_chunk(p1, filename,'1', DFS3, sizeof(p1) );
                    recv(socket_array[DFS3-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                }

                if(live_servers[DFS4-1]==1){

                    send_chunk(p1, filename,'1', DFS4, sizeof(p1) );
                    recv(socket_array[DFS4-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk
                    send_chunk(p2, filename, '2',DFS4, sizeof(p2) );
                    recv(socket_array[DFS4-1], buf, BUFSIZE, 0 ); //receive ack before sending next chunk

                }
            }

        }

        for(int i=0; i<num_servers; i++){
            if(live_servers[i]==1){
                n = send(socket_array[i], "done", strlen("done"), 0);
                printf("Sent %d bytes\n", n);

            }
        }


    }else if(strcmp(command, "ls")==0 ||strcmp(command, "LS") ==0){
        send(socket_array[0], command, sizeof(command), 0);

    }


    return 0;

}