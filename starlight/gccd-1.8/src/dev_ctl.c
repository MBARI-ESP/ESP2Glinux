/*
 * GCCD - Gnome CCD Camera Controller
 * Copyright (C) 2001 David Schmenk
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <stdio.h>
#include "gccd.h"

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
 * Request exposure.
 */
void ccd_expose_frame(struct ccd_exp *exposure)
{
    char           str[32];
    struct timeval now;
    CCD_ELEM_TYPE  msg[CCD_MSG_EXP_LEN/CCD_ELEM_SIZE];
    static int     image_num = 1;
    /*
     * Send the capture request.
     */
    msg[CCD_MSG_HEADER_INDEX]    = CCD_MSG_HEADER;
    msg[CCD_MSG_LENGTH_LO_INDEX] = CCD_MSG_EXP_LEN;
    msg[CCD_MSG_LENGTH_HI_INDEX] = 0;
    msg[CCD_MSG_INDEX]           = CCD_MSG_EXP;
    msg[CCD_EXP_WIDTH_INDEX]     = exposure->width;
    msg[CCD_EXP_HEIGHT_INDEX]    = exposure->height;
    msg[CCD_EXP_XOFFSET_INDEX]   = exposure->xoffset;
    msg[CCD_EXP_YOFFSET_INDEX]   = exposure->yoffset;
    msg[CCD_EXP_XBIN_INDEX]      = exposure->xbin;
    msg[CCD_EXP_YBIN_INDEX]      = exposure->ybin;
    msg[CCD_EXP_DAC_INDEX]       = exposure->dac_bits;
    msg[CCD_EXP_FLAGS_INDEX]     = exposure->flags;
    msg[CCD_EXP_MSEC_LO_INDEX]   = exposure->msec & 0xFFFF;
    msg[CCD_EXP_MSEC_HI_INDEX]   = exposure->msec >> 16;
    write(exposure->ccd->fd, (char *)msg, CCD_MSG_EXP_LEN);
    exposure->read_row = 0;
    /*
     * Set start time.
     */
    gettimeofday(&now, NULL);
    exposure->start = now.tv_sec  * 1000 + now.tv_usec  / 1000;
    /*
     * Create new image if one isn't already defined or doesn't match exposure parameters.
     */
    if (!exposure->image)
    {
        sprintf(str, "Image %d.fits", image_num++);
        exposure->image               = ccd_image_new(str);
        exposure->image->width        = exposure->width / exposure->xbin;
        exposure->image->height       = exposure->height / exposure->ybin;
        exposure->image->depth        = exposure->ccd->depth;
        strcpy(exposure->image->camera, exposure->ccd->camera);
    }
    else if ((exposure->image->width  != exposure->width / exposure->xbin)
          || (exposure->image->height != exposure->height / exposure->ybin)
          || (exposure->image->depth  != exposure->ccd->depth))
    {
        if (exposure->image->pixels)
            free(exposure->image->pixels);
        exposure->image->pixels = NULL;
        exposure->image->width        = exposure->width / exposure->xbin;
        exposure->image->height       = exposure->height / exposure->ybin;
        exposure->image->depth        = exposure->ccd->depth;
        exposure->image->pixels       = NULL;
        strcpy(exposure->image->camera, exposure->ccd->camera);
        strcat(exposure->image->camera, " CCD camera");
    }
    /*
     * Set exposure data.
     */
    if (!exposure->image->pixels)
        exposure->image->pixels   = malloc(exposure->image->width * (exposure->image->height < 2 ? 2 : exposure->image->height) * ((exposure->image->depth + 7)/8));
    exposure->image->color        = (exposure->xbin == 1 && exposure->ybin == 1) ? exposure->ccd->color : CCD_COLOR_MONOCHROME;
    exposure->image->xbin         = exposure->xbin;
    exposure->image->ybin         = exposure->ybin;
    exposure->image->exposure     = exposure->msec;
    exposure->image->pixmin       =
    exposure->image->pixmax       =
    exposure->image->datamin      = 0;
    exposure->image->datamax      = ~0UL >> (32 - exposure->dac_bits);
    exposure->image->pixel_width  = exposure->ccd->pixel_width  * exposure->xbin;
    exposure->image->pixel_height = exposure->ccd->pixel_height * exposure->ybin;
    exposure->image->time[0]      = '\0';
}
/*
 * Load exposed image one row at a time.
 */
