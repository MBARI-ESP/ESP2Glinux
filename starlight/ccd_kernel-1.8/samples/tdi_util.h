/***************************************************************************\
    
    TDI - Time Delay Integration Utilities
    
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

#ifndef _TDI_UTIL_H_
#define _TDI_UTIL_H_

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
#include "ccd_msg.h"

#ifndef min
#define min(a,b)    (((a)<(b))?(a):(b))
#endif
#ifndef max
#define max(a,b)    (((a)>(b))?(a):(b))
#endif
#define FALSE                   0
#define TRUE                    ~FALSE
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
 * Pixel size switch statement for all supported pixel depths.
 */
#define PIXEL_SIZE_CASE(size)           \
    switch (size)                       \
    {                                   \
        case 1:                         \
            PIXEL_LOOP(unsigned char);  \
            break;                      \
        case 2:                         \
            PIXEL_LOOP(unsigned short); \
            break;                      \
        case 4:                         \
            PIXEL_LOOP(unsigned long);  \
            break;                      \
    }


struct ccd_image
{
    int               fd;
    unsigned int      width;
    unsigned int      height;
    unsigned int      depth;
    unsigned int      datamin;
    unsigned int      datamax;
    unsigned int      pixmin;
    unsigned int      pixmax;
    unsigned char    *pixels;
    unsigned int      exposure;
    float             pixel_width;
    float             pixel_height;
    float             pixel_fov_width;
    float             pixel_fov_height;
    float             declination;
    float             right_ascension;
    char              date[DATE_STRING_LENGTH+1];
    char              time[TIME_STRING_LENGTH+1];
    char              camera[CAMERA_STRING_LENGTH+1];
    char              observer[OBSERVER_STRING_LENGTH+1];
    char              telescope[TELESCOPE_STRING_LENGTH+1];
    char              object[OBJECT_STRING_LENGTH+1];
    char              location[LOCATION_STRING_LENGTH+1];
    char              history[MAX_PROCESS_HISTORY][HISTORY_STRING_LENGTH+1];
    char              comments[MAX_COMMENTS][COMMENT_STRING_LENGTH+1];
};
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
struct ccd_exp
{
    struct ccd_dev   *ccd;
    struct ccd_image *image;
    unsigned int      xoffset;
    unsigned int      yoffset;
    unsigned int      width;
    unsigned int      height;
    unsigned int      xbin;
    unsigned int      ybin;
    unsigned int      dac_bits;
    unsigned int      flags;
    unsigned int      time_count;
    unsigned int      time_scale;
    unsigned int      msec;
    unsigned int      start;
    unsigned int      read_row;
    int               downloading;
};

int               ccd_image_write_fits_header(struct ccd_image *image);
int               ccd_image_write_fits_data_row(struct ccd_image *image);
int               ccd_image_load_fits(struct ccd_image *image, char *filename);
struct ccd_image *ccd_image_new(char *filename);
struct ccd_image *ccd_image_load(char *filename);
void              ccd_image_delete(struct ccd_image *image);
int               ccd_connect(struct ccd_dev *ccd);
int               ccd_release(struct ccd_dev *ccd);
void              ccd_expose(struct ccd_exp *exposure);
int               ccd_read_row(struct ccd_exp *exposure);
void              ccd_image_calibrate_row(struct ccd_image *raw, struct ccd_image *bias, struct ccd_image *dark, struct ccd_image *flat);

#endif // TDI_UTIL_H_
