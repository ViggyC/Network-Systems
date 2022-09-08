/*
 * uftp_client.c - A simple UDP client
 * usage: uftpclient <host> <port>
 * Adapted from sample udp_client
 * Author: Vignesh Chandrasekhar
 * Collaborators:
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFSIZE 1024

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
    char server_response[BUFSIZE];
    char error_message[BUFSIZE]; // the server might send an error message which we need to recoginze
    char file_buffer[BUFSIZE];   // file buffer datagram to send to server for PUT
    long size_of_file;

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
    bcopy((char *)server->h_addr,                                 // note: h_addr = h_addrlist[0]
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
        printf("User input: %s\n", buf);

        /* now split the users response in 'buf' by delimeter " " */

        command = strtok(temp_buf, " "); // source: https://www.youtube.com/watch?v=34DnZ2ewyZo
        command[strlen(command)] = '\0'; // first string needs null terminator
        // printf("%s\n", command);
        file_requested = strtok(NULL, " ");
        // printf("%s\n", file_requested);

        /* send the parsed user input to the server */
        n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&serveraddr, serverlen); // returns number of bytes sent
        if (n < 0)
            error("ERROR in sendto");

        /* !Need to handle case where user enters command get, put, delete but no file */

        /* At this point all we do is wait for the server
            After sending the users service request, we need to receive the servers response!!!
         */
        // bzero(server_response, sizeof(server_response));
        // bzero(error_message, sizeof(server_response)); // this could be the same server_response as above but just testing right now
        bzero(file_buffer, sizeof(file_buffer));

        /*Write logic to allow user to keep entering commands until user selects exit*/
        if (strcmp(command, "get") == 0)
        {
            // printf("Client wants to get file %s\n", file_requested);

            if (file_requested == NULL)
            {
                printf("Invalid request, please enter a file\n");
                continue;
            }

            FILE *file_get;
            // write the contents on server_response into file_get: EVERYTHING IN UNIX IS A FILE!!!
            file_get = fopen(file_requested, "w");
            bzero(buf, sizeof(buf));

            /*first get size of file in case we need to do partial receives*/
            n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen); // this call blocks!!!!
            char *p;
            size_of_file = strtol(buf, &p, 10);
            // printf("size of file: %lu \n", size_of_file);
            long received = 0;
            /* source: https://stackoverflow.com/questions/7749134/reading-and-writing-a-buffer-in-binary-file */
            if (BUFSIZE > size_of_file)
            {
                bzero(buf, sizeof(buf));
                n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen); // this call blocks!!!!
                fwrite(buf, 1, n, file_get);
                printf("Get Successful\n");
                fclose(file_get);
            }
            else
            {
                while (received < size_of_file)
                {
                    bzero(buf, sizeof(buf));
                    /* what we need to send is still more than what we can fit */
                    if (size_of_file - received > BUFSIZE)
                    {
                        n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen); // this call blocks!!!!
                        fwrite(buf, 1, n, file_get);
                        // printf("buf %s: \n", buf);
                        if (n < 0)
                            error("ERROR in sendto");
                        received += n;
                    }
                    else
                    {
                        /*other wise just send whats left over, it should fit */
                        n = recvfrom(sockfd, buf, size_of_file - received, 0, (struct sockaddr *)&serveraddr, &serverlen); // this call blocks!!!!
                        fwrite(buf, 1, n, file_get);

                        // printf("buf: %s \n", buf);

                        if (n < 0)
                            error("ERROR in sendto");
                        received += n;
                    }
                }
                fclose(file_get);
            }
        }
        else if (strcmp(command, "put") == 0)
        {
            // printf("Client wants to PUT %s\n", file_requested);
            // check how many bytes were sent/written
            if (file_requested == NULL)
            {
                printf("Invalid request, please enter a file\n");
                continue;
            }

            FILE *file_send; // server file descriptor, for get, server will open the file requested by client and write its contents into a SOCK-DGRM

            int fd = access(file_requested, F_OK | R_OK);
            if (!fd)
            {
                file_send = fopen(file_requested, "r");
            }
            else
            {
                file_send = NULL;
            }

            /* now read file contents into buffer*/
            // we open the file and copy its contents into 'file_buffer' and send that over to client using sendto
            // source: https://stackoverflow.com/questions/14002954/c-programming-how-to-read-the-whole-file-contents-into-a-buffer
            file_send = fopen(file_requested, "r"); /* Opening a file in w mode will delete its contents!*/
            if (file_send == NULL)
            {
                printf("Invalid file\n");
            }
            else
            {
                /* Source that helped me write a buffer into a file */
                // https://stackoverflow.com/questions/14002954/c-programming-how-to-read-the-whole-file-contents-into-a-buffer
                fseek(file_send, 0, SEEK_END);
                long fsize = ftell(file_send);
                fseek(file_send, 0, SEEK_SET);
                int bytes_sent = fread(file_buffer, sizeof(char), BUFSIZE, file_send);
                printf("Contents of client file: %s\n", file_buffer);

                n = sendto(sockfd, file_buffer, bytes_sent, 0, (struct sockaddr *)&serveraddr, serverlen);
                if (n < 0)
                    error("ERROR in sendto");
                printf("Client sent %d bytes\n", bytes_sent);
                fclose(file_send);
            }
        }
        else if (strcmp(command, "delete") == 0)
        {
            bzero(buf, sizeof(buf));
            n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
            if (n < 0)
                error("ERROR in recvfrom");
            printf("%s\n", buf);
        }
        else if (strcmp(command, "ls") == 0)
        {
            bzero(buf, sizeof(buf));
            printf("Here is a list of all the server files:\n");
            n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen); // this call blocks!!!!
            printf("%s\n", buf);
        }
        else if (strcmp(command, "exit") == 0)
        {
            bzero(buf, sizeof(buf));
            n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
            if (n < 0)
                error("ERROR in recvfrom");
            printf("%s\n", buf);
            close(sockfd);
            exit(1);
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