int ccd_load_frame(struct ccd_exp *exposure)
{
    int            msg_len;
    int            sizeof_pixel = (exposure->ccd->depth + 7) / 8;
    int            row_bytes    = (exposure->width / exposure->xbin) * sizeof_pixel;
    CCD_ELEM_TYPE *msg          = (CCD_ELEM_TYPE *)exposure->image->pixels;

    if (exposure->read_row == 0)
    {
        /*
         * Get header plus first scanline.
         */
        if ((msg_len = read(exposure->ccd->fd, exposure->image->pixels, row_bytes + CCD_MSG_IMAGE_LEN)) > 0)
        {
            if (msg[CCD_MSG_INDEX] == CCD_MSG_IMAGE)
            {
                /*
                 * Validate message length.
                 */
                if ((msg[CCD_MSG_LENGTH_LO_INDEX] + (msg[CCD_MSG_LENGTH_HI_INDEX] << 16)) != (row_bytes * (exposure->height / exposure->ybin) + CCD_MSG_IMAGE_LEN))
                {
                    fprintf(stderr, "Image size discrepency! Read %d, expected %d\n", msg[CCD_MSG_LENGTH_LO_INDEX] + (msg[CCD_MSG_LENGTH_HI_INDEX] << 16), row_bytes * (exposure->height / exposure->ybin) + CCD_MSG_IMAGE_LEN);
                    exposure->read_row = 0;
                    return (0);
                }
                /*
                 * Read rest of first scanline if it didn't make it (should never happen).
                 */
                if (msg_len != row_bytes + CCD_MSG_IMAGE_LEN)
                    read(exposure->ccd->fd, &exposure->image->pixels[msg_len - CCD_MSG_IMAGE_LEN], row_bytes - msg_len - CCD_MSG_IMAGE_LEN);
                exposure->read_row = (exposure->height / exposure->ybin > 1) ? 1 : 0;
                /*
                 * Move image data down to replace header.
                 */
                memcpy(exposure->image->pixels, exposure->image->pixels + CCD_MSG_IMAGE_LEN, row_bytes);
            }
            else
                fprintf(stderr, "Error: wrong message 0x%04X\n", msg[CCD_MSG_INDEX]);
        }
        else
        {
            /*
             * Error reading pixels.  Bail out.
             */
            fprintf(stderr, "Error reading exposure:%s\n", strerror(errno));
            exposure->read_row = 0;
            return (0);
        }
    }
    else
    {
        read(exposure->ccd->fd, &exposure->image->pixels[exposure->read_row * row_bytes], row_bytes);
        if (++(exposure->read_row) == exposure->height / exposure->ybin)
            /*
             * Loaded entire frame.
             */
            exposure->read_row = 0;
    }
    return (exposure->read_row != 0);
}
/*
 * Abort current exposures.
 */
void ccd_abort_exposures(struct ccd_exp *exposure)
{
    CCD_ELEM_TYPE msg[CCD_MSG_ABORT_LEN/CCD_ELEM_SIZE];

    /*
     * Send the abort request.
     */
    msg[CCD_MSG_HEADER_INDEX]    = CCD_MSG_HEADER;
    msg[CCD_MSG_LENGTH_LO_INDEX] = CCD_MSG_ABORT_LEN;
    msg[CCD_MSG_LENGTH_HI_INDEX] = 0;
    msg[CCD_MSG_INDEX]           = CCD_MSG_ABORT;
    write(exposure->ccd->fd, (char *)msg, CCD_MSG_ABORT_LEN);
}
/***************************************************************************\
*                                                                           *
*                     Low level telescope control                           *
*                                                                           *
\***************************************************************************/
/*
 * Starlight Xpress STAR 2000 paddle control.
 */
