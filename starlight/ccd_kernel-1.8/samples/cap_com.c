#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

int main(void)
{
    int fd, byte = 0;
    struct termios save_term, raw_term;

    if ((fd = open("/dev/ttyS0", O_RDWR, 0)) < 0)
    {
        fprintf(stderr, "Error opening communications port.\n");
        return (-1);
    }
    if (tcgetattr(fd, &save_term) < 0)
    {
        fprintf(stderr, "Error getting port attributes.\n");
        return (-1);
    }
    raw_term = save_term;
    raw_term.c_lflag &= ~(ECHO | ICANON  | IEXTEN | ISIG);
    raw_term.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw_term.c_cflag &= ~(CSIZE | PARENB);
    raw_term.c_cflag |=  CS8;
    raw_term.c_oflag &= ~OPOST;
    raw_term.c_cc[VMIN]  = 1;
    raw_term.c_cc[VTIME] = 0;
    cfsetispeed(&raw_term, B9600);
    cfsetospeed(&raw_term, B9600);
    if (tcsetattr(fd, TCSAFLUSH, &raw_term) < 0)
    {
        fprintf(stderr, "Error setting port attributes.\n");
        return (-1);
    }
    if (tcsetattr(fd, TCSAFLUSH, &save_term) < 0)
    {
        fprintf(stderr, "Error restoring port attributes.\n");
        return (-1);
    }
    while (1)
    {
        read(fd, &byte, 1);
        printf("Read byte 0x%02X\n", byte);
    }
    return (close(fd));
}
