/*
 * uftp_server.c - A  UDP FTP server
 * usage: uftpserver <port>
 * Adapted from sample udp_server
 * Author: Vignesh Chandrasekhar
 * Collaborators:
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>

#define BUFSIZE 1024

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
    char error_message[BUFSIZE] = "Invalid request!";
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
            error("ERROR in recvfrom");
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
                        bzero(buf, sizeof(buf));
                        /* what we need to send is still more than what we can fit */
                        if (fsize - sent > BUFSIZE)
                        {
                            fread(buf, sizeof(char), BUFSIZE, file_send);
                            n = sendto(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, clientlen);
                            // printf("buf: %s \n", buf);
                            if (n < 0)
                                error("ERROR in sendto");
                            sent += n;
                        }
                        else
                        {
                            /*other wise just send whats left over, it should fit */
                            fread(buf, sizeof(char), BUFSIZE, file_send);
                            n = sendto(sockfd, buf, fsize - sent, 0, (struct sockaddr *)&clientaddr, clientlen);
                            // printf("buf: %s \n", buf);
                            if (n < 0)
                                error("ERROR in sendto");
                            sent += n;
                        }
                    }
                }
            }

            fclose(file_send);
        }
        else if (strcmp(command, "put") == 0)
        {

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
                char *p;
                size_of_file = strtol(buf, &p, 10);
                file_get = fopen(file_requested, "wb"); // getting seg fault

                // printf("size of file: %lu \n", size_of_file);
            }
            printf("server received datagram from %s (%s)\n",
                   hostp->h_name, hostaddrp);
            printf("server received %lu/%d bytes: %s\n", strlen(buf), n, buf);
            long received = 0;

            if (BUFSIZE > size_of_file)
            {
                bzero(buf, sizeof(buf));
                n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen); // this call blocks!!!!
                fwrite(buf, 1, n, file_get);
                fclose(file_get);
                printf("server received datagram from %s (%s)\n",
                       hostp->h_name, hostaddrp);
                printf("server received %lu/%d bytes: %s\n", strlen(buf), n, buf);
            }
            else
            {
                while (received < size_of_file)
                {
                    bzero(buf, sizeof(buf));
                    /* what we need to send is still more than what we can fit */
                    if (size_of_file - received > BUFSIZE)
                    {
                        n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen); // this call blocks!!!!
                        fwrite(buf, 1, n, file_get);
                        // printf("buf %s: \n", buf);
                        if (n < 0)
                            error("ERROR in sendto");
                        received += n;
                        printf("server received datagram from %s (%s)\n", hostp->h_name, hostaddrp);
                        printf("server received %lu/%d bytes: %s\n", strlen(buf), n, buf);
                    }
                    else
                    {
                        /*other wise just send whats left over, it should fit */
                        n = recvfrom(sockfd, buf, size_of_file - received, 0, (struct sockaddr *)&clientaddr, &clientlen); // this call blocks!!!!
                        fwrite(buf, 1, n, file_get);

                        // printf("buf: %s \n", buf);

                        if (n < 0)
                            error("ERROR in sendto");
                        received += n;
                        printf("server received datagram from %s (%s)\n", hostp->h_name, hostaddrp);
                        printf("server received %lu/%d bytes: %s\n", strlen(buf), n, buf);
                    }
                }
                fclose(file_get);
            }
            char msg[] = "PUT successful";
            bzero(buf, sizeof(buf));
            strcpy(buf, msg);
            n = sendto(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, clientlen);
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
                char server_response[BUFSIZE];
                snprintf(server_response, sizeof(server_response), "DELETE %s successful", file_requested);
                if (remove(file_requested) == 0)
                {
                    n = sendto(sockfd, server_response, BUFSIZE, 0, (struct sockaddr *)&clientaddr, clientlen);
                }
            }

            printf("server received datagram from %s (%s)\n",
                   hostp->h_name, hostaddrp);
            printf("server received %lu/%d bytes: %s\n", strlen(buf), n, buf);
        }
        else if (strcmp(command, "ls") == 0)
        {
            /* Source: https://www.youtube.com/watch?v=0pjtn6HGPVI */
            DIR *directory;
            struct dirent *entry;
            char full_ls[BUFSIZE];
            bzero(full_ls, sizeof(full_ls));
            directory = opendir(".");
            if (directory == NULL)
            {
                return 1;
            }

            /* Keep reading directory entries */
            while ((entry = readdir(directory)) != NULL)
            {
                // printf("%s\n", entry->d_name);
                /* Source for concatenating strings */
                // https://stackoverflow.com/questions/2218290/concatenate-char-array-in-c
                strcat(full_ls, entry->d_name);
                strcat(full_ls, "\n");
            }
            n = sendto(sockfd, full_ls, BUFSIZE, 0, (struct sockaddr *)&clientaddr, clientlen);

            if (closedir(directory) == -1)
            {
                error("Error closing directory");
            }
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