#define CMD_DELAY(d)    ((d)+0x100)
static unsigned int  star2000_baud_rate        = B9600;
static unsigned int  star2000_init[]           = {6, CMD_DELAY(0), 0x0F, 0x00, 0xF0, 0x00, CMD_DELAY(0)};
static unsigned int  star2000_open[]           = {6, CMD_DELAY(0), 0x0F, 0x00, 0xF0, 0x00, CMD_DELAY(0)};
//static unsigned int  star2000_open[]         = {1, 0x00};
static unsigned int  star2000_close[]          = {1, 0x00};
static unsigned int  star2000_move_stop[]      = {1, 0x00};
static unsigned int  star2000_move_left[]      = {2, 0x08, 0x18};
static unsigned int  star2000_move_right[]     = {2, 0x01, 0x11};
static unsigned int  star2000_move_up[]        = {2, 0x04, 0x14};
static unsigned int  star2000_move_down[]      = {2, 0x02, 0x12};
static unsigned int  star2000_slew_stop[]      = {1, 0x00};
static unsigned int  star2000_slew_left[]      = {4, 0x08, 0x18, CMD_DELAY(100), 0x19};
static unsigned int  star2000_slew_right[]     = {4, 0x01, 0x11, CMD_DELAY(100), 0x19};
static unsigned int  star2000_slew_up[]        = {4, 0x04, 0x14, CMD_DELAY(100), 0x16};
static unsigned int  star2000_slew_down[]      = {4, 0x02, 0x12, CMD_DELAY(100), 0x16};
static unsigned int  star2000_focus_stop[]     = {0};
static unsigned int  star2000_focus_in_slow[]  = {0};
static unsigned int  star2000_focus_out_slow[] = {0};
static unsigned int  star2000_focus_in_med[]   = {0};
static unsigned int  star2000_focus_out_med[]  = {0};
static unsigned int  star2000_focus_in_fast[]  = {0};
static unsigned int  star2000_focus_out_fast[] = {0};
static unsigned int *star2000_dir[]            = {star2000_move_stop,
                                                  star2000_move_left, star2000_move_right, star2000_move_up, star2000_move_down,
                                                  star2000_slew_stop,
                                                  star2000_slew_left, star2000_slew_right, star2000_slew_up, star2000_slew_down,
                                                  star2000_focus_stop,
                                                  star2000_focus_in_slow, star2000_focus_out_slow,
                                                  star2000_focus_in_med,  star2000_focus_out_med,
                                                  star2000_focus_in_fast, star2000_focus_out_fast};
/*
 * LX200 serial inteface commands.
 */
static unsigned int  lx200_baud_rate        = B9600;
static unsigned int  lx200_init[]           = {0};
static unsigned int  lx200_open[]           = {5, '#', ':', 'R', 'G', '#'};
static unsigned int  lx200_close[]          = {7, ':', 'Q', '#', ':', 'R', 'S', '#'};
static unsigned int  lx200_move_stop[]      = {3, ':', 'Q', '#'};
static unsigned int  lx200_move_left[]      = {4, ':', 'M', 'e', '#'};
static unsigned int  lx200_move_right[]     = {4, ':', 'M', 'w', '#'};
static unsigned int  lx200_move_up[]        = {4, ':', 'M', 'n', '#'};
static unsigned int  lx200_move_down[]      = {4, ':', 'M', 's', '#'};
static unsigned int  lx200_slew_stop[]      = {7, ':', 'Q', '#', ':', 'R', 'G', '#'};
static unsigned int  lx200_slew_left[]      = {8, ':', 'R', 'C', '#', ':', 'M', 'e', '#'};
static unsigned int  lx200_slew_right[]     = {8, ':', 'R', 'C', '#', ':', 'M', 'w', '#'};
static unsigned int  lx200_slew_up[]        = {8, ':', 'R', 'C', '#', ':', 'M', 'n', '#'};
static unsigned int  lx200_slew_down[]      = {8, ':', 'R', 'C', '#', ':', 'M', 's', '#'};
static unsigned int  lx200_focus_stop[]     = {4, ':', 'F', 'Q', '#'};
static unsigned int  lx200_focus_in_slow[]  = {8, ':', 'F', 'S', '#', ':', 'F', '+', '#'};
static unsigned int  lx200_focus_out_slow[] = {8, ':', 'F', 'S', '#', ':', 'F', '-', '#'};
static unsigned int  lx200_focus_in_med[]   = {8, ':', 'F', 'M', '#', ':', 'F', '+', '#'};
static unsigned int  lx200_focus_out_med[]  = {8, ':', 'F', 'M', '#', ':', 'F', '-', '#'};
static unsigned int  lx200_focus_in_fast[]  = {8, ':', 'F', 'F', '#', ':', 'F', '+', '#'};
static unsigned int  lx200_focus_out_fast[] = {8, ':', 'F', 'F', '#', ':', 'F', '-', '#'};
static unsigned int *lx200_dir[]            = {lx200_move_stop,
                                               lx200_move_left, lx200_move_right, lx200_move_up, lx200_move_down,
                                               lx200_slew_stop,
                                               lx200_slew_left, lx200_slew_right, lx200_slew_up, lx200_slew_down,
                                               lx200_focus_stop,
                                               lx200_focus_in_slow, lx200_focus_out_slow,
                                               lx200_focus_in_med,  lx200_focus_out_med,
                                               lx200_focus_in_fast, lx200_focus_out_fast};
/*
 * Scope command strings.
 */
