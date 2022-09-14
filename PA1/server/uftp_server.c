/*
 * uftp_server.c - A  UDP FTP server
 * usage: uftpserver <port>
 * Adapted from sample udp_server
 * Author: Vignesh Chandrasekhar
 * Collaborators: Collaborators: Freddy Perez
 */

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

#define BUFSIZE 13000

/*
Workflow:

Much of this PA seems to be dealing with file operations and simply sending and receiving the result of those file operations
Most of the upd code is already implemented

*For each command/user input from the client, we need to set up a send and receive
*The server will read what the client says and store the client input in some buffer array
***GET: Server should open the file that the client
    requested(in read mode), store its contents in a buffer, and send that buffer back to the client
***PUT: Opposite of get
***DELETE:
***ls:
***exit:

*/

void error(char *msg)
{
    perror(msg);
    exit(0);
}

// https://linux.die.net/man/2/recvfrom

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
    char *array[2];                // for storing command and file - if one is requested
    char *hostaddrp;               /* dotted decimal host addr string */
    int optval;                    /* flag value for setsockopt */
    int n;                         /* message byte size */
    char *command;
    char *file_requested;
    char error_message[] = "Invalid request!";
    int size_of_file;

    /*
     * check command line arguments
     */
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    /*
     * socket: create the parent socket
     */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
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

    /* Allow server to keep accepted requests from clients */
    while (1)
    {

        /*
         * recvfrom: receive a UDP datagram from a client, client must pass its own address info thru socket?
         */
        // printf("top marker\n");
        bzero(buf, sizeof(buf));
        n = recvfrom(sockfd, buf, BUFSIZE, 0,
                     (struct sockaddr *)&clientaddr, &clientlen); // stores client message in buf, this function blocks

        if (n < 0)
        {
            printf("waiting for client request\n");
        }

        // printf("Server got %s from client\n", buf); // need to parse this.....ugh
        strcpy(temp_buf, buf); // calling strcpy function
        /* now split the users response (buff) by delimeter */
        command = strtok(temp_buf, " "); // source: https://www.youtube.com/watch?v=34DnZ2ewyZo
        file_requested = strtok(NULL, " ");
        /*
         * gethostbyaddr: determine who sent the datagram , client. This returns a linked list: hostent
         */
        hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, // where is clientAddr initialized - is it populated by recvfrom via socket???
                              sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        if (hostp == NULL)
            error("ERROR on gethostbyaddr");
        hostaddrp = inet_ntoa(clientaddr.sin_addr);
        if (hostaddrp == NULL)
            error("ERROR on inet_ntoa\n");

        printf("server received datagram from %s (%s)\n",
               hostp->h_name, hostaddrp);
        printf("server received %lu/%d bytes: %s\n", strlen(buf), n, buf);

        if (strcmp(command, "get") == 0)
        {
            FILE *file_send; // server file descriptor, for get, server will open the file requested by client and write its contents into a SOCK-DGRM
            /*First check if file exists*/
            int fd = access(file_requested, F_OK | R_OK);
            if (!fd)
            {
                file_send = fopen(file_requested, "rb");
            }
            else
            {
                file_send = NULL;
                bzero(buf, sizeof(buf));
                char msg[] = "File does not exist";
                strcpy(buf, msg);
                n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&clientaddr, clientlen);
                continue;
            }

            /* buffer can fit all of the file! */
            if (file_send != NULL)
            {
                /* now read file contents into buffer*/
                // we open the file and copy its contents into 'file_buffer' and send that over to client using sendto
                /* source: https://stackoverflow.com/questions/14002954/c-programming-how-to-read-the-whole-file-contents-into-a-buffer */

                fseek(file_send, 0, SEEK_END);
                long fsize = ftell(file_send);
                fseek(file_send, 0, SEEK_SET);
                // printf("Size of file requested: %lu \n", fsize);
                bzero(buf, sizeof(buf));
                sprintf(buf, "%ld", fsize);
                n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&clientaddr, clientlen);

                /* If file is larger than buffer size */
                long sent = 0;
                bzero(buf, sizeof(buf));

                if (BUFSIZE > fsize)
                {
                    bzero(buf, sizeof(buf));
                    fread(buf, sizeof(char), BUFSIZE, file_send);
                    // printf("Contents of file_buffer: %s\n", buf);
                    n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&clientaddr, clientlen);
                    if (n < 0)
                        error("ERROR in sendto");
                }
                else
                {
                    /* CJ office hours: Need to do partial sends and receives if file is greater than buffer size! */
                    while (sent < fsize)
                    {
                        // printf("large file\n");
                        bzero(buf, sizeof(buf));
                        /* what we need to send is still more than what we can fit */
                        if ((fsize - sent) > BUFSIZE)
                        {
                            fread(buf, sizeof(char), BUFSIZE, file_send);
                            n = sendto(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, clientlen);
                            usleep(10000); // shoutout Jarek in class!

                            // printf("buf: %s \n", buf);
                            if (n < 0)
                                error("ERROR in sendto");
                            sent += n;
                        }
                        else
                        {
                            /*other wise just send whats left over, it should fit */
                            fread(buf, sizeof(char), fsize - sent, file_send);
                            n = sendto(sockfd, buf, fsize - sent, 0, (struct sockaddr *)&clientaddr, clientlen);
                            usleep(10000);
                            // printf("buf: %s \n", buf);
                            if (n < 0)
                                error("ERROR in sendto");
                            sent += n;
                        }
                    }
                    printf("Total bytes sent: %lu\n", sent);
                }
            }

            fclose(file_send);
        }
        else if (strcmp(command, "put") == 0)
        {

            /* https://stackoverflow.com/questions/4181784/how-to-set-socket-timeout-in-c-when-making-multiple-connections */
            /* https://stackoverflow.com/questions/13547721/udp-socket-set-timeout */
            /* COME BACK TO THIS */
            // struct timeval tv;
            // tv.tv_sec = 5;
            // tv.tv_usec = 0;
            // if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
            //     perror("Error");
            // }

            FILE *file_get;
            // write the contents on server_response into file_get: EVERYTHING IN UNIX IS A FILE!!!

            // need to check total number of bytes sent and received
            bzero(buf, sizeof(buf));
            n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientlen, &clientlen);
            if (strcmp(buf, "Don't create this file") == 0)
            {
                // printf("buf %s: \n", buf);
                continue;
            }
            else
            {

                /* Create temp char array so we dont override file_requested */
                char temp[BUFSIZE];
                strcpy(temp, file_requested);

                /* so if temp/file_requested contains a '/', we know its in a subdirectory so we have to parse it */
                if (strrchr(temp, '/') != NULL)
                {
                    // printf("requesting: %s\n", file_requested);
                    /*SOURCE: https://stackoverflow.com/questions/19639288/c-split-a-string-and-select-last-element */
                    char *file = strrchr(file_requested, '/');
                    // printf("file: %s\n", file);
                    char *file_requested = file + 1;
                    // printf("file requested: %s\n",file_requested);
                    file_get = fopen(file_requested, "wb");
                }
                else
                {
                    // printf("file requested: %s\n",file_requested);
                    file_get = fopen(file_requested, "wb");
                }

                char *p;
                size_of_file = strtol(buf, &p, 10);
                // printf("size of file: %lu \n", size_of_file);
            }
            printf("server received datagram from %s (%s)\n",
                   hostp->h_name, hostaddrp);
            printf("server received %lu/%d bytes: %s\n", strlen(buf), n, buf);
            long received = 0;
            bzero(buf, sizeof(buf));

            if (BUFSIZE > size_of_file)
            {
                bzero(buf, sizeof(buf));
                n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen); // this call blocks!!!!
                fwrite(buf, 1, n, file_get);
                printf("server received datagram from %s (%s)\n",
                       hostp->h_name, hostaddrp);
                printf("server received %lu/%d bytes: %s\n", strlen(buf), n, buf);
                fclose(file_get);
                char msg[] = "PUT successful";
                bzero(buf, sizeof(buf));
                strcpy(buf, msg);
                n = sendto(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, clientlen);
            }
            else
            {
                while (received < size_of_file)
                {
                    // printf("large file alert!\n");
                    bzero(buf, sizeof(buf));
                    n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen); // this call blocks!!!!
                    // printf("buf %s: \n", buf);
                    if (n < 0)
                    {
                        printf("Server timeout\n");
                        // error("ERROR in recvfrom");
                    }
                    else
                    {
                        fwrite(buf, 1, n, file_get);
                        received += n;
                    }
                    printf("server received datagram from %s (%s)\n", hostp->h_name, hostaddrp);
                    printf("server received %lu/%d bytes: %s\n", strlen(buf), n, buf);
                }

                // printf("closing file\n");
                fclose(file_get);
                // printf("Received %lu bytes\n", received);
                char msg[] = "PUT successful";
                bzero(buf, sizeof(buf));
                strcpy(buf, msg);
                n = sendto(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, clientlen);
            }
        }
        else if (strcmp(command, "delete") == 0)
        {
            /* Delete source: https://www.tutorialkart.com/c-programming/c-delete-file/ */
            if (file_requested == NULL)
            {
                printf("No file provided by client\n");
                n = sendto(sockfd, error_message, BUFSIZE, 0, (struct sockaddr *)&clientaddr, clientlen);
                if (n < 0)
                    error("ERROR in sendto");
            }
            else
            {

                char temp[BUFSIZE];
                strcpy(temp, file_requested);

                /* so if temp/file_requested contains a '/', we know its in a subdirectory so we have to parse it */
                if (strrchr(temp, '/') != NULL)
                {
                    // printf("requesting: %s\n", file_requested);
                    /*SOURCE: https://stackoverflow.com/questions/19639288/c-split-a-string-and-select-last-element */
                    char *file = strrchr(file_requested, '/');
                    printf("file: %s\n", file);
                    char *file_requested = file + 1;
                    printf("file requested: %s\n", file_requested);
                }
                else
                {
                    // printf("file requested: %s\n",file_requested);
                }

                printf("file requested: %s\n", file_requested);

                char server_response[100];
                snprintf(server_response, sizeof(server_response), "DELETE %s successful", file_requested);
                if (remove(file_requested) == 0)
                {
                    n = sendto(sockfd, server_response, BUFSIZE, 0, (struct sockaddr *)&clientaddr, clientlen);
                }
                else
                {
                    bzero(buf, sizeof(buf));
                    char *msg = "Delete failed, try again and make sure directory is right";
                    strcpy(buf, msg);
                    n = sendto(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, clientlen);
                }
            }

            printf("server received datagram from %s (%s)\n",
                   hostp->h_name, hostaddrp);
            printf("server received %lu/%d bytes: %s\n", strlen(buf), n, buf);
        }
        else if (strcmp(command, "ls") == 0)
        {
            char full_ls[BUFSIZE];

            FILE *fp;
            int status;
            char path[BUFSIZE];
            /* SOURCE: https://pubs.opengroup.org/onlinepubs/009696799/functions/popen.html */
            fp = popen("ls -l", "r");
            if (fp == NULL)
                /* Handle error */;

            while (fgets(path, BUFSIZE, fp) != NULL)
                // printf("%s", path);
                strcat(full_ls, path);
            strcat(full_ls, "\n");

            /* Source: https://www.youtube.com/watch?v=0pjtn6HGPVI */
            // DIR *directory;
            // struct dirent *entry;
            // char full_ls[BUFSIZE];
            // bzero(full_ls, sizeof(full_ls));
            // directory = opendir(".");
            // if (directory == NULL)
            // {
            //     return 1;
            // }

            // /* Keep reading directory entries */
            // while ((entry = readdir(directory)) != NULL)
            // {
            //     // printf("%s\n", entry->d_name);
            //     /* Source for concatenating strings */
            //     // https://stackoverflow.com/questions/2218290/concatenate-char-array-in-c
            //     strcat(full_ls, entry->d_name);
            //     strcat(full_ls, "\n");
            // }

            n = sendto(sockfd, full_ls, BUFSIZE, 0, (struct sockaddr *)&clientaddr, clientlen);

            // if (closedir(directory) == -1)
            // {
            //     error("Error closing directory");
            // }
        }
        else if (strcmp(command, "exit") == 0)

        {
            // printf("Client wants to exit\n");
            bzero(buf, sizeof(buf));
            char server_exit_response[] = "Server acknowledges client exit";
            strcpy(buf, server_exit_response);
            n = sendto(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, clientlen);
            if (n < 0)
                error("ERROR in sendto");
        }
    }
    close(sockfd);
    return 0;
}
