
#include <math.h>
#include <vga.h>
#include <vgakeyboard.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "../ccd_msg.h"

#ifndef min
#define min(a,b)    (((a)<(b))?(a):(b))
#endif
#ifndef max
#define max(a,b)    (((a)>(b))?(a):(b))
#endif
#define FALSE                   0
#define TRUE                    ~FALSE
/*
 * IOCTL commands specific to this device.
 */
#define CCD_CTRL_CMD_WRITE_SERIAL   0x0010
#define CCD_CTRL_CMD_READ_SERIAL    0x0011
#define CCD_CTRL_CMD_SET_SERIAL     0x0012
#define CCD_CTRL_CMD_GET_SERIAL     0x0013
/*
 * String sizes.
 */
#define DEFAULT_STRING_LENGTH   68
#define DATE_STRING_LENGTH      DEFAULT_STRING_LENGTH
#define TIME_STRING_LENGTH      DEFAULT_STRING_LENGTH
#define CAMERA_STRING_LENGTH    DEFAULT_STRING_LENGTH
#define TELESCOPE_STRING_LENGTH DEFAULT_STRING_LENGTH
#define OBSERVER_STRING_LENGTH  DEFAULT_STRING_LENGTH
#define OBJECT_STRING_LENGTH    DEFAULT_STRING_LENGTH
#define LOCATION_STRING_LENGTH  DEFAULT_STRING_LENGTH
#define DIR_STRING_LENGTH       PATH_MAX
#define NAME_STRING_LENGTH      64
#define EXT_STRING_LENGTH       16
#define MAX_PROCESS_HISTORY     32
#define HISTORY_STRING_LENGTH   68
#define MAX_COMMENTS            32
#define COMMENT_STRING_LENGTH   68
/*
 * CCD device structure.
 */
struct ccd_dev
{
    char            filename[NAME_STRING_LENGTH];
    int             fd;
    unsigned int    width;
    unsigned int    height;
    unsigned int    depth;
    unsigned int    fields;
    unsigned int    dac_bits;
    unsigned int    color;
    unsigned int    caps;
    float           pixel_width;
    float           pixel_height;
    char            camera[CAMERA_STRING_LENGTH+1];
};

/***************************************************************************\
*                                                                           *
*                     Low level CCD camera control                          *
*                                                                           *
\***************************************************************************/

/*
 * Connect to CCD camera.
 */
int ccd_connect(struct ccd_dev *ccd)
{
    int           fd, msg_len, i;
    CCD_ELEM_TYPE msg[CCD_MSG_CCD_LEN/CCD_ELEM_SIZE];

    ccd->fd = 0;
    if ((fd = open(ccd->filename, O_RDWR, 0)) < 0)
        return (FALSE);
    /*
     * Request CCD parameters.
     */
    msg[CCD_MSG_HEADER_INDEX]    = CCD_MSG_HEADER;
    msg[CCD_MSG_LENGTH_LO_INDEX] = CCD_MSG_QUERY_LEN;
    msg[CCD_MSG_LENGTH_HI_INDEX] = 0;
    msg[CCD_MSG_INDEX]           = CCD_MSG_QUERY;
    write(fd, (char *)msg, CCD_MSG_QUERY_LEN);
    if ((msg_len = read(fd, (char *)msg, CCD_MSG_CCD_LEN)) != CCD_MSG_CCD_LEN)
    {
        fprintf(stderr, "CCD message length wrong: %d\n", msg_len);
        return (FALSE);
    }
    /*
     * Response from CCD query.
     */
    if (msg[CCD_MSG_INDEX] != CCD_MSG_CCD)
    {
        fprintf(stderr, "Wrong message returned from query: 0x%04X", msg[CCD_MSG_INDEX]);
        return (FALSE);
    }
    ccd->fd           = fd;
    ccd->width        = msg[CCD_CCD_WIDTH_INDEX];
    ccd->height       = msg[CCD_CCD_HEIGHT_INDEX];
    ccd->pixel_width  = (int)((msg[CCD_CCD_PIX_WIDTH_INDEX]  / 25.6) + 0.5) / 10.0;
    ccd->pixel_height = (int)((msg[CCD_CCD_PIX_HEIGHT_INDEX] / 25.6) + 0.5) / 10.0;
    ccd->fields       = msg[CCD_CCD_FIELDS_INDEX];
    ccd->depth        = msg[CCD_CCD_DEPTH_INDEX];
    ccd->dac_bits     = msg[CCD_CCD_DAC_INDEX];
    ccd->color        = msg[CCD_CCD_COLOR_INDEX];
    ccd->caps         = msg[CCD_CCD_CAPS_INDEX];
    strncpy(ccd->camera, (char *)&msg[CCD_CCD_NAME_INDEX], CCD_CCD_NAME_LEN);
    ccd->camera[CCD_CCD_NAME_LEN] = '\0';
    for (i = CCD_CCD_NAME_LEN - 1; i && (ccd->camera[i] == '\0' || ccd->camera[i] == ' '); i--)
        ccd->camera[i] = '\0'; // Strip off trailing spaces
    return (TRUE);
}
int ccd_release(struct ccd_dev *ccd)
{
    int fd;

    if ((fd = ccd->fd))
    {
        ccd->fd = 0;
        return (close(fd));
    }
    return (0);
}
/*
 * Device control.
 */
