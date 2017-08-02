#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(void)
{
    int fd;
    pid_t child;

    fd = open("/dev/cdata-misc", O_RDWR);
    child = fork();

    write(fd, "he", 2);
    write(fd, "llo", 3);
    write(fd, "llo", 3);
    write(fd, "llo", 3);
    
    close(fd);
}
