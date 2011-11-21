/*
 *  ser2net - A program for allowing telnet connection to serial ports
 *  Copyright (C) 2001  Corey Minyard <minyard@acm.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* This code handles generating the configuration for the serial port. */

#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "devcfg.h"
#include "utils.h"

#ifdef __CYGWIN__
void cfmakeraw(struct termios *termios_p) {
    termios_p->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
    termios_p->c_oflag &= ~OPOST;
    termios_p->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
    termios_p->c_cflag &= ~(CSIZE|PARENB);
    termios_p->c_cflag |= CS8;
}
#endif

/* Initialize a serial port control structure for the first time.
   This should only be called when the port is created.  It sets the
   port to the default 9600N81. */
void
devinit(struct termios *termctl)
{
    cfmakeraw(termctl);
    cfsetospeed(termctl, B9600);
    cfsetispeed(termctl, B9600);
    termctl->c_cflag &= ~(CSTOPB);
    termctl->c_cflag &= ~(CSIZE);
    termctl->c_cflag |= CS8;
    termctl->c_cflag &= ~(PARENB);
    termctl->c_cflag &= ~(CLOCAL);
    termctl->c_cflag &= ~(HUPCL);
    termctl->c_cflag |= CREAD;
    termctl->c_cflag &= ~(CRTSCTS);
    termctl->c_iflag &= ~(IXON | IXOFF | IXANY);
    termctl->c_iflag |= IGNBRK;
}

/* Configure a serial port control structure based upon input strings
   in instr.  These strings are described in the man page for this
   program. */
