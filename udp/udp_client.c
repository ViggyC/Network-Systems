/*
 * uftp_client.c - A  UDP FTP client
 * usage: udpclient <host> <port>
 * Appended by Vignesh Chandrasekhar for PA1
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFSIZE 1024

/*Note: Datagrams are not connected to a remote host*/

/*
 * error - wrapper for perror
 */
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
  char *hostname;
  char buf[BUFSIZE];

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

  printf("This is the client\n");
  /* gethostbyname: get the server's DNS entry */
  server = gethostbyname(hostname); // server's name or IP address, does this bypass getaddrinfo()?
  if (server == NULL)
  {
    fprintf(stderr, "ERROR, no such host as %s\n", hostname);
    exit(0);
  }

  /* build the server's Internet address */
  bzero((char *)&serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;                              // internet domains
  bcopy((char *)server->h_addr,                                 // note: h_addr = h_addrlist[0]
        (char *)&serveraddr.sin_addr.s_addr, server->h_length); // need to figureout what this is doing
  serveraddr.sin_port = htons(portno);

  /* get a message from the user */
  // here is where I need to add logic for different FTP options: get, put, delete, exit, ls
  bzero(buf, BUFSIZE);
  printf("Please enter msg: ");
  fgets(buf, BUFSIZE, stdin); // get user input from standard input

  /* send the message to the server */
  serverlen = sizeof(serveraddr);
  n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen); // returns number of bytes sent
  if (n < 0)
    error("ERROR in sendto");

  /* print the server's reply */
  // this shouldnt really change, this will be handled by the server
  n = recvfrom(sockfd, buf, strlen(buf), 0, &serveraddr, &serverlen);
  if (n < 0)
    error("ERROR in recvfrom");
  printf("Echo from server: %s", buf);
  return 0;
}
