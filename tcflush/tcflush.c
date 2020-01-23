/***  termios flush stdin and/or stdout ***/

#include <stdio.h>
#include <sys/types.h>
#include <termios.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
  if (argv[1]) {
    fprintf(stderr, 
      "flush stdout and stdin terminals -- 1/22/20 brent@mbari.org\n"
      "Flush input FIFO associated with stdin and\n"
      "      output FIFO associated with stdout\n"
      "(accepts no options)\n\n"
      "If both stdin and stdout are terminals,\n"
      "stdout is flushed, then, after a 50ms delay, stdin is flushed.\n"
    );
    return 2;
  }
  int outNotTTY = tcflush(STDOUT_FILENO, TCOFLUSH);
  if (!outNotTTY) usleep(50*1000);
  return -tcflush(STDIN_FILENO, TCIFLUSH);
}