int
devconfig(char *instr, dev_info_t *dinfo)
{
    char *str;
    char *pos;
    char *strtok_data;
    int  rv = 0;
    struct termios *termctl = &dinfo->termctl;

    str = strdup(instr);
    if (str == NULL) {
	return -1;
    }

    dinfo->allow_2217 = 0;
    dinfo->disablebreak = 0;
    dinfo->shortest = 0;
    dinfo->maxAge.tv_sec = 0;
    dinfo->maxAge.tv_usec = 0;
    dinfo->banner = NULL;
    dinfo->trace_read = NULL;
    dinfo->trace_write = NULL;
    dinfo->trace_both = NULL;
    pos = strtok_r(str, ", \t", &strtok_data);
    while (pos != NULL) {
	if (strcasecmp(pos, "300") == 0) {
	    cfsetospeed(termctl, B300);
	    cfsetispeed(termctl, B300);
	} else if (strcasecmp(pos, "600") == 0) {
	    cfsetospeed(termctl, B600);
	    cfsetispeed(termctl, B600);
	} else if (strcasecmp(pos, "1200") == 0) {
	    cfsetospeed(termctl, B1200);
	    cfsetispeed(termctl, B1200);
	} else if (strcasecmp(pos, "2400") == 0) {
	    cfsetospeed(termctl, B2400);
	    cfsetispeed(termctl, B2400);
	} else if (strcasecmp(pos, "4800") == 0) {
	    cfsetospeed(termctl, B4800);
	    cfsetispeed(termctl, B4800);
	} else if (strcasecmp(pos, "9600") == 0) {
	    cfsetospeed(termctl, B9600);
	    cfsetispeed(termctl, B9600);
	} else if (strcasecmp(pos, "19200") == 0) {
	    cfsetospeed(termctl, B19200);
	    cfsetispeed(termctl, B19200);
	} else if (strcasecmp(pos, "38400") == 0) {
	    cfsetospeed(termctl, B38400);
	    cfsetispeed(termctl, B38400);
	} else if (strcasecmp(pos, "57600") == 0) {
	    cfsetospeed(termctl, B57600);
	    cfsetispeed(termctl, B57600);
	} else if (strcasecmp(pos, "115200") == 0) {
	    cfsetospeed(termctl, B115200);
	    cfsetispeed(termctl, B115200);
	} else if (strcasecmp(pos, "1STOPBIT") == 0) {
	    termctl->c_cflag &= ~(CSTOPB);
	} else if (strcasecmp(pos, "2STOPBITS") == 0) {
	    termctl->c_cflag |= CSTOPB;
	} else if (strcasecmp(pos, "7DATABITS") == 0) {
	    termctl->c_cflag &= ~(CSIZE);
	    termctl->c_cflag |= CS7;
	} else if (strcasecmp(pos, "8DATABITS") == 0) {
	    termctl->c_cflag &= ~(CSIZE);
	    termctl->c_cflag |= CS8;
	} else if (strcasecmp(pos, "NONE") == 0) {
	    termctl->c_cflag &= ~(PARENB);
	} else if (strcasecmp(pos, "EVEN") == 0) {
	    termctl->c_cflag |= PARENB;
	    termctl->c_cflag &= ~(PARODD);
	} else if (strcasecmp(pos, "ODD") == 0) {
	    termctl->c_cflag |= PARENB | PARODD;
        } else if (strcasecmp(pos, "XONXOFF") == 0) {
            termctl->c_iflag |= (IXON | IXOFF | IXANY);
            termctl->c_cc[VSTART] = 17;
            termctl->c_cc[VSTOP] = 19;      
        } else if (strcasecmp(pos, "-XONXOFF") == 0) {
            termctl->c_iflag &= ~(IXON | IXOFF | IXANY);
        } else if (strcasecmp(pos, "RTSCTS") == 0) {
            termctl->c_cflag |= CRTSCTS;  
        } else if (strcasecmp(pos, "-RTSCTS") == 0) {
            termctl->c_cflag &= ~CRTSCTS;
        } else if (strcasecmp(pos, "LOCAL") == 0) {
            termctl->c_cflag |= CLOCAL;  
        } else if (strcasecmp(pos, "-LOCAL") == 0) {
            termctl->c_cflag &= ~CLOCAL;
        } else if (strcasecmp(pos, "HANGUP_WHEN_DONE") == 0) {
            termctl->c_cflag |= HUPCL;  
        } else if (strcasecmp(pos, "-HANGUP_WHEN_DONE") == 0) {
            termctl->c_cflag &= ~HUPCL;
        } else if (strcasecmp(pos, "remctl") == 0) {
	    dinfo->allow_2217 = 1;
	} else if (strcasecmp(pos, "NOBREAK") == 0) {
	    dinfo->disablebreak = 1;
	} else if (strncasecmp(pos, "tr=", 3) == 0) {
	    /* trace read, data from the port to the socket */
	    dinfo->trace_read = find_tracefile(pos + 3);
	} else if (strncasecmp(pos, "tw=", 3) == 0) {
	    /* trace write, data from the socket to the port */
	    dinfo->trace_write = find_tracefile(pos + 3);
	} else if (strncasecmp(pos, "tb=", 3) == 0) {
	    /* trace both directions. */
	    dinfo->trace_both = find_tracefile(pos + 3);
        } else if (strncasecmp(pos, "shortest=", 9) == 0) {
            char *end;
            unsigned long ms = strtoul(pos+=9, &end, 10);
            if (pos == end)
              goto cfgErr;
            dinfo->shortest = ms;
        } else if (strncasecmp(pos, "maxAge=", 7) == 0) {
            char *end;
            double maxAge = strtod(pos+=7, &end);
            if (pos == end || maxAge < 0.0 || maxAge > 1.0e9)
              goto cfgErr;
            dinfo->maxAge.tv_sec = maxAge;
            dinfo->maxAge.tv_usec = (maxAge-dinfo->maxAge.tv_sec)*1e6 - 0.5;
	} else if ((dinfo->banner = find_banner(pos))) {
	    /* It's a banner to display at startup, it's already set. */
	} else {
cfgErr:
	    rv = -1;
	    goto out;
	}

	pos = strtok_r(NULL, ", \t", &strtok_data);
    }

out:
    free(str);
    return rv;
}

static char *
baud_string(int speed)
{
    char *str;
    switch (speed) {
    case B300: str = "300"; break;
    case B1200: str = "1200"; break;
    case B2400: str = "2400"; break;
    case B4800: str = "4800"; break;
    case B9600: str = "9600"; break;
    case B19200: str = "19200"; break;
    case B38400: str = "38400"; break;
    case B57600: str = "57600"; break;
    case B115200: str = "115200"; break;
    default: str = "unknown speed";
    }
    return str;
}