void ccd_control(struct ccd_dev *ccd, int cmd, unsigned long param)
{
    CCD_ELEM_TYPE  msg[CCD_MSG_CTRL_LEN/CCD_ELEM_SIZE];
    /*
     * Send the control command.
     */
    msg[CCD_MSG_HEADER_INDEX]    = CCD_MSG_HEADER;
    msg[CCD_MSG_LENGTH_LO_INDEX] = CCD_MSG_CTRL_LEN;
    msg[CCD_MSG_LENGTH_HI_INDEX] = 0;
    msg[CCD_MSG_INDEX]           = CCD_MSG_CTRL;
    msg[CCD_CTRL_CMD_INDEX]      = cmd;
    msg[CCD_CTRL_PARM_LO_INDEX]  = param & 0xFFFF;
    msg[CCD_CTRL_PARM_HI_INDEX]  = param >> 16;
    write(ccd->fd, (char *)msg, CCD_MSG_CTRL_LEN);
}
/*
 * Serial port I/O.
 */
void ccd_write_serial(struct ccd_dev *ccd, char *str)
{
    char ser_buf[64];
    ser_buf[0] = min(63, strlen(str));
    strcpy(&(ser_buf[1]), str);
    ccd_control(ccd, CCD_CTRL_CMD_WRITE_SERIAL, (unsigned long)ser_buf);
}
int ccd_read_serial(struct ccd_dev *ccd, char *str)
{
    static char ser_buf[65];
    ccd_control(ccd, CCD_CTRL_CMD_READ_SERIAL, (unsigned long)ser_buf);
    ser_buf[ser_buf[0] + 1] = '\0';
    strncpy(str, &(ser_buf[1]), ser_buf[0] + 1);
    return (ser_buf[0]);
}
int main(int argc, char **argv)
{
    int    c, i;
    char   string[65];
    struct ccd_dev ccd;

    strcpy(ccd.filename, "/dev/ccda");
    if (ccd_connect(&ccd))
    {
        printf("Connected to %s\n", ccd.camera);
        ccd_write_serial(&ccd, "UUU Hello!\n");
        while (1)
        {
            if (c = ccd_read_serial(&ccd, string))
            {
                ccd_write_serial(&ccd, string);
                printf("Read %d characters: %s ", c, string);
                for (i = 0; i < c; i++)
                {
                    printf("[0x%02X] ", (unsigned char)string[i]);
                }
                printf("\n");
            }
        }
        ccd_release(&ccd);
    }
    else
    {
        printf("Unable to connect to %d\n", ccd.filename);
    }
    return (0);
}
