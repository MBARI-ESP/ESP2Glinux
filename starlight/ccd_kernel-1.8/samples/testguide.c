/***************************************************************************\
    
    Copyright (c) 2001 David Schmenk
    
    All rights reserved.
    
    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the
    "Software"), to deal in the Software without restriction, including
    without limitation the rights to use, copy, modify, merge, publish,
    distribute, and/or sell copies of the Software, and to permit persons
    to whom the Software is furnished to do so, provided that the above
    copyright notice(s) and this permission notice appear in all copies of
    the Software and that both the above copyright notice(s) and this
    permission notice appear in supporting documentation.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
    OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
    OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
    HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
    INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
    FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
    NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
    WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
    
    Except as contained in this notice, the name of a copyright holder
    shall not be used in advertising or otherwise to promote the sale, use
    or other dealings in this Software without prior written authorization
    of the copyright holder.
    
\***************************************************************************/

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
#include "ccd_msg.h"
#include "ui.h"

#define DEFAULT_CCD_NAME         "/dev/ccda"
#define DEFAULT_COM_NAME         "/dev/ttyS0"
#define FOCUS_SCALE                 4
#define GUIDE_SCALE                 10
#define FD_FIELD_EVEN               0
#define FD_FIELD_ODD                1
#define POINT_IN_RECT(px,py,x1,y1,x2,y2) ((px)>=(x1)&&(px)<(x2)&&(py)>=(y1)&&(py)<(y2))
#define max(a, b) ((a)>(b)?(a):(b))
/*
 * Scope control.
 */
#define SX_STAR2000                 0
#define SCOPE_LEFT                  8
#define SCOPE_RIGHT                 1
#define SCOPE_UP                    4
#define SCOPE_DOWN                  2
#define SCOPE_STOP                  0
#define SCOPE_GO                    1
/*
 * Starlight Xpress STAR 2000 paddle control.
 */
unsigned int  star2000_baud_rate    = B9600;
unsigned char star2000_init[]       = {0x05, 0x0D, 0x00, 0x00, 0xF0, 0x00};
unsigned char star2000_reset[]      = {0x01, 0x00};
unsigned char star2000_go_left[]    = {0x02, 0x08, 0x18};
unsigned char star2000_go_right[]   = {0x02, 0x01, 0x11};
unsigned char star2000_go_up[]      = {0x02, 0x04, 0x14};
unsigned char star2000_go_down[]    = {0x02, 0x02, 0x12};
unsigned char star2000_stop_left[]  = {0x01, 0x00};
unsigned char star2000_stop_right[] = {0x01, 0x00};
unsigned char star2000_stop_up[]    = {0x01, 0x00};
unsigned char star2000_stop_down[]  = {0x01, 0x00};
/*
 * Scope command strings.
 */
unsigned int  *scope_baud_rate[]  = {&star2000_baud_rate, 0};
unsigned char *scope_init[]       = {star2000_init, 0};
unsigned char *scope_reset[]      = {star2000_reset, 0};
unsigned char *scope_go_left[]    = {star2000_go_left, 0};
unsigned char *scope_go_right[]   = {star2000_go_right, 0};
unsigned char *scope_go_up[]      = {star2000_go_up, 0};
unsigned char *scope_go_down[]    = {star2000_go_down, 0};
unsigned char *scope_stop_left[]  = {star2000_stop_left, 0};
unsigned char *scope_stop_right[] = {star2000_stop_right, 0};
unsigned char *scope_stop_up[]    = {star2000_stop_up, 0};
unsigned char *scope_stop_down[]  = {star2000_stop_down, 0};
/*
 * Connect to scope.
 */
struct termios save_term;
int    scope_dev;
int scope_connect(char *dev_name, int dev)
{
    int scope;
    struct termios raw_term;

    if ((scope = open(dev_name, O_RDWR, 0)) < 0)
    {
        fprintf(stderr, "Error opening telescope communications port.\n");
        return (-1);
    }
    if (tcgetattr(scope, &save_term) < 0)
    {
        fprintf(stderr, "Error getting scope port attributes.\n");
        return (-1);
    }
    raw_term = save_term;
    raw_term.c_lflag &= ~(ECHO | ICANON  | IEXTEN | ISIG);
    raw_term.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw_term.c_cflag &= ~(CSIZE | PARENB);
    raw_term.c_cflag |=  CS8;
    raw_term.c_oflag &= ~OPOST;
    raw_term.c_cc[VMIN]  = 0;
    raw_term.c_cc[VTIME] = 0;
    cfsetispeed(&raw_term, *scope_baud_rate[dev]);
    cfsetospeed(&raw_term, *scope_baud_rate[dev]);
    if (tcsetattr(scope, TCSAFLUSH, &raw_term) < 0)
    {
        fprintf(stderr, "Error setting scope port attributes.\n");
        return (-1);
    }
    usleep(300); // Allow time for SX2K to power up.
    write(scope, &scope_init[dev][1], scope_init[dev][0]);
    scope_dev = dev;
    return (scope);
}
/*
 * Move scope.
 */
int scope_move(int scope, int dir, int go)
{
#if 0
    switch (dir)
    {
        case SCOPE_LEFT:
            if (go)
                write(scope, &scope_go_left[scope_dev][1], scope_go_left[scope_dev][0]);
            else
                write(scope, &scope_stop_left[scope_dev][1], scope_stop_left[scope_dev][0]);
            break;
        case SCOPE_RIGHT:
            if (go)
                write(scope, &scope_go_right[scope_dev][1], scope_go_right[scope_dev][0]);
            else
                write(scope, &scope_stop_right[scope_dev][1], scope_stop_right[scope_dev][0]);
            break;
        case SCOPE_UP:
            if (go)
                write(scope, &scope_go_up[scope_dev][1], scope_go_up[scope_dev][0]);
            else
                write(scope, &scope_stop_up[scope_dev][1], scope_stop_up[scope_dev][0]);
            break;
        case SCOPE_DOWN:
            if (go)
                write(scope, &scope_go_down[scope_dev][1], scope_go_down[scope_dev][0]);
            else
                write(scope, &scope_stop_down[scope_dev][1], scope_stop_down[scope_dev][0]);
            break;
    }
#else
    static int pressed = 0;
    char cmd[2];

    if (go)
    {
        pressed |= dir;
        cmd[0] = pressed;
        cmd[1] = pressed | 0x10;
        write(scope, cmd, 2);
    }
    else
    {
        pressed &= ~dir;
        if (!pressed)
        {
            cmd[0] = 0;
            write(scope, cmd, 1);
        }
        else
        {
            cmd[0] = pressed;
            cmd[1] = pressed | 0x10;
            write(scope, cmd, 2);
        }
    }
#endif
}
/*
 * Close scope.
 */
