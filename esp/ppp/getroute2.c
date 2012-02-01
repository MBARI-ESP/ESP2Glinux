/*********************  getroute2.c  *****************
*
*  Quick hack to determine the network interface providing 
*  a route to the specified destination IPv4 address
*
*  Roughly equivalent to:   ip route get
*
*  The ip command is large and missing from ESP Linux
*
*******************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

static const char *word = " \t\n";

int main(int argc, char **argv)
{
  struct in_addr dstAdr;
  char line[255];
  FILE *p = popen("/sbin/route -n", "r");
  if (!p) {
    perror("Missing route utility");
    exit(3);
  }
  if (argc < 2) {
usage:
    fprintf (stderr,
"Scan network routes, outputting the interface associated with the first\n"
"route whose Destination/Netmask matches the destination address given\n");
    exit(2);
  }  
  if (!inet_aton(argv[1], &dstAdr)) {
    fprintf (stderr,
             "Argument must be an IPv4 address of the form aa.bb.cc.dd\n");
    goto usage;
  }
  while (fgets(line, sizeof(line), p)) {
    char *adrs, *netMasks, *flags, *iface;
    struct in_addr adr, netMask;
    if ((adrs = strtok(line, word)) && inet_aton(adrs, &adr) && 
          strtok(NULL, word) &&   //skip gateway
        (netMasks= strtok(NULL, word)) && inet_aton(netMasks, &netMask) &&
        (flags=strtok(NULL, word)) && strchr(flags, 'U') &&
        strtok(NULL, word) && strtok(NULL, word) && strtok(NULL, word) &&
        (iface = strtok(NULL, word))) {
      if ((dstAdr.s_addr & netMask.s_addr) == (adr.s_addr & netMask.s_addr)) {
        puts(iface);
        exit(0);
      }
    }
  }
  exit(1);
}