static unsigned int  *scope_baud_rate[] = {&star2000_baud_rate, &lx200_baud_rate, 0};
static unsigned int  *scope_init[]      = {star2000_init,        lx200_init,      0};
static unsigned int  *scope_open[]      = {star2000_open,        lx200_open,      0};
static unsigned int  *scope_close[]     = {star2000_close,       lx200_close,     0};
static unsigned int **scope_dir[]       = {star2000_dir,         lx200_dir,       0};
/*
 * Write command string to scope with delays.
 */
static void scope_write(int fd, unsigned int *cmd)
{
    int           i;
    unsigned char c;

    for (i = 1; i <= cmd[0]; i++)
    {
        if (cmd[i] >= 0x100)
        {
            tcdrain(fd);
            usleep((cmd[i] - 0x100) * 1000);
        }
        else
        {
            c = cmd[i];
            write(fd, &c, 1);
        }
    }
}
/*
 * Connect to scope.
 */
int scope_connect(struct scope_dev *scope)
{
    static int prev_iface = -1;
    struct termios raw_term;
    int i;

    if (scope->iface > SCOPE_MANUAL)
    {
        if ((scope->fd = open(scope->filename, O_RDWR, 0)) < 0)
        {
            fprintf(stderr, "Error opening telescope communications port %s.\n", scope->filename);
            return (-1);
        }
        if (tcgetattr(scope->fd, &(scope->save_term)) < 0)
        {
            perror("Error getting scope port attributes");
            return (-1);
        }
        raw_term = scope->save_term;
        raw_term.c_lflag &= ~(ECHO | ICANON  | IEXTEN | ISIG);
        raw_term.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw_term.c_cflag &= ~(CSIZE | PARENB);
        raw_term.c_cflag |=  CS8;
        raw_term.c_oflag &= ~OPOST;
        raw_term.c_cc[VMIN]  = 0;
        raw_term.c_cc[VTIME] = 0;
        cfsetispeed(&raw_term, *scope_baud_rate[scope->iface]);
        cfsetospeed(&raw_term, *scope_baud_rate[scope->iface]);
        if (tcsetattr(scope->fd, TCSAFLUSH, &raw_term) < 0)
        {
            perror("Error setting scope port attributes");
            return (-1);
        }
        if (prev_iface != scope->iface)
        {
            for (i = 1; i <= scope_init[scope->iface][0]; i++)
            {
                if (scope_init[scope->iface][i] >= 0x100)
                    scope_init[scope->iface][i] = scope->init_delay + 0x100;
            }
            scope_write(scope->fd, scope_init[scope->iface]);
            prev_iface = scope->iface;
        }
        for (i = 1; i <= scope_open[scope->iface][0]; i++)
        {
            if (scope_open[scope->iface][i] >= 0x100)
                scope_open[scope->iface][i] = scope->init_delay + 0x100;
        }
        scope_write(scope->fd, scope_open[scope->iface]);
    }
    else
        scope->fd = 0;
    return (0);
}
/*
 * Close scope connection.
 */
int scope_release(struct scope_dev *scope)
{
    int fd;

    if ((fd = scope->fd))
    {
        scope_write(fd, scope_close[scope->iface]);
        if (tcsetattr(fd, TCSAFLUSH, &(scope->save_term)) < 0)
        {
            perror("Error restoring scope port attributes");
            return (-1);
        }
        scope->fd = 0;
        return (close(fd));
    }
    return (0);
}
/*
 * Move scope.
 */
