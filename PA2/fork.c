#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main()
{
    int a = 2;
    int b = 5;
    pid_t result = 0;
    int pid = fork();
    if (pid == 0)
    {
        result = a + b;
    }
    else if (pid != 0)
    {
        result = a * b;
    }

    printf("%d ", result);

    return 0;
}