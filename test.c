#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(void)
{
    int fd;
    pid_t child;

    child = fork();

    fd = open("/dev/cdata-misc", O_RDWR);
    write(fd, "he", 2);
    write(fd, "llo", 3);
    close(fd);
}
