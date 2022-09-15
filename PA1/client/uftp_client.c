/*
 * uftp_client.c - A simple UDP client
 * usage: uftpclient <host> <port>
 * Adapted from sample udp_client
 * Author: Vignesh Chandrasekhar
 * Collaborators: Freddy Perez
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <netdb.h>

#define BUFSIZE 13000

typedef struct frame
{
    int frame_type; // 0 for ACK, 1 for data
    char data[1024];
    int sq_no;
    int ack;
} Frame;

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char **argv)
{
    int sockfd, portno, n; // socket file descriptor, portnumber for local connection to server, n for result of operations
    int serverlen;
    struct sockaddr_in serveraddr; // we cast this to struct sockaddr
    struct hostent *server;
    char *hostname;         // from user input
    char buf[BUFSIZE];      // buffer we will send to server for ftp services
    char temp_buf[BUFSIZE]; // for parsing user input
    char file_from_server[BUFSIZE];
    char *command; // One of => GET PUT DELETE ls EXIT
    char *file_requested;
    long size_of_file;
    char ACK[] = "+ACK";

    Frame frame_send;
    Frame frame_recv;
    int frame_id = 0;     // initialize
    int received_ack = 1; // flag 1 - ack received, 0 - ack not received, only for client

    /* check command line arguments */
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <hostname> <port>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    // https://stackoverflow.com/questions/13547721/udp-socket-set-timeout
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        perror("Error");
    }

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname); // server's name or IP address, does this bypass getaddrinfo()?
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* menu */
    printf("Here are your FTP services. Select one.\n");
    printf("1. get [filename]\n");
    printf("2. put [filename]\n");
    printf("3. delete [filename]\n");
    printf("4. ls\n");
    printf("5. exit\n");

    /* build the server's Internet address */
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;                              // internet domains
    bcopy((char *)server->h_addr_list[0],                         // note: h_addr = h_addrlist[0]
          (char *)&serveraddr.sin_addr.s_addr, server->h_length); // need to figureout what this is doing
    serveraddr.sin_port = htons(portno);
    // printf("Server h_name: %s\n", server->h_name);

    /* send the message to the server */
    serverlen = sizeof(serveraddr);
    int counter = 0;
    /* Allow client to keep making requests until exit */
    while (1)
    {

        bzero(buf, sizeof(buf));
        // printf("top marker\n");

        fgets(buf, BUFSIZE, stdin); // buf now stores the message to be sent to the server
        counter++;
        // printf("%d\n", counter);

        buf[strlen(buf) - 1] = '\0'; // null terminator for new line in user input

        strcpy(temp_buf, buf); // calling strcpy function
        // printf("User input: %s\n", buf);

        /* now split the users response in 'buf' by delimeter " " */

        command = strtok(temp_buf, " "); // source: https://www.youtube.com/watch?v=34DnZ2ewyZo
        command[strlen(command)] = '\0'; // first string needs null terminator
        // printf("%s\n", command);
        file_requested = strtok(NULL, " ");
        // printf("%s\n", file_requested);

        /* Dont let the client send get put or delete commands without a file */
        /* This eliminates the need to send extra messages for error correction */
        if ((strcmp(command, "get") == 0 && file_requested == NULL) || (strcmp(command, "put") == 0 && file_requested == NULL) || (strcmp(command, "delete") == 0 && file_requested == NULL))
        {
            printf("Enter a file name\n");
            continue;
        }

        /* send the parsed user input to the server */
        n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&serveraddr, serverlen); // returns number of bytes sent
        if (n < 0)
            error("ERROR in sendto");

        /* !Need to handle case where user enters command get, put, delete but no file */

        /* At this point all we do is wait for the server
            After sending the users service request, we need to receive the servers response!!!
         */

        /*Write logic to allow user to keep entering commands until user selects exit*/
        if (strcmp(command, "get") == 0)
        {
            // printf("Client wants to get file %s\n", file_requested);
            FILE *file_get;

            /* Error handling server response - if file does not exist on server side */
            bzero(buf, sizeof(buf));
            /*first get size of file in case we need to do partial receives*/
            n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen); // this call blocks!!!!
            if (n < 0)
            {
                printf("client timeout: get\n");
                continue;
            }

            if (strcmp(buf, "File does not exist") == 0)
            {
                printf("%s\n", buf);
                continue;
            }
            else
            {
                // write the contents on server_response into file_get: EVERYTHING IN UNIX IS A FILE!!!

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
            }

            // printf("size of file: %lu \n", size_of_file);
            long received = 0;
            /* source: https://stackoverflow.com/questions/7749134/reading-and-writing-a-buffer-in-binary-file */
            if (BUFSIZE > size_of_file)
            {
                bzero(buf, sizeof(buf));
                n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen); // this call blocks!!!!
                fwrite(buf, 1, n, file_get);
                printf("Received %d bytes\n", n);
                printf("Get Successful\n");
                fclose(file_get);
            }
            else
            {
                /* note: for client get, client need only keep reading in the file into buf */
                while (received < size_of_file)
                {
                    bzero(buf, sizeof(buf));
                    /* what we need to send is still more than what we can fit */

                    n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen); // this call blocks!!!!
                    if (n < 0)
                    {
                        printf("client timeout: get, packet may have been lost! \n");
                        break; // need to verify this logic
                    }
                    else
                    {
                        fwrite(buf, sizeof(char), n, file_get);
                        // printf("buf %s: \n", buf);
                        received += n;
                    }

                    /* Here send an ACK notifying the client that packet was received */
                    bzero(buf, sizeof(buf));
                    strcpy(buf, ACK);
                    n = sendto(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, serverlen);
                }
                printf("Received %lu bytes\n", received);
                printf("Get Successful\n");
                fclose(file_get);
            }
        }
        else if (strcmp(command, "put") == 0)
        {
            // printf("Client wants to PUT %s\n", file_requested);
            //  check how many bytes were sent/written

            FILE *file_send; // server file descriptor, for get, server will open the file requested by client and write its contents into a SOCK-DGRM

            int fd = access(file_requested, F_OK | R_OK);
            if (!fd)
            {
                file_send = fopen(file_requested, "rb");
            }
            else
            {
                file_send = NULL;
            }

            if (file_send == NULL)
            {
                printf("Enter an existing file\n");
                char msg[] = "Don't create this file";
                bzero(buf, sizeof(buf));
                strcpy(buf, msg);
                n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&serveraddr, serverlen);
            }
            else
            {
                /* now read file contents into buffer*/
                // we open the file and copy its contents into 'buf' and send that over to client using sendto
                // source: https://stackoverflow.com/questions/14002954/c-programming-how-to-read-the-whole-file-contents-into-a-buffer

                /* Source that helped me write a buffer into a file */
                // https://stackoverflow.com/questions/14002954/c-programming-how-to-read-the-whole-file-contents-into-a-buffer
                fseek(file_send, 0, SEEK_END);
                long fsize = ftell(file_send);
                fseek(file_send, 0, SEEK_SET);
                bzero(buf, sizeof(buf));
                sprintf(buf, "%ld", fsize);
                /* sending size of file to server first */
                n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&serveraddr, serverlen);
                /* If file is larger than buffer size */
                long sent = 0;
                bzero(buf, sizeof(buf));

                printf("File size: %lu\n", fsize);

                /* buffer can fit all of the file! */
                if (BUFSIZE > fsize)
                {
                    bzero(buf, sizeof(buf));
                    fread(buf, sizeof(char), BUFSIZE, file_send);
                    n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&serveraddr, serverlen);
                    sent += n;
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
                        if ((fsize - sent) > BUFSIZE)
                        {
                            fread(buf, sizeof(char), BUFSIZE, file_send);
                            n = sendto(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, serverlen);

                            // printf("buf: %s \n", buf);
                            if (n < 0)
                                error("ERROR in sendto");
                            sent += n;

                            /* "Lazy" sleep solution */
                            // usleep(15000);

                            /* Receive ACK from server */
                            bzero(buf, sizeof(buf));
                            n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
                        }
                        else
                        {
                            /*other wise just send whats left over, it should fit */
                            fread(buf, sizeof(char), fsize - sent, file_send);
                            n = sendto(sockfd, buf, fsize - sent, 0, (struct sockaddr *)&serveraddr, serverlen);

                            // printf("buf: %s \n", buf);
                            if (n < 0)
                                error("ERROR in sendto");
                            sent += n;
                            // usleep(15000);

                            /* Receive ACK from server */
                            bzero(buf, sizeof(buf));
                            n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
                        }
                    }
                }

                printf("Client sent %lu bytes\n", sent);
                fclose(file_send);
                n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen); // this call blocks!!!!

                if (n < 0)
                {
                    printf("client timeout in put\n");
                    continue;
                }
                else
                {
                    printf("%s\n", buf);
                }
            }
        }
        else if (strcmp(command, "delete") == 0)
        {
            bzero(buf, sizeof(buf));
            n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
            if (n < 0)
            {
                printf("client timeout in delete\n");
            }
            else
            {
                printf("%s\n", buf);
            }
        }
        else if (strcmp(command, "ls") == 0)
        {

            bzero(buf, sizeof(buf));
            n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen); // this call blocks!!!!
            // timeout
            if (n < 0)
            {
                printf("client timeout in ls\n");
            }
            else
            {
                printf("Here is a list of all the server files:\n");
                printf("%s\n", buf);
            }
        }
        else if (strcmp(command, "exit") == 0)
        {
            bzero(buf, sizeof(buf));
            n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
            if (n < 0)
            {
                printf("client timeout in exit\n");
            }
            else
            {
                printf("%s\n", buf);
                close(sockfd);
                exit(1);
            }
        }
        else
        {
            printf("Invalid input\n");
        }

        /* Old code from starter file: */
        // n = recvfrom(sockfd, buf, strlen(buf), 0, &serveraddr, &serverlen); // this call blocks!!!!
        // if (n < 0)
        //     error("ERROR in recvfrom");
        // printf("Echo from server: %s\n", buf);
    }

    return 0; // maybe not necessary here
}