int scope_close(int scope)
{
    write(scope, &scope_reset[scope_dev][1], scope_reset[scope_dev][0]);
    if (tcsetattr(scope, TCSAFLUSH, &save_term) < 0)
    {
        fprintf(stderr, "Error restoring scope port attributes.\n");
        return (-1);
    }
    return (close(scope));
}
/*
 * Connect to CCD camera.
 */
int ccd_connect(char         *dev_name,
                unsigned int *ccd_width,
                unsigned int *ccd_height,
                unsigned int *ccd_fields,
                unsigned int *ccd_depth,
                unsigned int *ccd_dac_bits)
{
    int           ccd, msg_len;
    CCD_ELEM_TYPE msg[CCD_MSG_CCD_LEN/CCD_ELEM_SIZE];

    if ((ccd = open(dev_name, O_RDWR, 0)) < 0)
    {
        fprintf(stderr, "Unable to open device: %s\n", dev_name);
        return (-1);
    }
    /*
     * Request CCD parameters.
     */
    msg[CCD_MSG_HEADER_INDEX]    = CCD_MSG_HEADER;
    msg[CCD_MSG_LENGTH_LO_INDEX] = CCD_MSG_QUERY_LEN;
    msg[CCD_MSG_LENGTH_HI_INDEX] = 0;
    msg[CCD_MSG_INDEX]           = CCD_MSG_QUERY;
    write(ccd, (char *)msg, CCD_MSG_QUERY_LEN);
    if ((msg_len = read(ccd, (char *)msg, CCD_MSG_CCD_LEN)) != CCD_MSG_CCD_LEN)
    {
        fprintf(stderr, "CCD message length wrong: %d\n", msg_len);
        return (-1);
    }
    /*
     * Response from CCD query.
     */
    if (msg[CCD_MSG_INDEX] != CCD_MSG_CCD)
    {
        fprintf(stderr, "Wrong message returned from query: 0x%04X", msg[CCD_MSG_INDEX]);
        return (-1);
    }
    *ccd_width        = msg[CCD_CCD_WIDTH_INDEX];
    *ccd_height       = msg[CCD_CCD_HEIGHT_INDEX];
    *ccd_fields       = msg[CCD_CCD_FIELDS_INDEX];
    *ccd_depth        = msg[CCD_CCD_DEPTH_INDEX];
    *ccd_dac_bits     = msg[CCD_CCD_DAC_INDEX];
    return (ccd);
}
/*
 * Request exposure.
 */
void ccd_expose_frame(int          fd,
                      unsigned int xoffset,
                      unsigned int yoffset,
                      unsigned int width,
                      unsigned int height,
                      unsigned int xbin,
                      unsigned int ybin,
                      unsigned int dac_bits,
                      unsigned int flags,
                      unsigned int exposure)
{
    CCD_ELEM_TYPE msg[CCD_MSG_EXP_LEN/CCD_ELEM_SIZE];
    /*
     * Send the capture request.
     */
    msg[CCD_MSG_HEADER_INDEX]    = CCD_MSG_HEADER;
    msg[CCD_MSG_LENGTH_LO_INDEX] = CCD_MSG_EXP_LEN;
    msg[CCD_MSG_LENGTH_HI_INDEX] = 0;
    msg[CCD_MSG_INDEX]           = CCD_MSG_EXP;
    msg[CCD_EXP_WIDTH_INDEX]     = width;
    msg[CCD_EXP_HEIGHT_INDEX]    = height;
    msg[CCD_EXP_XOFFSET_INDEX]   = xoffset;
    msg[CCD_EXP_YOFFSET_INDEX]   = yoffset;
    msg[CCD_EXP_XBIN_INDEX]      = xbin;
    msg[CCD_EXP_YBIN_INDEX]      = ybin;
    msg[CCD_EXP_DAC_INDEX]       = dac_bits;
    msg[CCD_EXP_FLAGS_INDEX]     = flags;
    msg[CCD_EXP_MSEC_LO_INDEX]   = exposure & 0xFFFF;
    msg[CCD_EXP_MSEC_HI_INDEX]   = exposure >> 16;
    write(fd, (char *)msg, CCD_MSG_EXP_LEN);
}
/*
 * Load exposed image one row at a time.
 */
