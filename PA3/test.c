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

int main()
{
    char null_term[] = "hi my name \0 \0 is Vignesh";
    size_t length;
    length = strlen(null_term);
    printf("length: %lu", length);

    return 0;
}