void scope_move(struct scope_dev *scope, unsigned int dir)
{
    if (scope->flags & SCOPE_SWAP_XY)
    {
        switch (dir)
        {
            case SCOPE_UP:
                dir = SCOPE_RIGHT;
                break;
            case SCOPE_DOWN:
                dir = SCOPE_LEFT;
                break;
            case SCOPE_LEFT:
                dir = SCOPE_UP;
                break;
            case SCOPE_RIGHT:
                dir = SCOPE_DOWN;
                break;
        }
    }
    switch (dir)
    {
        case SCOPE_UP:
            if (scope->flags & SCOPE_REV_DEC)
                dir = SCOPE_DOWN;
            break;
        case SCOPE_DOWN:
            if (scope->flags & SCOPE_REV_DEC)
                dir = SCOPE_UP;
            break;
        case SCOPE_LEFT:
            if (scope->flags & SCOPE_REV_RA)
                dir = SCOPE_RIGHT;
            break;
        case SCOPE_RIGHT:
            if (scope->flags & SCOPE_REV_RA)
                dir = SCOPE_LEFT;
            break;
        case SCOPE_FOCUS_IN:
        case SCOPE_FOCUS_OUT:
            if (scope->flags & SCOPE_FOCUS_FAST)
                dir += 4;
            else if (scope->flags & SCOPE_FOCUS_MED)
                dir += 2;
        case SCOPE_FOCUS_STOP:
            break;
        case SCOPE_STOP:
        default:
            dir   = SCOPE_STOP;
    }
    if (dir <= SCOPE_DOWN && scope->flags & SCOPE_SLEW)
        dir += SCOPE_DOWN + 1;
    if (scope->iface > SCOPE_MANUAL)
    {
        scope_write(scope->fd, scope_dir[scope->iface][dir]);
        tcdrain(scope->fd);
    }
}
/***************************************************************************\
*                                                                           *
*                   Low level filter wheel control                          *
*                                                                           *
\***************************************************************************/
int wheel_read(struct wheel_dev *wheel)
{
    unsigned char cmd[4];

    read(wheel->fd, cmd, 4);
    switch (cmd[1])
    {
        case 129:
            wheel->current = cmd[2];
            wheel->status  = WHEEL_IDLE;
            break;
        case 130:
            wheel->current = cmd[2] - 48;
            wheel->status  = WHEEL_IDLE;
            break;
        case 131:
            wheel->num_filters = cmd[2] - 48;
            wheel->current     = 1;
            wheel->status      = WHEEL_IDLE;
            break;
    }
    return (wheel->status == WHEEL_IDLE);
}
/*
 * Connect to scope.
 */
int wheel_connect(struct wheel_dev *wheel)
{
    struct termios raw_term;

    if ((wheel->fd = open(wheel->filename, O_RDWR, 0)) < 0)
    {
        fprintf(stderr, "Error opening filter wheel communications port %s.\n", wheel->filename);
        return (-1);
    }
    if (tcgetattr(wheel->fd, &(wheel->save_term)) < 0)
    {
        perror("Error getting filter wheel port attributes");
        return (-1);
    }
    raw_term = wheel->save_term;
    raw_term.c_lflag &= ~(ECHO | ICANON  | IEXTEN | ISIG);
    raw_term.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw_term.c_cflag &= ~(CSIZE | PARENB);
    raw_term.c_cflag |=  CS8;
    raw_term.c_oflag &= ~OPOST;
    raw_term.c_cc[VMIN]  = 0;
    raw_term.c_cc[VTIME] = 0;
    cfsetispeed(&raw_term, B9600);
    cfsetospeed(&raw_term, B9600);
    if (tcsetattr(wheel->fd, TCSAFLUSH, &raw_term) < 0)
    {
        perror("Error setting filter wheel port attributes");
        return (-1);
    }
    return (wheel_reset(wheel));
}
/*
 * Close filter wheel connection.
 */
int wheel_release(struct wheel_dev *wheel)
{
    int fd;

    if ((fd = wheel->fd))
    {
        wheel->fd = 0;
        if (tcsetattr(fd, TCSAFLUSH, &(wheel->save_term)) < 0)
        {
            perror("Error restoring filter wheel port attributes");
            return (-1);
        }
        return (close(fd));
    }
    return (0);
}
int wheel_reset(struct wheel_dev *wheel)
{
    unsigned char cmd[4];

    if (wheel->status != WHEEL_IDLE)
        return (-1);
    cmd[0] = 165;
    cmd[1] = 3;
    cmd[2] = 32;
    cmd[3] = 200;
    write(wheel->fd, cmd, 4);
    wheel->status = WHEEL_BUSY;
    return (0);
}
int wheel_query(struct wheel_dev *wheel)
{
    unsigned char cmd[4];

    if (wheel->status != WHEEL_IDLE)
        return (-1);
    cmd[0] = 165;
    cmd[1] = 2;
    cmd[2] = 32;
    cmd[3] = 199;
    write(wheel->fd, cmd, 4);
    wheel->status = WHEEL_BUSY;
    return (0);
}
int wheel_goto(struct wheel_dev *wheel, int pos)
{
    unsigned char cmd[4];

    if (wheel->status != WHEEL_IDLE)
        return (-1);
    if (pos >= 1 && pos <= wheel->num_filters)
    {
        cmd[0] = 165;
        cmd[1] = 1;
        cmd[2] = pos;
        cmd[3] = 166 + pos;
        write(wheel->fd, cmd, 4);
        wheel->status = WHEEL_BUSY;
    }
    return (0);
}
