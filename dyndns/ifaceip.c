
#include <stdio.h>

#include <string.h> /* for strncpy */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <errno.h>


static int ifaceIP (const char *iface, struct in_addr *ip)
{
  int err = -1;   
  if (!iface || strlen(iface)>IFNAMSIZ)
    errno = EINVAL;
  else{
    struct ifreq ifr;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd >= 0) {
      /* I want to get an IPv4 IP address */
      ifr.ifr_addr.sa_family = AF_INET;
      strncpy(ifr.ifr_name, iface, IFNAMSIZ);
      if (!(err = ioctl(fd, SIOCGIFADDR, &ifr)))
        *ip = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
    }
    close(fd);
  }
  return err;
}


int main(int argc, char **argv)
{
 struct in_addr ip;
 if (ifaceIP(argv[1], &ip)) {
   perror ("failed to get IP address");
   return 1;
 }
 printf("%s at %s\n", argv[1], inet_ntoa(ip));
 return 0;
}
