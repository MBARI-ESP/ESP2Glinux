/*
    routing.c, manipulating routing table for PPTP Client
    Copyright (C) 2006  James Cameron <quozl@us.netrek.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "routing.h"

/*

Design discussion.

The primary task of this module is to add a host route to the PPTP
server so that the kernel continues to deliver PPTP control and data
connection packets to the server despite the new PPP interface that is
created.  The flag --no-host-route is to disable this (not yet implemented).

A secondary task may be to implement all-to-tunnel routing if the
appropriate flag is specified on the command line.  The flag
--route-all is to implement this (not yet implemented).

Revised:  brent@mbari.org July 25, 2012

The 'ip' command is not available on many memory constrained embedded systems
(It's about 250kB!)
This is an alternative implementation using the standard 'route' command instead.
*/

static char *oldIface = NULL;
static struct in_addr svrAdr, svrGateway;

void routing_init(char *ip) {
  static const char *word = " \t\n";
  char line[256];
  oldIface = NULL;
  snprintf(line, sizeof(line), "/sbin/route -n");
  FILE *p;
  if (!inet_aton(ip, &svrAdr)) {
    syslog (LOG_ERR, "routing_init() was passed an invalid IPv4 address");
    return;
  }
  p = popen(line, "r");
  if (!p) {
    syslog (LOG_ERR, "Cannot execute route utility: %s", strerror(errno));
    return;
  }
  while (fgets(line, sizeof(line), p)) {
    char *adrs, *netMasks, *gateways, *flags, *iface;
    struct in_addr adr, netMask;
    if ((adrs = strtok(line, word)) && inet_aton(adrs, &adr) && 
        (gateways= strtok(NULL, word)) && inet_aton(gateways, &svrGateway) &&
        (netMasks= strtok(NULL, word)) && inet_aton(netMasks, &netMask) &&
        (flags=strtok(NULL, word)) && strchr(flags, 'U') &&
        /* skip Metric, Ref and Use fields */
        strtok(NULL, word) && strtok(NULL, word) && strtok(NULL, word) &&
        (iface = strtok(NULL, word))) {
      if ((svrAdr.s_addr & netMask.s_addr) == (adr.s_addr & netMask.s_addr)) {
        oldIface = strdup(iface);
        goto ret;
      }
    }
  }
  syslog(LOG_ERR, "there is no active route to server %s", ip);
ret:
  pclose(p);
}

static int delRoute(const char *suffix1, const char *suffix2, int logErrs) {
/*
  command is route del <svrIP><suffix1><suffix2> 
*/
  if (oldIface) {
    char buf[200];
    char *svrIP = inet_ntoa(svrAdr);
    snprintf(buf, sizeof(buf), 
      "/sbin/route del %s%s%s", svrIP, suffix1, suffix2);
    if (system(buf) && logErrs) {
      syslog (LOG_ERR,
              "Could not delete route to %s: %s", svrIP, strerror(errno));
      return 1;
    }
  }else{
    syslog (LOG_ERR, "previous routing_init() failed");
    return -1;
  }
  return 0;
}

void routing_start() {
  if (!delRoute(" ", "2>/dev/null", 0)) {
    char buf[200];
    char *svrIP = inet_ntoa(svrAdr);
    const char *gateIP = "";
    const char *gateway = gateIP;
    if (svrGateway.s_addr) {  //preserve routing via gateway if one specified
      gateway = " gw ";
      svrIP = strdup(svrIP);
      gateIP = inet_ntoa(svrGateway);
    }
    snprintf(buf, sizeof(buf), 
      "/sbin/route add %s%s%s dev %s", svrIP, gateway, gateIP, oldIface);      
    if (system(buf)) {
      syslog (LOG_ERR,
              "Could not add route to %s: %s", svrIP, strerror(errno));
    }
    if (svrGateway.s_addr)
      free(svrIP);
  }
}

void routing_end() {
  delRoute(" dev ", oldIface, 1);
}