int ccd_load_frame(int          fd, 
                   char        *buf, 
                   unsigned int width, 
                   unsigned int height, 
                   unsigned int xbin, 
                   unsigned int ybin, 
                   unsigned int sizeof_pixel)
{
    int            msg_len;
    static int     read_row  = 0;
    int            row_bytes = (width / xbin) * sizeof_pixel;
    CCD_ELEM_TYPE *msg       = (CCD_ELEM_TYPE *)buf;

    if (!buf)
    {
        read_row = 0;
        return (1);
    }
    if (read_row == 0)
    {
        /*
         * Get header plus first scanline.
         */
        if ((msg_len = read(fd, buf, row_bytes + CCD_MSG_IMAGE_LEN)) > 0)
        {
            if (msg[CCD_MSG_INDEX] == CCD_MSG_IMAGE)
            {
                /* 
                 * Validate message length.
                 */
                if ((msg[CCD_MSG_LENGTH_LO_INDEX] + (msg[CCD_MSG_LENGTH_HI_INDEX] << 16)) != (row_bytes * (height / ybin) + CCD_MSG_IMAGE_LEN))
                {
                    fprintf(stderr, "Image size discrepency!\n");
                    read_row = 0;
                    return (0);
                }
                /*
                 * Read rest of first scanline if it didn't make it (should never happen).
                 */
                if (msg_len != row_bytes + CCD_MSG_IMAGE_LEN)
                    read(fd, &buf[msg_len - CCD_MSG_IMAGE_LEN], row_bytes - msg_len - CCD_MSG_IMAGE_LEN);
                ui_update_load(ybin * FRAME_LOAD_HEIGHT / height);
                read_row = (height / ybin > 1) ? 1 : 0;
                /*
                 * Move image data down to replace header.
                 */
                memcpy(buf, buf + CCD_MSG_IMAGE_LEN, row_bytes);
            }
        }
    }
    else 
    {
        read(fd, &buf[read_row * row_bytes], row_bytes);
        if (++read_row == height / ybin)
        {
            /*
             * Loaded entire frame.
             */
            read_row = 0;
            ui_update_load(-1);
        }
        else
            ui_update_load(read_row * ybin * FRAME_LOAD_HEIGHT / height);
    }
    return (read_row == 0);
}
/*
 * Abort current exposures.
 */
void ccd_abort_exposures(int fd)
{
    CCD_ELEM_TYPE msg[CCD_MSG_ABORT_LEN/CCD_ELEM_SIZE];
    
    /*
     * Send the abort request.
     */
    msg[CCD_MSG_HEADER_INDEX]    = CCD_MSG_HEADER;
    msg[CCD_MSG_LENGTH_LO_INDEX] = CCD_MSG_ABORT_LEN;
    msg[CCD_MSG_LENGTH_HI_INDEX] = 0;
    msg[CCD_MSG_INDEX]           = CCD_MSG_ABORT;
    write(fd, (char *)msg, CCD_MSG_ABORT_LEN);
    /*
     * Reset load image state.
     */
    ccd_load_frame(fd, NULL, 0, 0, 1, 1, 0);
}
int ccd_close(int ccd)
{
   return (close(ccd));
}
/*
 * Reverse scanline order.
 */
void flip_image_vert(char *image, int width, int height)
{
    int             i, j, tmp;
    unsigned short *top    = (unsigned short *)image;
    unsigned short *bottom = (unsigned short *)image + (height - 1) * width;
    
    for (j = 0; j < height / 2; j++)
    {
        for (i = 0; i < width; i++)
        {
            tmp       = top[i];
            top[i]    = bottom[i];
            bottom[i] = tmp;
        }
        top    += width;
        bottom -= width;
    }
}
/*
 * Reverse scanline pixel order.
 */
void flip_image_horiz(char *image, int width, int height)
{
    int             i, j, tmp;
    unsigned short *scanline = (unsigned short *)image;
    
    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width / 2; i++)
        {
            tmp                     = scanline[i];
            scanline[i]             = scanline[width - i - 1];
            scanline[width - i - 1] = tmp;
        }
        scanline += width;
    }
}
/*
 * Scale image.  Go in reverse so src == dst works.
 */
void scale_image(char *src_image, char *dst_image, int width, int height, int scale)
{
    int i, j, k, l, scale_width = width * scale;
    unsigned short *image        = (unsigned short *)src_image;
    unsigned short *scaled_image = (unsigned short *)dst_image;

    for (j = height - 1; j >= 0; j--)
        for (i = width - 1; i >= 0; i--)
        {
            int src_offset =  j * width + i;
            int dst_offset = (j * scale_width + i) * scale;
            int pix        = image[src_offset];
            for (k = 0; k < scale;  k++)
                for (l = 0; l < scale; l++)
                     scaled_image[dst_offset + scale_width * k + l] = pix;
        }
}
/*
 * Adjsut exposure amounts based on current exposure.
 */
int adjust_exp(int current, int amount)
{
    if (current < 100) /* 1 .. 100 msec */
    {
        current += amount;
        if (current < 1)
            current = 1;
        if (current > 100)
            current = 100;
    }
    else if (current < 1000) /* 100 .. 1000 msec */
    {
        current += amount * 10;
        if (current < 100)
            current = 90;
        if (current > 1000)
            current = 1000;
    }
    else if (current < 60000) /* 1 sec .. 1 min */
    {
        current += amount * 100;
        if (current < 1000)
            current = 900;
        if (current > 60000)
            current = 60000;
    }
    else if (current < 3600000) /* i min .. 1 hour */
    {
        current += amount * 6000;
        if (current < 60000)
            current = 59000;
        if (current > 3600000)
            current = 3600000;
    }
    return (current);
}
/*
 * Update guide image and exposure.
 */
void update_guide_image(char *guide_image, int guide_width, int guide_height, int *guide_exp, int flip_vert, int flip_horiz)
{
    int j, sum, median, max, guide_hist[256];
    unsigned short *guide_src = (unsigned short *)guide_image;

    /*
     * Do any post processing like flipping.
     */
    if (flip_vert)  flip_image_vert(guide_image, guide_width, guide_height);
    if (flip_horiz) flip_image_horiz(guide_image, guide_width, guide_height);
    /*
     * Scale by factor of GUIDE_SCALE and display.
     */
    scale_image(guide_image, guide_image, guide_width, guide_height, GUIDE_SCALE);
    ui_update_guide_image(guide_image, guide_width * GUIDE_SCALE, guide_height * GUIDE_SCALE);
    /*
     * Auto exposure.
     */
    for (j = 0; j < 256; j++)
        guide_hist[j] = 0;
    for (j = 0; j < guide_height * guide_width; j++)
        guide_hist[guide_src[j] >> 8]++;
    /*
     * Find median and max pixel values.
     */
    sum = median = max = 0;
    for (j = 0; j < 256; j++)
    {
        if ((sum += guide_hist[j]) < guide_height * guide_width / 2)
            median = j;
        if (guide_hist[j])
            max = j;
    }
    if (max < 4 && *guide_exp < 500)
        *guide_exp += 10;
    if (median > 1 && *guide_exp > 10)
        *guide_exp -= 10;
}
/*
 * FITS file routines.
 */