void
serparm_to_str(char *str, int strlen, struct termios *termctl)
{
    speed_t speed = cfgetospeed(termctl);
    int     stopbits = termctl->c_cflag & CSTOPB;
    int     databits = termctl->c_cflag & CSIZE;
    int     parity_enabled = termctl->c_cflag & PARENB;
    int     parity = termctl->c_cflag & PARODD;
    char    *sstr;
    char    pchar, schar, dchar;

    sstr = baud_string(speed);

    if (stopbits) 
	schar = '2';
    else
	schar = '1';

    switch (databits) {
    case CS7: dchar = '7'; break;
    case CS8: dchar = '8'; break;
    default: dchar = '?';
    }

    if (parity_enabled) {
	if (parity) {
	    pchar = 'O';
	} else {
	    pchar = 'E';
	}
    } else {
	pchar = 'N';
    }

    snprintf(str, strlen, "%s %c%c%c", sstr, pchar, dchar, schar);
}

/* Send the serial port device configuration to the control port. */
void
show_devcfg(struct controller_info *cntlr, struct termios *termctl)
{
    speed_t speed = cfgetospeed(termctl);
    int     stopbits = termctl->c_cflag & CSTOPB;
    int     databits = termctl->c_cflag & CSIZE;
    int     parity_enabled = termctl->c_cflag & PARENB;
    int     parity = termctl->c_cflag & PARODD;
    int     xon = termctl->c_iflag & IXON;
    int     xoff = termctl->c_iflag & IXOFF;
    int     xany = termctl->c_iflag & IXANY;
    int     flow_rtscts = termctl->c_cflag & CRTSCTS;
    int     clocal = termctl->c_cflag & CLOCAL;
    int     hangup_when_done = termctl->c_cflag & HUPCL;
    char    *str;

    str = baud_string(speed);
    controller_outs(cntlr, str);
    controller_outs(cntlr, " ");

    if (xon && xoff && xany) {
      controller_outs(cntlr, "XONXOFF ");
    }      
    
    if (flow_rtscts) {
      controller_outs(cntlr, "RTSCTS ");
    }

    if (clocal) {
      controller_outs(cntlr, "LOCAL ");
    }

    if (hangup_when_done) {
      controller_outs(cntlr, "HANGUP_WHEN_DONE ");
    }

    if (stopbits) {
	str = "2STOPBITS ";
    } else {
	str = "1STOPBIT ";
    }
    controller_outs(cntlr, str);

    switch (databits) {
    case CS7: str = "7DATABITS "; break;
    case CS8: str = "8DATABITS "; break;
    default: str = "unknown databits ";
    }
    controller_outs(cntlr, str);

    if (parity_enabled) {
	if (parity) {
	    str = "ODD";
	} else {
	    str = "EVEN";
	}
    } else {
	str = "NONE";
    }
    controller_outs(cntlr, str);
}

int
setdevcontrol(char *instr, int fd)
{
    int rv = 0;
    char *str;
    char *pos;
    int status;
    char *strtok_data;

    str = malloc(strlen(instr) + 1);
    if (str == NULL) {
	return -1;
    }

    strcpy(str, instr);

    pos = strtok_r(str, " \t", &strtok_data);
    while (pos != NULL) {
       if (strcasecmp(pos, "RTSHI") == 0) {
           ioctl(fd, TIOCMGET, &status);
           status |= TIOCM_RTS;
           ioctl(fd, TIOCMSET, &status);
       } else if (strcasecmp(pos, "RTSLO") == 0) {
           ioctl(fd, TIOCMGET, &status);
           status &= ~TIOCM_RTS;
           ioctl(fd, TIOCMSET, &status);
       } else if (strcasecmp(pos, "DTRHI") == 0) {
           ioctl(fd, TIOCMGET, &status);
           status |= TIOCM_DTR;
           ioctl(fd, TIOCMSET, &status);
       } else if (strcasecmp(pos, "DTRLO") == 0) {
           ioctl(fd, TIOCMGET, &status);
           status &= ~TIOCM_DTR;               /* AKA drop DTR */
           ioctl(fd, TIOCMSET, &status);
	} else {
	    rv = -1;
	    goto out;
	}

	pos = strtok_r(NULL, " \t", &strtok_data);
    }

out:
    free(str);
    return rv;
}

void
show_devcontrol(struct controller_info *cntlr, int fd)
{
    char *str;
    int  status;

    ioctl(fd, TIOCMGET, &status);

    if (status & TIOCM_RTS) {
	str = "RTSHI ";
    } else {
	str = "RTSLO ";
    }
    controller_outs(cntlr, str);

    if (status & TIOCM_DTR) {
	str = "DTRHI ";
    } else {
	str = "DTRLO ";
    }
    controller_outs(cntlr, str);
}