#define FITS_CARD_COUNT     36
#define FITS_CARD_SIZE      80
#define FITS_RECORD_SIZE    (FITS_CARD_COUNT*FITS_CARD_SIZE)
/*
 * Convert unsigned LE pixels to signed BE pixels.
 */
static void convert_pixels(unsigned char *src, unsigned char *dst, unsigned int sign_bit, int pixel_size, int count)
{
    unsigned int pixel;

    switch (pixel_size)
    {
        case 1:
            memcpy(dst, src, count);
            break;
        case 2:
            while (count--)
            {
                pixel = *(unsigned short *)src ^ sign_bit;
                *(unsigned short *)dst = ((pixel & 0xFF00) >> 8) | ((pixel & 0x00FF) << 8);
                src += 2;
                dst += 2;
            }
            break;
        case 4:
            while (count--)
            {
                pixel = *(unsigned long *)src ^ sign_bit;
                *(unsigned long *)dst = ((pixel & 0xFF000000) >> 24) 
                                    | ((pixel & 0x00FF0000) >> 8)
                                    | ((pixel & 0x0000FF00) << 8)
                                    | ((pixel & 0x000000FF) << 24);
                src += 4;
                dst += 4;
            }
            break;
    }
}
/*
 * Save image to FITS file.
 */
void save_fits(char *image, int width, int height, int pixel_size, int exposure)
{
    static int     fileno = 0;
    char           filename[1024];
    char           record[FITS_CARD_COUNT][FITS_CARD_SIZE];
    unsigned char *fits_pixels;
    int            i, j, k, image_size, image_pitch, fd;

    /*
     * Create file.
     */
    sprintf(filename, "image%03d.fits", fileno++);
    fd = creat(filename, 0666);
    /*
     * Fill header records.
     */
    memset(record, ' ', FITS_RECORD_SIZE);
    i = 0;
    sprintf(record[i++], "SIMPLE  = %19c /", 'T');
    sprintf(record[i++], "BITPIX  = %19d /", pixel_size*8);
    sprintf(record[i++], "NAXIS   = %19d /", 2);
    sprintf(record[i++], "NAXIS1  = %19d /",   width);
    sprintf(record[i++], "NAXIS2  = %19d /",   height);
    if (pixel_size == 2)
    {
        sprintf(record[i++], "BZERO   = %19f", 32768.0);
        sprintf(record[i++], "BSCALE  = %19f", 1.0);
    }
    if (pixel_size == 4)
    {
        sprintf(record[i++], "BZERO   = %19f", 2147483647.0);
        sprintf(record[i++], "BSCALE  = %19f", 1.0);
    }
    sprintf(record[i++], "EXPOSURE= %19f", (float)exposure / 1000.0);
    sprintf(record[i++], "CREATOR = 'Test Guide Application'");
    sprintf(record[i++], "END");
    for (k = 0; k < FITS_RECORD_SIZE; k++)
        if (((char *)record)[k] == '\0')
            ((char *)record)[k] = ' ';
    write(fd, record, FITS_RECORD_SIZE);
    /*
     * Convert and write image data.
     */
    image_pitch = width  * pixel_size;
    image_size  = height * image_pitch;
    fits_pixels = malloc(image_pitch);
    for (i = 0; i < height; i++)
    {
        convert_pixels(image + image_size - (i+1) * image_pitch, fits_pixels, 1 << (pixel_size*8-1), pixel_size, width);
        write(fd, fits_pixels, image_pitch);
    }
    free(fits_pixels);
    /*
     * Pad remaining record size with zeros and close.
     */
    memset(record, 0, FITS_RECORD_SIZE);
    if (image_size % FITS_RECORD_SIZE)
        write(fd, record, FITS_RECORD_SIZE - (image_size % FITS_RECORD_SIZE));
    close(fd);
}
/*
 * Main program.
 */
int main(int argc, char **argv)
{
    unsigned int   ccd_width, ccd_height, ccd_fields, ccd_depthbytes, ccd_dac_bits, ccd_read_row;
    unsigned int   image_width, image_height, image_xoffset, image_yoffset, image_bin, image_dac_bits, image_flags;
    unsigned int   frame_xoffset, frame_yoffset;
    unsigned int   guide_width, guide_height, guide_xoffset, guide_yoffset, guide_field, guide_exp;
    int            exposure, exposing, continuous, guiding, focusing;
    int            flip_vert, flip_horiz, contrast_stretch;
    int            i, j, scancode, scope_type;
    int            fd_keyboard, fd_mouse, fd_scope, fd_ccd, fd_field[2], fd_max;
    fd_set         event_set;
    unsigned char *ccd_image, *ccd_half_image, *guide_image;
    struct timeval now, start, stop, wakeup;
    char          *ccd_name, *scope_name, *new_key_state, old_key_state[MAX_KEY];

    /*
     * Set defaults.
     */
    ccd_name = DEFAULT_CCD_NAME;
    scope_name = DEFAULT_COM_NAME;
    /*
     * Read options.
     */
    opterr = 0;
    while ((i = getopt (argc, argv, "c:t:")) != EOF)
    {
        switch (i)
        {
            case 'c':
                ccd_name = optarg;
                break;
            case 't':
                scope_name = optarg;
                break;
            case '?':
                fprintf(stderr, "Usage: %s [-c camera_device_name] [-t telescope_device_name]\n", argv[0]);
                exit (1);
        }
    }
    /*
     * Open communications to the telescope.
     */
    scope_type = SX_STAR2000;
    if ((fd_scope = scope_connect(scope_name, scope_type)) < 0)
    {
        fprintf(stderr, "Unable to connect to %s.\n", scope_name);
        exit (-1);
    }
    /*
     * Create CCD and displayable image.
     */
    if ((fd_ccd = ccd_connect(ccd_name, &ccd_width, &ccd_height, &ccd_fields, &ccd_depthbytes, &ccd_dac_bits)) < 0)
    {
        fprintf(stderr, "Unable to connect to %s\n", ccd_name);
        exit(1);
    }
    if (ccd_fields > 1)
    {
        char field_name[32];

        /*
         * Cool, a multi-field camera like the Starlight Xpress M series.
         * Open a file descriptor to each field.
         */
        field_name[0] = '\0';
        strcpy(field_name, ccd_name);
        strcat(field_name, "1");
        if ((fd_field[FD_FIELD_ODD] = ccd_connect(field_name, &ccd_width, &ccd_height, &ccd_fields, &ccd_depthbytes, &ccd_dac_bits)) < 0)
        {
            fprintf(stderr, "Unable to connect to %s\n", field_name);
            exit(1);
        }
        field_name[0] = '\0';
        strcpy(field_name, ccd_name);
        strcat(field_name, "2");
        if ((fd_field[FD_FIELD_EVEN] = ccd_connect(field_name, &ccd_width, &ccd_height, &ccd_fields, &ccd_depthbytes, &ccd_dac_bits)) < 0)
        {
            fprintf(stderr, "Unable to connect to %s\n", field_name);
            exit(1);
        }
    }
    ccd_depthbytes   = (ccd_depthbytes + 7) / 8;
    image_xoffset    = 0;
    image_yoffset    = 0;
    image_width      = ccd_width;
    image_height     = ccd_height;
    if (image_width > FRAME_IMAGE_WIDTH)
    {
        image_width   = FRAME_IMAGE_WIDTH;
        image_xoffset = (ccd_width - FRAME_IMAGE_WIDTH) / 2;
    }
    if (image_height > FRAME_IMAGE_HEIGHT)
    {
        image_height   = FRAME_IMAGE_HEIGHT;
        image_yoffset = (ccd_height - FRAME_IMAGE_HEIGHT) / 2;
    }
    image_bin        = 1;
    image_dac_bits   = ccd_dac_bits;
    image_flags      = 0;
    frame_xoffset    = (FRAME_IMAGE_WIDTH  - image_width)  / 2; /* Center image in CCD frame */
    frame_yoffset    = (FRAME_IMAGE_HEIGHT - image_height) / 2;
    guide_field      = FD_FIELD_ODD;
    guide_width      = FRAME_GUIDE_WIDTH  / GUIDE_SCALE;
    guide_height     = FRAME_GUIDE_HEIGHT / GUIDE_SCALE;
    if (!(guide_width & 1))
        guide_width--;
    if (!(guide_height & 1))
        guide_height--;
    guide_xoffset    = ccd_width  / 2 - guide_width  / 2;
    guide_yoffset    = ccd_height / 2 - guide_height / 2;
    focusing         = 0;
    flip_vert        = 0;
    flip_horiz       = 0;
    contrast_stretch = 1;
    ccd_image        = malloc(ccd_width * ccd_height * ccd_depthbytes);
    ccd_half_image   = malloc(ccd_width * ccd_height * ccd_depthbytes);
    guide_image      = malloc(guide_width * GUIDE_SCALE * guide_height * GUIDE_SCALE * ccd_depthbytes);
    /*
     * Set up SVGA mode 640X480X8 and mouse.
     */
    if (ui_init(&fd_keyboard, &fd_mouse) < 0)
    {
        fprintf(stderr, "SVGA unavailable\n");
        exit(1);
    }
    for (i = 0; i < MAX_KEY; i++)
        old_key_state[i] = 0;
    /*
     * Starting exposure value in msec.
     */
    exposure   = 1000;
    exposing   = 0;
    continuous = 0;
    guiding    = 0;
    guide_exp  = 500;
    ui_update_expo(exposure, 0);
    /*
     * Loop until quit.
     */
    while (1)
    {
        wakeup.tv_sec  = 0;
        wakeup.tv_usec = 250000;
        FD_ZERO(&event_set);
        if (guiding)
        {
            FD_SET(fd_field[FD_FIELD_EVEN], &event_set);
            FD_SET(fd_field[FD_FIELD_ODD],  &event_set);
            fd_max = max(fd_field[FD_FIELD_EVEN], fd_field[FD_FIELD_ODD]);
        }
        else
        {
            FD_SET(fd_ccd, &event_set);
            fd_max = fd_ccd;
        }
        FD_SET(fd_keyboard, &event_set);
        fd_max = max(fd_max, fd_keyboard);
        if (fd_mouse >= 0)
        {
            FD_SET(fd_mouse, &event_set);
            fd_max = max(fd_mouse, fd_max);
        }
        select(fd_max + 1, &event_set, NULL, NULL, &wakeup);
        /*
         * Check on CCD data.
         */
        if (guiding && FD_ISSET(fd_field[guide_field], &event_set) && ccd_load_frame(fd_field[guide_field], guide_image, guide_width, guide_height, 1, 1, ccd_depthbytes))
        {
            /*
             * Download and display guide image.
             */
            update_guide_image(guide_image, guide_width, guide_height, &guide_exp, flip_vert, flip_horiz);
            /*
             * Start next guide image.
             */
            ccd_expose_frame(fd_field[guide_field], guide_xoffset, guide_yoffset, guide_width, guide_height, 1, 1, ccd_dac_bits / 2, CCD_EXP_FLAGS_NOWIPE_FRAME, guide_exp);
        }
        if ((!guiding && FD_ISSET(fd_ccd, &event_set)                 && ccd_load_frame(fd_ccd, ccd_image, image_width, image_height, image_bin, image_bin, ccd_depthbytes))
         || ( guiding && FD_ISSET(fd_field[!guide_field], &event_set) && ccd_load_frame(fd_field[!guide_field], ccd_image, image_width, image_height, image_bin, image_bin, ccd_depthbytes)))
        {
            exposing = continuous;
            /*
             * Do any post processing like flipping, merging, and focus stretch.
             */
            if (image_bin > 1) scale_image(ccd_image, ccd_image, image_width / image_bin, image_height / image_bin, image_bin);
            if (flip_vert)     flip_image_vert(ccd_image, image_width, image_height);
            if (flip_horiz)    flip_image_horiz(ccd_image, image_width, image_height);
            if (focusing)      scale_image(ccd_image, ccd_image, image_width, image_height, FOCUS_SCALE);
            if (guiding)
            {
                ccd_abort_exposures(fd_field[guide_field]);
                if (guide_field == FD_FIELD_ODD)
                {
                    /*
                     * This is the first image, save for merging. And start second half of exposure.
                     */
                    for (i = 0; i < image_width * image_height * ccd_depthbytes; i++)
                        ccd_half_image[i] = ccd_image[i];
                    exposing = 1;
                }
                else
                {
                    unsigned short *src_image   = (unsigned short *)ccd_image;
                    unsigned short *merge_image = (unsigned short *)ccd_half_image;
                    /*
                     * This is the second image, merge with the previous half.
                     */
                    for (i = 0; i < image_width * image_height; i++)
                    {
                        j = src_image[i] + merge_image[i];
                        if (j > 0xFFFF)
                            j = 0xFFFF;
                        src_image[i] = j;
                    }
                }
                guide_field = !guide_field;
            }
            /*
             * Start next frame exposure.
             */
            if (exposing)
            {
                int msec = guiding ? exposure / 2 : exposure;
                ccd_expose_frame(guiding ? fd_field[!guide_field] : fd_ccd, image_xoffset, image_yoffset, image_width, image_height, image_bin, image_bin, image_dac_bits, image_flags, msec);
                {
                    gettimeofday(&start, NULL);
                    stop.tv_usec = start.tv_usec + (msec % 1000) * 1000;
                    stop.tv_sec  = start.tv_sec  + (msec / 1000);
                    if (stop.tv_usec > 1000000)
                    {
                        stop.tv_sec++;
                        stop.tv_usec -= 1000000;
                    }
                }
            }
            else
                start.tv_sec = start.tv_usec = 0;
            /*
             * Start guide exposure.
             */
            if (guiding)
                ccd_expose_frame(fd_field[guide_field], guide_xoffset, guide_yoffset, guide_width, guide_height, 1, 1, ccd_dac_bits / 2, exposing ? CCD_EXP_FLAGS_NOWIPE_FRAME : 0, guide_exp);
            ui_update_ccd_image(ccd_image, frame_xoffset, frame_yoffset, image_width * (focusing ? FOCUS_SCALE : 1), image_height * (focusing ? FOCUS_SCALE : 1), contrast_stretch);
        }
        /*
         * Check on keyboard input.
         */
        if (FD_ISSET(fd_keyboard, &event_set) && keyboard_update())
        {
            new_key_state = keyboard_getstate();
            for (scancode = 0; scancode < MAX_KEY; scancode++)
            {
                if (old_key_state[scancode] != new_key_state[scancode])
                {
                    if ((old_key_state[scancode] = new_key_state[scancode]))
                    {
                        /*
                         * Key pressed.
                         */
                        switch (scancode)
                        {
                            case SCANCODE_SPACE: /* Expose frame */
                                if (!exposing && !focusing)
                                {
                                    exposing = 1;
                                    ccd_expose_frame(guiding ? fd_field[!guide_field] : fd_ccd, image_xoffset, image_yoffset, image_width, image_height, image_bin, image_bin, image_dac_bits, image_flags, guiding ? exposure / 2 : exposure);
                                    {
                                        int msec = guiding ? exposure / 2 : exposure;
                                        gettimeofday(&start, NULL);
                                        stop.tv_usec = start.tv_usec + (msec % 1000) * 1000;
                                        stop.tv_sec  = start.tv_sec  + (msec / 1000);
                                        if (stop.tv_usec > 1000000)
                                        {
                                            stop.tv_sec++;
                                            stop.tv_usec -= 1000000;
                                        }
                                    }
                                }
                                break;
                            case SCANCODE_C: /* Toggle continuous capture mode */
                                continuous = !continuous;
                                break;
                            case SCANCODE_B: /* Toggle bin accumulation when binning */
                                image_flags ^= CCD_EXP_FLAGS_NOBIN_ACCUM;
                                break;
                            case SCANCODE_S: /* Toggle contrast strecth */
                                contrast_stretch = !contrast_stretch;
                                if (focusing)
                                    ui_update_ccd_image(ccd_image, 0, 0, image_width * FOCUS_SCALE, image_height * FOCUS_SCALE, contrast_stretch);
                                else
                                    ui_update_ccd_image(ccd_image, frame_xoffset, frame_yoffset, image_width, image_height, contrast_stretch);
                                break;
                            case SCANCODE_V: /* Toggle vertical flip */
                                flip_vert = !flip_vert;
                                flip_image_vert(ccd_image, image_width, image_height);
                                if (focusing)
                                    ui_update_ccd_image(ccd_image, 0, 0, image_width * FOCUS_SCALE, image_height * FOCUS_SCALE, contrast_stretch);
                                else
                                    ui_update_ccd_image(ccd_image, frame_xoffset, frame_yoffset, image_width, image_height, contrast_stretch);
                                break;
                            case SCANCODE_H: /* Toggle horizontal flip */
                                flip_horiz = !flip_horiz;
                                flip_image_horiz(ccd_image, image_width, image_height);
                                if (focusing)
                                    ui_update_ccd_image(ccd_image, 0, 0, image_width * FOCUS_SCALE, image_height * FOCUS_SCALE, contrast_stretch);
                                else
                                    ui_update_ccd_image(ccd_image, frame_xoffset, frame_yoffset, image_width, image_height, contrast_stretch);
                                break;
                            case SCANCODE_G: /* Toggle guide image */
                                if (ccd_fields > 1 && !exposing)
                                {
                                    guiding = !guiding;
                                    if (guiding)
                                    {
                                        guide_field = FD_FIELD_ODD;
                                        ccd_expose_frame(fd_field[guide_field], guide_xoffset, guide_yoffset, guide_width, guide_height, 1, 1, ccd_dac_bits / 2, 0, guide_exp);
                                    }
                                    else
                                        ccd_abort_exposures(fd_field[guide_field]);

                                }
                                break;
                            case SCANCODE_F: /* Toggle focus mode */
                                focusing = !focusing;
                                if (!focusing)
                                {
                                    image_xoffset  = 0;
                                    image_yoffset  = 0;
                                    image_width    = ccd_width;
                                    image_height   = ccd_height;
                                    if (image_width > FRAME_IMAGE_WIDTH)
                                    {
                                        image_width   = FRAME_IMAGE_WIDTH;
                                        image_xoffset = (ccd_width - FRAME_IMAGE_WIDTH) / 2;
                                    }
                                    if (image_height > FRAME_IMAGE_HEIGHT)
                                    {
                                        image_height   = FRAME_IMAGE_HEIGHT;
                                        image_yoffset = (ccd_height - FRAME_IMAGE_HEIGHT) / 2;
                                    }

                                    frame_xoffset  = (FRAME_IMAGE_WIDTH  - image_width)  / 2;
                                    frame_yoffset  = (FRAME_IMAGE_HEIGHT - image_height) / 2;
                                    ccd_abort_exposures(fd_ccd);
                                    ccd_expose_frame(fd_ccd, image_xoffset, image_yoffset, image_width, image_height, image_bin, image_bin, image_dac_bits, image_flags, exposure);
                                    {
                                        int msec = guiding ? exposure / 2 : exposure;
                                        gettimeofday(&start, NULL);
                                        stop.tv_usec = start.tv_usec + (msec % 1000) * 1000;
                                        stop.tv_sec  = start.tv_sec  + (msec / 1000);
                                        if (stop.tv_usec > 1000000)
                                        {
                                            stop.tv_sec++;
                                            stop.tv_usec -= 1000000;
                                        }
                                    }
                                    continuous   = guiding       = 0;
                                    guide_field  = FD_FIELD_ODD;
                                    exposing     = 1;
                                    ccd_read_row = 0;
                                }
                                else
                                {
                                    image_width    = ccd_width  / FOCUS_SCALE;
                                    image_height   = ccd_height / FOCUS_SCALE;
                                    image_xoffset  = ccd_width  / 2 - image_width / 2;
                                    image_yoffset  = ccd_height / 2 - image_height / 2;
                                    frame_xoffset  = (FRAME_IMAGE_WIDTH  - (image_width  * FOCUS_SCALE)) / 2;
                                    frame_yoffset  = (FRAME_IMAGE_HEIGHT - (image_height * FOCUS_SCALE)) / 2;
                                    ccd_abort_exposures(fd_ccd);
                                    ccd_expose_frame(fd_ccd, image_xoffset, image_yoffset, image_width, image_height, image_bin, image_bin, image_dac_bits, image_flags, exposure);
                                    {
                                        int msec = guiding ? exposure / 2 : exposure;
                                        gettimeofday(&start, NULL);
                                        stop.tv_usec = start.tv_usec + (msec % 1000) * 1000;
                                        stop.tv_sec  = start.tv_sec  + (msec / 1000);
                                        if (stop.tv_usec > 1000000)
                                        {
                                            stop.tv_sec++;
                                            stop.tv_usec -= 1000000;
                                        }
                                    }
                                    exposing     = continuous    = 1;
                                    guide_field  = FD_FIELD_ODD;
                                    guiding      = 0;
                                    ccd_read_row = 0;
                                }
                                break;
                            case SCANCODE_F1: /* Set image bin factor */
                            case SCANCODE_F2:
                            case SCANCODE_F3:
                            case SCANCODE_F4:
                                if (exposing)
                                    ccd_abort_exposures(fd_ccd);
                                image_bin = scancode - SCANCODE_F1 + 1;
                                if (exposing)
                                {
                                    ccd_expose_frame(fd_ccd, image_xoffset, image_yoffset, image_width, image_height, image_bin, image_bin, image_dac_bits, image_flags, exposure);
                                    {
                                        int msec = guiding ? exposure / 2 : exposure;
                                        gettimeofday(&start, NULL);
                                        stop.tv_usec = start.tv_usec + (msec % 1000) * 1000;
                                        stop.tv_sec  = start.tv_sec  + (msec / 1000);
                                        if (stop.tv_usec > 1000000)
                                        {
                                            stop.tv_sec++;
                                            stop.tv_usec -= 1000000;
                                        }
                                    }
                                    ccd_read_row = 0;
                                }
                                break;
                            case SCANCODE_F5: /* Set # of DAC bits read */
                                image_dac_bits = ccd_dac_bits;
                                break;
                            case SCANCODE_F6: /* Set # of DAC bits read */
                                image_dac_bits = ccd_dac_bits * 3 / 4;
                                break;
                            case SCANCODE_F7: /* Set # of DAC bits read */
                                image_dac_bits = ccd_dac_bits / 2;
                                break;
                            case SCANCODE_F8: /* Set # of DAC bits read */
                                image_dac_bits = ccd_dac_bits / 4;
                                break;
                            case SCANCODE_F9: /* Adjust exposure down by large amount */
                                exposure = adjust_exp(exposure, -10);
                                if (!exposing)
                                    ui_update_expo(exposure, 0);
                                break;
                            case SCANCODE_F10: /* Adjust exposure down by small amount */
                                exposure = adjust_exp(exposure, -1);
                                if (!exposing)
                                    ui_update_expo(exposure, 0);
                                break;
                            case SCANCODE_F11: /* Adjust exposure up by small amount */
                                exposure = adjust_exp(exposure, 1);
                                ui_update_expo(exposure, 0);
                                break;
                            case SCANCODE_F12: /* Adjust exposure up by large amount */
                                exposure = adjust_exp(exposure, 10);
                                ui_update_expo(exposure, 0);
                                break;
                            case SCANCODE_W: /* Write FITS file of current image */
                                save_fits(ccd_image, image_width, image_height, ccd_depthbytes, exposure);
                                break;
                            case SCANCODE_CURSORLEFT: /* move scope left */
                                scope_move(fd_scope, SCOPE_LEFT, SCOPE_GO);
                                ui_update_arrows(ARROW_LEFT, 1);
                                break;
                            case SCANCODE_CURSORRIGHT: /* move scope right */
                                scope_move(fd_scope, SCOPE_RIGHT, SCOPE_GO);
                                ui_update_arrows(ARROW_RIGHT, 1);
                                break;
                            case SCANCODE_CURSORUP: /* move scope up */
                                scope_move(fd_scope, SCOPE_UP, SCOPE_GO);
                                ui_update_arrows(ARROW_UP, 1);
                                break;
                            case SCANCODE_CURSORDOWN: /* move scope down */
                                scope_move(fd_scope, SCOPE_DOWN, SCOPE_GO);
                                ui_update_arrows(ARROW_DOWN, 1);
                                break;
                            case SCANCODE_ESCAPE:
                                ui_exit();
                                scope_close(fd_scope);
                                ccd_close(fd_ccd);
                                return(0);
                        }
                    }
                    else
                    {
                        /*
                         * Key released.
                         */
                        switch (scancode)
                        {
                            case SCANCODE_CURSORLEFT: /* move scope left */
                                scope_move(fd_scope, SCOPE_LEFT, SCOPE_STOP);
                                ui_update_arrows(ARROW_LEFT, 0);
                                break;
                            case SCANCODE_CURSORRIGHT: /* move scope right */
                                scope_move(fd_scope, SCOPE_RIGHT, SCOPE_STOP);
                                ui_update_arrows(ARROW_RIGHT, 0);
                                break;
                            case SCANCODE_CURSORUP: /* move scope up */
                                scope_move(fd_scope, SCOPE_UP, SCOPE_STOP);
                                ui_update_arrows(ARROW_UP, 0);
                                break;
                            case SCANCODE_CURSORDOWN: /* move scope down */
                                scope_move(fd_scope, SCOPE_DOWN, SCOPE_STOP);
                                ui_update_arrows(ARROW_DOWN, 0);
                                break;
                        }
                    }
                }
            }
        }
        /*
         * Check on mouse input.
         */
        if (fd_mouse >= 0 && FD_ISSET(fd_mouse, &event_set) && mouse_update())
        {
            struct mouse_state mouse;

            mouse.x      = mouse_getx();
            mouse.y      = mouse_gety();
            mouse.button = mouse_getbutton();
            ui_show_hide_mouse();
            ui_update_mouse(mouse.x, mouse.y, mouse.button);
            ui_show_hide_mouse();

            if (POINT_IN_RECT(mouse.x, mouse.y, FRAME_IMAGE_LEFT, FRAME_IMAGE_TOP, FRAME_IMAGE_RIGHT, FRAME_IMAGE_BOTTOM))
            {
                /*
                 * Mouse is in image frame.
                 */
                if ((mouse.button & MOUSE_LEFTBUTTON) && !exposing && !guiding)
                {
                    unsigned short *ccd_src   = (unsigned short *)ccd_image;
                    unsigned short *guide_dst = (unsigned short *)guide_image;
                    /*
                     * Update guide star location.
                     */
                    guide_xoffset = mouse.x - FRAME_IMAGE_LEFT - frame_xoffset - guide_width / 2;
                    guide_yoffset = mouse.y - FRAME_IMAGE_TOP  - frame_yoffset - guide_width / 2;
                    /*
                     * Constrict to active pixels.
                     */
                    if (guide_xoffset < 0)
                        guide_xoffset = 0;
                    if (guide_xoffset > image_width - guide_width)
                        guide_xoffset = image_width - guide_width;
                    if (guide_yoffset < 0)
                        guide_yoffset = 0;
                    if (guide_yoffset > image_height - guide_height)
                        guide_yoffset = image_height - guide_height;
                    /*
                     * Grab pixels in current image and put them in guide frame.
                     */
                    for (j = 0; j < guide_height; j++)
                        for (i = 0; i < guide_width; i++)
                            guide_dst[j * guide_width + i] = ccd_src[(j + guide_yoffset) * image_width + i + guide_xoffset];
                    scale_image(guide_image, guide_image, guide_width, guide_height, GUIDE_SCALE);
                    ui_update_guide_image(guide_image, guide_width * GUIDE_SCALE, guide_height * GUIDE_SCALE);
                    /*
                     * Do any flipping.
                     */
                    if (flip_vert)
                        guide_yoffset = image_height - guide_yoffset - guide_height;
                    if (flip_horiz)
                        guide_xoffset = image_width  - guide_xoffset - guide_width;
                    /*
                     * Add image offsets to get to CCD coordinates.
                     */
                    guide_xoffset += image_xoffset;
                    guide_yoffset += image_yoffset;
                }
            }
            if (POINT_IN_RECT(mouse.x, mouse.y, FRAME_EXPO_LEFT, FRAME_EXPO_TOP, FRAME_EXPO_RIGHT, FRAME_EXPO_BOTTOM))
            {
                /*
                 * Mouse is in exposure frame.
                 */
                if ((mouse.button & MOUSE_LEFTBUTTON) && !exposing && !guiding)
                {
                }
            }
        }
        /*
         * Update exposure meter.
         */
        if (exposing && timerisset(&start))
        {
            int msec_now, msec_stop;

            gettimeofday(&now, NULL);
            msec_now  = now.tv_sec  * 1000 + now.tv_usec  / 1000;
            msec_stop = stop.tv_sec * 1000 + stop.tv_usec / 1000;
            ui_update_expo(exposure, msec_stop - msec_now);
        }
    }
    scope_close(fd_scope);
    ccd_close(fd_ccd);
    return (0);
}

