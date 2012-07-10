/***************************************************************************\

    Copyright (c) 2001, 2002 David Schmenk

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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))
#include <linux/slab.h>
#else
#include <linux/malloc.h>
#endif
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <asm/uaccess.h>
#include "ccd.h"

EXPORT_SYMBOL (ccd_register_device);
EXPORT_SYMBOL (ccd_unregister_device);

#define min_ccd(a,b)                (((a)<(b))?(a):(b))
#define max_ccd(a,b)                (((a)>(b))?(a):(b))
#define MODE(d)                     (MINOR(d)&0x80)
#define FIELD(d)                    ((MINOR(d)&0x30)>>4)
#define NUM(d)                      (MINOR(d)&0x0F)
#define MSEC2JIFFY(m)               ((m)/((int)(1000/HZ)))
#define MODE_BINARY                 0x00
#define MODE_TEXT                   0x80
#define MAX_CCDS                    2
#define MAX_FIELDS                  2
#define CCD_START_FRAME_LOAD        -1
#define CCD_END_FRAME_LOAD          1000000 /* Make sure no CCD has this many rows */
#define MAX_IN_BUF_SIZE             1024
/*
 * Tokens.
 */
#define CCD_QUERY_TOKEN             (CCD_MSG_QUERY)
#define CCD_ERRORNO_TOKEN           (CCD_MSG_ERRORNO)
#define CCD_EXP_TOKEN               (CCD_MSG_EXP)
#define CCD_EXP_WIDTH_TOKEN         (CCD_EXP_TOKEN+1)
#define CCD_EXP_HEIGHT_TOKEN        (CCD_EXP_TOKEN+2)
#define CCD_EXP_XOFFSET_TOKEN       (CCD_EXP_TOKEN+3)
#define CCD_EXP_YOFFSET_TOKEN       (CCD_EXP_TOKEN+4)
#define CCD_EXP_XBIN_TOKEN          (CCD_EXP_TOKEN+5)
#define CCD_EXP_YBIN_TOKEN          (CCD_EXP_TOKEN+6)
#define CCD_EXP_DAC_TOKEN           (CCD_EXP_TOKEN+7)
#define CCD_EXP_FLAGS_TOKEN         (CCD_EXP_TOKEN+8)
#define CCD_EXP_MSEC_TOKEN          (CCD_EXP_TOKEN+9)
#define CCD_ABORT_TOKEN             (CCD_MSG_ABORT)
#define CCD_CTRL_TOKEN              (CCD_MSG_CTRL)
#define CCD_CTRL_CMD_TOKEN          (CCD_CTRL_TOKEN+1)
#define CCD_CTRL_PARM_TOKEN         (CCD_CTRL_TOKEN+2)
#define CCD_CCD_TOKEN               (CCD_MSG_CCD)
#define CCD_CCD_NAME_TOKEN          (CCD_MSG_CCD+1)
#define CCD_CCD_WIDTH_TOKEN         (CCD_MSG_CCD+2)
#define CCD_CCD_HEIGHT_TOKEN        (CCD_MSG_CCD+3)
#define CCD_CCD_FIELDS_TOKEN        (CCD_MSG_CCD+4)
#define CCD_CCD_DEPTH_TOKEN         (CCD_MSG_CCD+5)
#define CCD_CCD_DAC_TOKEN           (CCD_MSG_CCD+6)
#define CCD_CCD_CAP_TOKEN           (CCD_MSG_CCD+7)
#define CCD_IMAGE_TOKEN             (CCD_MSG_IMAGE)
#define CCD_IMAGE_WIDTH_TOKEN       (CCD_IMAGE_TOKEN+1)
#define CCD_IMAGE_HEIGHT_TOKEN      (CCD_IMAGE_TOKEN+2)
#define CCD_NUM_TOKEN               0xFE01
#define CCD_UNKNOWN_TOKEN           0xFFFF
/*
 * Predeclaration.
 */
struct ccd_exposure;
struct ccd_device;
/*
 * Per client instance data structure.
 */
struct ccd_client
{
    int                  opened;
    unsigned int         exposure_flags;
    unsigned int         in_buf_len;
    unsigned int         out_buf_len;
    unsigned int         out_buf_size;
    unsigned char       *in_buf;
    unsigned char       *out_buf;
    CCD_ELEM_TYPE        parse_msg[CCD_MSG_EXP_LEN/CCD_ELEM_SIZE]; /* Capture is currently the largest client message */
    unsigned int         parse_token;
    unsigned int         parse_attrib;
    struct ccd_device   *device;
    struct ccd_exposure *exposure_list;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,17))
    wait_queue_head_t    read_wait;
#else
    struct wait_queue   *read_wait;
#endif
};
/*
 * Exposure request.
 */
struct ccd_exposure
{
    unsigned int         msec;
    unsigned int         begin;
    unsigned int         end;
    unsigned int         width;
    unsigned int         height;
    unsigned int         xoffset;
    unsigned int         yoffset;
    unsigned int         xbin;
    unsigned int         ybin;
    unsigned int         dac_bits;
    unsigned int         flags;
    unsigned int         complete;
    struct ccd_device   *device;
    struct ccd_client   *client;
    struct ccd_exposure *next;
    struct ccd_exposure *client_next;
};
/*
 * Per device instance data structure.
 */
struct ccd_device
{
    int          registered;
    int          opened;
    char         id_string[CCD_CCD_NAME_LEN + 1];
    unsigned int width;
    unsigned int height;
    unsigned int pixel_width;
    unsigned int pixel_height;
    unsigned int image_fields;
    unsigned int image_depth;
    unsigned int dac_bits;
    unsigned int color_format;
    unsigned int flag_caps;
    unsigned int sizeof_pixel;
    int          current_read_row;
    /*
     * Exposure lists.
     */
    struct ccd_exposure *exposure_list;
    struct timer_list    exposure_timer;
    /*
     * Function pointers.
     */
    void          *param;
    int          (*open)(void *);
    int          (*control)(void *, unsigned short, unsigned long);
    int          (*close)(void *);
    int          (*read_row)(void *, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned char *);
    void         (*begin_read)(void *, unsigned int, unsigned int);
    void         (*end_read)(void *, unsigned int);
    void         (*latch_frame)(void *, unsigned int);
    void         (*new_frame)(void *, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);
    void         (*temp_control)(void *, unsigned short *, int *);
};
/*
 * Global data.
 */
struct ccd_device ccd_devices[MAX_CCDS];        /* Registered by mini-driver                                */
struct ccd_client ccd_clients[MAX_CCDS][3];     /* One client for each field, plus an all fields client.    */
int               ccd_major    = 127;           /* Default major #                                          */
char              ccd_string[] = "ccd";

/***************************************************************************\
*                                                                           *
*                     CCD text output helper routines.                      *
*                                                                           *
\***************************************************************************/

/*
 * Write string representation of value into buffer.
 */
int put_int(char *buf, unsigned int value)
{
    char s[10];
    int  l, i = 0;
    do {s[i++] = value % 10 + '0';} while (value /= 10);
    l = i;
    while (i--) *buf++ = s[i];
    return (l);
}
/*
 * Write string representation of attribute/value pair into buffer.
 */
int put_attrib(char *buf, char *attrib, unsigned int value)
{
    int slen = strlen(attrib);
    buf[0] = ' ';
    memcpy(buf + 1, attrib, slen++);
    buf[slen++] = '=';
    return (slen + put_int(buf + slen, value));
}

/***************************************************************************\
*                                                                           *
*                        CCD exposure management routines.                  *
*                                                                           *
\***************************************************************************/

/*
 * Start a new exposure.
 */
void start_exposure(struct ccd_exposure *exposure)
{
    /*
     * Do any processing and set exposure times.
     */
    exposure->device->new_frame(exposure->device->param, exposure->xoffset, exposure->yoffset, exposure->width, exposure->height, exposure->xbin, exposure->ybin, exposure->dac_bits, exposure->msec, exposure->flags);
    exposure->begin                           = jiffies;
    exposure->end                             = exposure->begin + MSEC2JIFFY(exposure->msec);
    exposure->device->exposure_timer.expires  = exposure->end;
    exposure->device->exposure_timer.data     = (unsigned long)exposure;
    add_timer(&(exposure->device->exposure_timer));
}
/*
 * Complete exposure and prepare for reading.
 */
void complete_exposure(unsigned long ptr)
{
    struct ccd_exposure *exposure = (struct ccd_exposure *)ptr;

    if (exposure && !exposure->complete && exposure->device && exposure->device->exposure_list && (exposure->device->exposure_list == exposure))
    {
        /*
         * Latch the integrated frame.
         */
        exposure->device->latch_frame(exposure->device->param, exposure->flags);
        exposure->device->current_read_row = CCD_START_FRAME_LOAD;
        /*
         * Wake up any waiting process.
         */
        exposure->complete = 1;
        wake_up_interruptible(&(exposure->client->read_wait));
    }
}
/*
 * Update exposure times following passed in exposure.
 */
void update_exposures(struct ccd_exposure *exposure)
{
    while (exposure->next)
    {
        exposure->next->begin = exposure->end;
        exposure              = exposure->next;
        exposure->end         = exposure->begin + MSEC2JIFFY(exposure->msec);
    }
}
/*
 * Delete exposure.
 */
void del_exposure(struct ccd_exposure *exposure)
{
    struct ccd_exposure *prev_exposure, **exposure_list = &(exposure->device->exposure_list);

    if (*exposure_list == exposure)
    {
        if (!exposure->complete)
            /*
             * Kill the timer.
             */
            del_timer(&(exposure->device->exposure_timer));
        /*
         * Remove current exposure.
         */
        if ((*exposure_list = exposure->next))
        {
            if (exposure->end <= (*exposure_list)->begin)
            {
                /*
                 * Start next exposure.  Need to update the begin end times due to time spent in I/O.
                 */
                update_exposures(*exposure_list);
                start_exposure(*exposure_list);
            }
            else
            {
                /*
                 * Set timer for next exposure. Note - may have missed it due to download time
                 * of previous frame.
                 */
                if ((*exposure_list)->end <= jiffies)
                    complete_exposure((unsigned long)*exposure_list);
                else
                {
                    (*exposure_list)->device->exposure_timer.expires = (*exposure_list)->end;
                    (*exposure_list)->device->exposure_timer.data    = (unsigned long)*exposure_list;
                    add_timer(&((*exposure_list)->device->exposure_timer));
                }
            }
        }
    }
    else
    {
        /*
         * Remove from list and update remaining times.
         */
        for (prev_exposure = *exposure_list; prev_exposure && prev_exposure->next != exposure; prev_exposure = prev_exposure->next);
        if (prev_exposure)
        {
            prev_exposure->next = exposure->next;
            update_exposures(prev_exposure);
        }
    }
}
/*
 * Read exposed pixels from CCD.
 */
int read_exposure(struct ccd_exposure *exposure, char *buf, unsigned int count)
{
    struct ccd_exposure *prev_exposure;
    struct ccd_device   *dev       = exposure->device;
    struct ccd_client   *client    = exposure->client;
    int                  row_bytes = (exposure->width / exposure->xbin) * dev->sizeof_pixel;
    int                  rcount    = 0;

    /*
     * Check for begin-of-frame processing.
     */
    if (dev->current_read_row == CCD_START_FRAME_LOAD)
    {
        dev->begin_read(dev->param, exposure->yoffset, exposure->flags);
        dev->current_read_row = 0;
    }
    /*
     * Read out as many scanlines as there is room in the buffer.
     */
    while ((count >= row_bytes) && (dev->current_read_row <= (exposure->height - exposure->ybin)))
    {
        dev->read_row(dev->param, exposure->xoffset, dev->current_read_row + exposure->yoffset, exposure->width, exposure->xbin, exposure->ybin, exposure->dac_bits, exposure->flags, buf);
        dev->current_read_row += exposure->ybin;
        buf                   += row_bytes;
        rcount                += row_bytes;
        count                 -= row_bytes;
    }
    /*
     * Check for end-of-frame processing.
     */
    if (dev->current_read_row > (exposure->height - exposure->ybin))
    {
        dev->end_read(dev->param, exposure->flags);
        /*
         * Flag this frame load as complete.
         */
        dev->current_read_row = CCD_END_FRAME_LOAD;
        /*
         * Remove the exposure record from all the lists and free it up.
         */
        del_exposure(exposure);
        if (exposure == client->exposure_list)
            client->exposure_list = exposure->client_next;
        else
        {
            for (prev_exposure = client->exposure_list; prev_exposure && prev_exposure->client_next != exposure; prev_exposure = prev_exposure->client_next);
            if (prev_exposure)
                prev_exposure->client_next = exposure->client_next;
        }
        kfree(exposure);
    }
    return (rcount);
}
/*
 * Put binary exposure image into buffer.
 */
int read_binary_exposure(struct ccd_exposure *exposure, char *buf, unsigned int count)
{
    CCD_ELEM_TYPE       *image_buf;
    size_t               image_count;
    struct ccd_device   *dev       = exposure->device;
    struct ccd_client   *client    = exposure->client;
    int                  row_bytes = (exposure->width / exposure->xbin) * dev->sizeof_pixel;
    int                  rcount    = 0;

    /*
     * Dump image data into buffer with enough room for a full scanline (plus initial message header).
     */
    if (count >= row_bytes + (dev->current_read_row == CCD_START_FRAME_LOAD ? CCD_MSG_IMAGE_LEN : 0))
    {
        image_buf   = (CCD_ELEM_TYPE *)buf;
        image_count = count;
    }
    else
    {
        image_buf   = (CCD_ELEM_TYPE *)client->out_buf;
        image_count = row_bytes + (dev->current_read_row == CCD_START_FRAME_LOAD ? CCD_MSG_IMAGE_LEN : 0);
    }
    /*
     * Send command header if this is the first data sent.
     */
    if (dev->current_read_row == CCD_START_FRAME_LOAD)
    {
        unsigned int image_width  = (exposure->width  / exposure->xbin);
        unsigned int image_height = (exposure->height / exposure->ybin);
        unsigned int image_size   = image_width * image_height
                                  * dev->sizeof_pixel
                                  + CCD_MSG_IMAGE_LEN;
        image_buf[CCD_MSG_HEADER_INDEX]    = CCD_MSG_HEADER;
        image_buf[CCD_MSG_LENGTH_LO_INDEX] = image_size & 0xFFFF;
        image_buf[CCD_MSG_LENGTH_HI_INDEX] = image_size >> 16;
        image_buf[CCD_MSG_INDEX]           = CCD_MSG_IMAGE;
        rcount                             = CCD_MSG_IMAGE_LEN;
    }
    /*
     * Read exposure.
     */
    rcount += read_exposure(exposure, (unsigned char *)image_buf + rcount, image_count - rcount);
    /*
     * Check for sub-row_bytes user buffer size.
     */
    if (image_buf != (CCD_ELEM_TYPE *)buf)
    {
        /*
         * Copy as much into user buffer as will fit.
         */
        client->out_buf_len = rcount - count;
        memcpy(buf, client->out_buf, count);
        memcpy(client->out_buf, client->out_buf + count, client->out_buf_len);
        rcount = count;
    }
    return (rcount);
}
/*
 * Put text exposure image into buffer.
 */
int read_text_exposure(struct ccd_exposure *exposure, char *buf, unsigned int count)
{
    int                  rcount;
    char                *image_buf;
    size_t               image_count;
    struct ccd_device   *dev       = exposure->device;
    struct ccd_client   *client    = exposure->client;
    int                  row_bytes = (exposure->width / exposure->xbin) * dev->sizeof_pixel;

    /*
     * Dump single scanline of image data at end of out_buf, then translate into text.
     */
    image_buf   = client->out_buf + client->out_buf_size - row_bytes - dev->sizeof_pixel;
    image_count = row_bytes;
    /*
     * Send command header if this is the first data sent.
     */
    if (dev->current_read_row == CCD_START_FRAME_LOAD)
    {
        unsigned int image_width  = (exposure->width  / exposure->xbin);
        unsigned int image_height = (exposure->height / exposure->ybin);

        client->out_buf[client->out_buf_len++] = '<';
        memcpy(client->out_buf + client->out_buf_len, CCD_IMAGE_STRING, strlen(CCD_IMAGE_STRING));
        client->out_buf_len += strlen(CCD_IMAGE_STRING);
        client->out_buf_len += put_attrib(client->out_buf + client->out_buf_len, CCD_IMAGE_WIDTH_STRING,  image_width);
        client->out_buf_len += put_attrib(client->out_buf + client->out_buf_len, CCD_IMAGE_HEIGHT_STRING, image_height);
        memcpy(client->out_buf + client->out_buf_len, ">\n", 2);
        client->out_buf_len += 2;
    }
    /*
     * Read exposure.
     */
    rcount = read_exposure(exposure, image_buf, image_count);
    /*
     * Translate image data into text.
     */
    image_buf = client->out_buf + client->out_buf_size - row_bytes - dev->sizeof_pixel;
    while (row_bytes)
    {
        unsigned int pixel;

        if (dev->sizeof_pixel == 2)
            pixel = *(unsigned short *)image_buf;
        else if (dev->sizeof_pixel == 4)
            pixel = *(unsigned int *)image_buf;
        else
            pixel = *(unsigned char *)image_buf;
        client->out_buf_len += put_int(client->out_buf + client->out_buf_len, pixel);
        client->out_buf[client->out_buf_len++] = '\n';
        row_bytes -= dev->sizeof_pixel;
        image_buf += dev->sizeof_pixel;
    }
    /*
     * Add end image element when done.
     */
    if (dev->current_read_row == CCD_END_FRAME_LOAD)
    {
        memcpy(client->out_buf + client->out_buf_len, "</", 2);
        client->out_buf_len += 2;
        memcpy(client->out_buf + client->out_buf_len, CCD_IMAGE_STRING, strlen(CCD_IMAGE_STRING));
        client->out_buf_len += strlen(CCD_IMAGE_STRING);
        memcpy(client->out_buf + client->out_buf_len, ">\n", 2);
        client->out_buf_len += 2;
    }
    /*
     * Copy as much into user buffer as will fit.
     */
    rcount = min_ccd(count, client->out_buf_len);
    memcpy(buf, client->out_buf, rcount);
    if ((client->out_buf_len -= rcount))
        memcpy(client->out_buf, client->out_buf + rcount, client->out_buf_len);
    return (rcount);
}
/*
 * Create a new exposure request from a message block.
 */
void new_exposure(struct ccd_client *client, CCD_ELEM_TYPE *msg)
{
    struct ccd_exposure *prev_exposure,
                       **exposure_list = &(client->device->exposure_list),
                        *exposure      = kmalloc(sizeof(struct ccd_exposure), GFP_KERNEL);
    /*
     * Create new exposure request from message block.
     */
    exposure->width       = msg[CCD_EXP_WIDTH_INDEX];
    exposure->height      = msg[CCD_EXP_HEIGHT_INDEX];
    exposure->xoffset     = msg[CCD_EXP_XOFFSET_INDEX];
    exposure->yoffset     = msg[CCD_EXP_YOFFSET_INDEX];
    exposure->xbin        = msg[CCD_EXP_XBIN_INDEX];
    exposure->ybin        = msg[CCD_EXP_YBIN_INDEX];
    exposure->dac_bits    = msg[CCD_EXP_DAC_INDEX];
    exposure->flags       = (msg[CCD_EXP_FLAGS_INDEX] & ~CCD_FIELD_MASK) | client->exposure_flags;
    exposure->msec        = msg[CCD_EXP_MSEC_LO_INDEX] + (msg[CCD_EXP_MSEC_HI_INDEX] << 16); /* Careful here with big endian machines to pack this right */
    exposure->complete    = 0;
    exposure->device      = client->device;
    exposure->client      = client;
    exposure->next        = NULL;
    exposure->client_next = client->exposure_list;
    client->exposure_list = exposure;
    /*
     * Validate some of the parameters.
     */
    if (exposure->xbin < 1) exposure->xbin = 1;
    if (exposure->ybin < 1) exposure->ybin = 1;
    if (exposure->width  > client->device->width)  exposure->width  = client->device->width;
    if (exposure->height > client->device->height) exposure->height = client->device->height;
    if (exposure->dac_bits > client->device->dac_bits) exposure->dac_bits = client->device->dac_bits;
    if (exposure->width  + exposure->xoffset > client->device->width)  exposure->xoffset = client->device->width  - exposure->width;
    if (exposure->height + exposure->yoffset > client->device->height) exposure->yoffset = client->device->height - exposure->height;
    /*
     * Insert exposure in order of completion time.
     */
    if (*exposure_list)
    {
        if ( (exposure->flags & CCD_NOWIPE_FRAME)
         && !(exposure->flags & (*exposure_list)->flags & CCD_FIELD_BOTH)
         && !(*exposure_list)->next)
        {
            /*
             * Exposures that don't require a wipe and only one field can overlap with current exposure in the other field.
             * Can only be one current exposure for this to work (should be most of the time).
             */
            exposure->begin = jiffies;
            exposure->end   = exposure->begin + MSEC2JIFFY(exposure->msec);
            if (exposure->end < (*exposure_list)->end)
            {
                /*
                 * Overlap exposure with current exposure if they use different fields
                 * and the new one finishes before the current one. Insert new exposure
                 * before current exposure.
                 */
                exposure->next = *exposure_list;
                *exposure_list = exposure;
                /*
                 * Update the timers.
                 */
                del_timer(&(exposure->device->exposure_timer));
                exposure->device->exposure_timer.expires = exposure->end;
                exposure->device->exposure_timer.data    = (unsigned long)exposure;
                add_timer(&(exposure->device->exposure_timer));
            }
            else
            {
                /*
                 * Overlap exposure with current one if they use different fields
                 * but the new one finished after the current one.  This only works
                 * if there are no exposures following the current one.  Insert new
                 * exposure after the current one. This is the case of progressive
                 * exposure mode for multi-frame device like SX-MX cameras.
                 */
                exposure->device->new_frame(exposure->device->param, exposure->xoffset, exposure->yoffset, exposure->width, exposure->height, exposure->xbin, exposure->ybin, exposure->dac_bits, exposure->msec, exposure->flags);
                (*exposure_list)->next = exposure;
            }
        }
        else
        {
            /*
             * Add to end of list.
             */
            for (prev_exposure = *exposure_list; prev_exposure->next; prev_exposure = prev_exposure->next);
            exposure->begin     = prev_exposure->end;
            exposure->end       = exposure->begin + MSEC2JIFFY(exposure->msec);
            prev_exposure->next = exposure;
        }
    }
    else
    {
        /*
         * Only exposure, start now.
         */
        *exposure_list  = exposure;
        start_exposure(exposure);
    }
}
/*
 * Abort all current CCD I/O.
 */
void abort_exposures(struct ccd_client *client)
{
    struct ccd_exposure *exposure;

    /*
     * Tell the driver to stop reading the frame if its in the middle of a download or wake up a pending read.
     */
    if (client->exposure_list)
    {
        if ((client->device->exposure_list == client->exposure_list) && client->device->current_read_row != CCD_END_FRAME_LOAD)
        {
            if (client->device->current_read_row != CCD_START_FRAME_LOAD)
            {
                client->device->control(client->device->param, CCD_CTRL_CMD_RESET, 0);
                client->device->end_read(client->device->param, client->exposure_list->flags);
            }
            client->device->current_read_row = CCD_END_FRAME_LOAD;
        }
        if (!client->exposure_list->complete)
            wake_up_interruptible(&(client->read_wait));
    }
    /*
     * Remove all the exposures.
     */
    while ((exposure = client->exposure_list))
    {
        del_exposure(exposure);
        client->exposure_list = client->exposure_list->client_next;
        kfree(exposure);
    }
    client->out_buf_len = 0;
}

/***************************************************************************\
*                                                                           *
*                            CCD message parsers.                           *
*                                                                           *
\***************************************************************************/

/*
 * Parse binary message.
 */
int parse_binary_msg(struct ccd_client *client)
{
    int            msg_len;
    CCD_ELEM_TYPE *reply;
    CCD_ELEM_TYPE *msg = (CCD_ELEM_TYPE *)client->in_buf;

    while (client->in_buf_len)
    {
        /*
         * Check for valid message header.
         */
        if (msg[CCD_MSG_HEADER_INDEX] == CCD_MSG_HEADER)
        {
            if (client->in_buf_len >= CCD_MSG_QUERY_LEN && msg[CCD_MSG_INDEX] == CCD_MSG_QUERY)
            {
                /*
                 * Put CCD parameters in read buffer.
                 */
                if (client->out_buf_len + CCD_MSG_CCD_LEN <= client->out_buf_size)
                {
                    reply = (CCD_ELEM_TYPE *)(client->out_buf + client->out_buf_len);
                    memcpy((char *)&reply[CCD_CCD_NAME_INDEX], client->device->id_string, CCD_CCD_NAME_LEN);
                    reply[CCD_MSG_HEADER_INDEX]        = CCD_MSG_HEADER;
                    reply[CCD_MSG_LENGTH_LO_INDEX]     = CCD_MSG_CCD_LEN;
                    reply[CCD_MSG_LENGTH_HI_INDEX]     = 0;
                    reply[CCD_MSG_INDEX]               = CCD_MSG_CCD;
                    reply[CCD_CCD_WIDTH_INDEX]         = client->device->width;
                    reply[CCD_CCD_HEIGHT_INDEX]        = client->device->height;
                    reply[CCD_CCD_PIX_WIDTH_INDEX]     = client->device->pixel_width;
                    reply[CCD_CCD_PIX_HEIGHT_INDEX]    = client->device->pixel_height;
                    reply[CCD_CCD_FIELDS_INDEX]        = client->device->image_fields;
                    reply[CCD_CCD_DEPTH_INDEX]         = client->device->image_depth;
                    reply[CCD_CCD_DAC_INDEX]           = client->device->dac_bits;
                    reply[CCD_CCD_COLOR_INDEX]         = client->device->color_format;
                    reply[CCD_CCD_CAPS_INDEX]          = client->device->flag_caps;
                    client->out_buf_len               += CCD_MSG_CCD_LEN;
                }
                else
                    return (-ENOBUFS);
            }
            else if (client->in_buf_len >= CCD_MSG_EXP_LEN && msg[CCD_MSG_INDEX] == CCD_MSG_EXP)
                /*
                 * New exposure request.
                 */
                new_exposure(client, msg);
            else if (client->in_buf_len >= CCD_MSG_CTRL_LEN && msg[CCD_MSG_INDEX] == CCD_MSG_CTRL)
                /*
                 * Control request.
                 */
                client->device->control(client->device->param, msg[CCD_CTRL_CMD_INDEX], msg[CCD_CTRL_PARM_LO_INDEX] | (msg[CCD_CTRL_PARM_HI_INDEX] << 16));
            else if (client->in_buf_len >= CCD_MSG_ABORT_LEN && msg[CCD_MSG_INDEX] == CCD_MSG_ABORT)
                /*
                 * Abort all exposures.
                 */
                abort_exposures(client);
	    else if (client->in_buf_len >= CCD_MSG_TEMP_LEN && msg[CCD_MSG_INDEX] == CCD_MSG_TEMP)
	    {
		int temp = msg[CCD_TEMP_SET_LO_INDEX] | (msg[CCD_TEMP_SET_HI_INDEX] << 16);
		client->device->temp_control(client->device->param, &(msg[CCD_TEMP_FAN_INDEX]), &temp); 
                /*
                 * Put CCD temp in read buffer.
                 */
                if (client->out_buf_len + CCD_MSG_TEMP_LEN <= client->out_buf_size)
		{
                    reply = (CCD_ELEM_TYPE *)(client->out_buf + client->out_buf_len);
                    reply[CCD_MSG_HEADER_INDEX]        = CCD_MSG_HEADER;
                    reply[CCD_MSG_LENGTH_LO_INDEX]     = CCD_MSG_TEMP_LEN;
                    reply[CCD_MSG_LENGTH_HI_INDEX]     = 0;
                    reply[CCD_MSG_INDEX]               = CCD_MSG_TEMP;
		    reply[CCD_TEMP_FAN_INDEX]          = msg[CCD_TEMP_FAN_INDEX];
                    reply[CCD_TEMP_SET_LO_INDEX]       = temp & 0xffff;
                    reply[CCD_TEMP_SET_HI_INDEX]       = temp >> 16;
                    client->out_buf_len               += CCD_MSG_TEMP_LEN;
		}
                else
                    return (-ENOBUFS);
  	    }
            /*
             * Shift out last command buffer.
             */
            msg_len = msg[CCD_MSG_LENGTH_LO_INDEX] + (msg[CCD_MSG_LENGTH_HI_INDEX] << 16);
            if (msg_len >= client->in_buf_len)
                client->in_buf_len = 0;
            else
                memcpy(client->in_buf, client->in_buf + msg_len, client->in_buf_len -= msg_len);
        }
        else
            /*
             * Garbage.
             */
            client->in_buf_len = 0;
    }
    return (0);
}
/*
 * Very simple lexical analyzer.
 */
int get_token(unsigned char **buf, int len, int *tokens, char **strings, unsigned int *value)
{
    int i, j;

    while (len && **buf <= ' ') len--, (*buf)++;
    if (len)
    {
        if (**buf >= 'A' && **buf <= 'Z')
        {
            for (i = 0; len && (*buf)[i] >= 'A' && (*buf)[i] <= 'Z'; i++, len--);
            if (len)
            {
                for (j = 0; strings[j][0] != '\0'; j++)
                    if (!strncmp(strings[j], *buf, i))
                        return (*buf += i, tokens[j]); /* Match   */
                return (*buf += i, CCD_UNKNOWN_TOKEN); /* Unknown */
            }
        }
        else if (**buf >= '0' && **buf <= '9')
        {
            for (i = *value = 0; len && (*buf)[i] >= '0' && (*buf)[i] <= '9'; i++, len--)
                *value = *value * 10 + (*buf)[i] - '0';
            if (len)
                return (*buf += i, CCD_NUM_TOKEN); /* Number */
        }
        else
            return (*((*buf)++)); /* Punctuation token */
    }
    return (-1); /* Exhausted buffer */
}
/*
 * Token arrays.
 */
char *msg_strings[]     = { CCD_QUERY_STRING, CCD_ERRORNO_STRING, CCD_EXP_STRING, CCD_ABORT_STRING, CCD_CTRL_STRING, "" };
int   msg_tokens[]      = { CCD_QUERY_TOKEN,  CCD_ERRORNO_TOKEN,  CCD_EXP_TOKEN,  CCD_ABORT_TOKEN,  CCD_CTRL_TOKEN};
char *query_strings[]   = { "" };
int   query_tokens[]    = { 0 };
char *img_strings[]     = { CCD_EXP_WIDTH_STRING, CCD_EXP_HEIGHT_STRING, CCD_EXP_XOFFSET_STRING, CCD_EXP_YOFFSET_STRING, CCD_EXP_XBIN_STRING, CCD_EXP_YBIN_STRING, CCD_EXP_DAC_STRING, CCD_EXP_FLAGS_STRING, CCD_EXP_MSEC_STRING, ""};
int   img_tokens[]      = { CCD_EXP_WIDTH_TOKEN,  CCD_EXP_HEIGHT_TOKEN,  CCD_EXP_XOFFSET_TOKEN,  CCD_EXP_YOFFSET_TOKEN,  CCD_EXP_XBIN_TOKEN,  CCD_EXP_YBIN_TOKEN,  CCD_EXP_DAC_TOKEN,  CCD_EXP_FLAGS_TOKEN,  CCD_EXP_MSEC_TOKEN};
char *abort_strings[]   = { "" };
int   abort_tokens[]    = { 0 };
char *ctrl_strings[]    = { CCD_CTRL_CMD_STRING, CCD_CTRL_PARM_STRING, ""};
int   ctrl_tokens[]     = { CCD_CTRL_CMD_TOKEN,  CCD_CTRL_PARM_TOKEN};
char *errorno_strings[] = { "" };
int   errorno_tokens[]  = { 0 };
/*
 * Text message parser state machine. Syntax errors are just dis-regarded.
 */
int parse_text_msg(struct ccd_client *client)
{
    static int     last_errorno = 0;
    unsigned char *scan_buf;
    int            token;
    unsigned int   num;

    /*
     * Upper-case buffer before parsing.
     */
    for (num = 0; num < client->in_buf_len; num++)
        if (client->in_buf[num] >= 'a' && client->in_buf[num] <= 'z')
            client->in_buf[num] -= 'a' - 'A';
    while (client->in_buf_len)
    {
        scan_buf = client->in_buf;
        switch (client->parse_msg[CCD_MSG_INDEX])
        {
            case CCD_QUERY_TOKEN:
                if (((token = get_token(&scan_buf, client->in_buf_len, query_tokens, query_strings, &num)) == '>')
                 && (client->parse_token == '/'))
                {
                    /*
                     * Put CCD parameters in read buffer.
                     */
                    if (client->out_buf_len + 150 <= client->out_buf_size)
                    {
                        client->out_buf[client->out_buf_len++] = '<';
                        memcpy(client->out_buf + client->out_buf_len, CCD_CCD_STRING, strlen(CCD_CCD_STRING));
                        client->out_buf_len += strlen(CCD_CCD_STRING);
                        memcpy(client->out_buf + client->out_buf_len, CCD_CCD_NAME_STRING, strlen(CCD_CCD_NAME_STRING));
                        client->out_buf_len += strlen(CCD_CCD_NAME_STRING);
                        client->out_buf[client->out_buf_len++] = '=';
                        client->out_buf[client->out_buf_len++] = '"';
                        memcpy(client->out_buf + client->out_buf_len, client->device->id_string, CCD_CCD_NAME_LEN);
                        client->out_buf_len += CCD_CCD_NAME_LEN;
                        client->out_buf[client->out_buf_len++] = '"';
                        client->out_buf_len += put_attrib(client->out_buf + client->out_buf_len, CCD_CCD_WIDTH_STRING,        client->device->width);
                        client->out_buf_len += put_attrib(client->out_buf + client->out_buf_len, CCD_CCD_HEIGHT_STRING,       client->device->height);
                        client->out_buf_len += put_attrib(client->out_buf + client->out_buf_len, CCD_CCD_PIX_WIDTH_STRING,    client->device->pixel_width);
                        client->out_buf_len += put_attrib(client->out_buf + client->out_buf_len, CCD_CCD_PIX_HEIGHT_STRING,   client->device->pixel_height);
                        client->out_buf_len += put_attrib(client->out_buf + client->out_buf_len, CCD_CCD_FIELDS_STRING,       client->device->image_fields);
                        client->out_buf_len += put_attrib(client->out_buf + client->out_buf_len, CCD_CCD_DEPTH_STRING,        client->device->image_depth);
                        client->out_buf_len += put_attrib(client->out_buf + client->out_buf_len, CCD_CCD_DAC_STRING,          client->device->dac_bits);
                        client->out_buf_len += put_attrib(client->out_buf + client->out_buf_len, CCD_CCD_COLOR_STRING,        client->device->color_format);
                        client->out_buf_len += put_attrib(client->out_buf + client->out_buf_len, CCD_CCD_CAPS_STRING,         client->device->flag_caps);
                        memcpy(client->out_buf + client->out_buf_len, "/>\n", 3);
                        client->out_buf_len += 3;
                        last_errorno = 0;
                    }
                    else
                        last_errorno = -ENOMEM;
                    /*
                     * Reset state machine to look for new message.
                     */
                    client->parse_msg[CCD_MSG_INDEX] = 0;
                }
                else if (token == -1)
                {
                    /*
                     * Ran out of buffer space.
                     */
                    last_errorno = -ENOMEM;
                    return (0);
                }
                break;
            case CCD_ERRORNO_TOKEN:
                if (((token = get_token(&scan_buf, client->in_buf_len, errorno_tokens, errorno_strings, &num)) == '>')
                    && (client->parse_token == '/'))
                {
                    /*
                     * Put CTRL request return value in read buffer.
                     */
                    if (client->out_buf_len + 150 <= client->out_buf_size)
                    {
                        client->out_buf[client->out_buf_len++] = '<';
                        memcpy(client->out_buf + client->out_buf_len, CCD_ERRORNO_STRING, strlen(CCD_ERRORNO_STRING));
                        client->out_buf_len += strlen(CCD_ERRORNO_STRING);
                        client->out_buf_len += put_attrib(client->out_buf + client->out_buf_len, "VALUE", last_errorno);
                        memcpy(client->out_buf + client->out_buf_len, "/>\n", 3);
                        client->out_buf_len += 3;
                    }
                    else
                        /* Drop it on the ground if not enough buffer space */
                        ;
                    /*
                     * Reset state machine to look for new message.
                     */
                    client->parse_msg[CCD_MSG_INDEX] = 0;
                }
                else if (token == -1)
                {
                    /*
                        * Ran out of buffer space.
                        */
                    return (0);
                }
                break;
            case CCD_EXP_TOKEN:
                switch ((token = get_token(&scan_buf, client->in_buf_len, img_tokens, img_strings, &num)))
                {
                    /*
                     * Ran out of buffer space.
                     */
                    case -1:
                        last_errorno = -ENOMEM;
                        return (0);
                    /*
                     * Known image request attributes.
                     */
                    case CCD_EXP_WIDTH_TOKEN:
                    case CCD_EXP_HEIGHT_TOKEN:
                    case CCD_EXP_XOFFSET_TOKEN:
                    case CCD_EXP_YOFFSET_TOKEN:
                    case CCD_EXP_XBIN_TOKEN:
                    case CCD_EXP_YBIN_TOKEN:
                    case CCD_EXP_DAC_TOKEN:
                    case CCD_EXP_FLAGS_TOKEN:
                    case CCD_EXP_MSEC_TOKEN:
                        client->parse_attrib = token - CCD_EXP_WIDTH_TOKEN + CCD_EXP_WIDTH_INDEX;
                        break;
                    case CCD_NUM_TOKEN:
                        if ((client->parse_token == '=') && client->parse_attrib)
                        {
                            if (client->parse_attrib == CCD_EXP_MSEC_LO_INDEX)
                            {
                                client->parse_msg[CCD_EXP_MSEC_LO_INDEX] = num & 0xFFFF;
                                client->parse_msg[CCD_EXP_MSEC_HI_INDEX] = num >> 16;
                            }
                            else
                                client->parse_msg[client->parse_attrib] = num;
                        }
                        client->parse_attrib = 0;
                        break;
                    /*
                     * Complete message.
                     */
                    case '>':
                        if (client->parse_token == '/')
                        {
                            /*
                             * New exposure request.
                             */
                            new_exposure(client, client->parse_msg);
                            /*
                             * Reset state machine to look for new message.
                             */
                            client->parse_msg[CCD_MSG_INDEX] = 0;
                            last_errorno = 0;
                        }
                        break;
                }
                break;
            case CCD_CTRL_TOKEN:
                switch ((token = get_token(&scan_buf, client->in_buf_len, ctrl_tokens, ctrl_strings, &num)))
                {
                    /*
                     * Ran out of buffer space.
                     */
                    case -1:
                        last_errorno = -ENOMEM;
                        return (0);
                    /*
                     * Known control attributes.
                     */
                    case CCD_CTRL_CMD_TOKEN:
                    case CCD_CTRL_PARM_TOKEN:
                        client->parse_attrib = token - CCD_CTRL_CMD_TOKEN + CCD_CTRL_CMD_INDEX;
                        break;
                    case CCD_NUM_TOKEN:
                        if ((client->parse_token == '=') && client->parse_attrib)
                        {
                            if (client->parse_attrib == CCD_CTRL_PARM_LO_INDEX)
                            {
                                client->parse_msg[CCD_CTRL_PARM_LO_INDEX] = num & 0xFFFF;
                                client->parse_msg[CCD_CTRL_PARM_HI_INDEX] = num >> 16;
                            }
                            else
                                client->parse_msg[client->parse_attrib] = num;
                        }
                        client->parse_attrib = 0;
                        break;
                    /*
                     * Complete message.
                     */
                    case '>':
                        if (client->parse_token == '/')
                        {
                            /*
                             * New control request.
                             */
                            last_errorno = client->device->control(client->device->param,
                                                              client->parse_msg[CCD_CTRL_CMD_INDEX],
                                                              client->parse_msg[CCD_CTRL_PARM_LO_INDEX] | (client->parse_msg[CCD_CTRL_PARM_HI_INDEX] << 16));
                            /*
                             * Reset state machine to look for new message.
                             */
                            client->parse_msg[CCD_MSG_INDEX] = 0;
                        }
                        break;
                }
                break;
            case CCD_ABORT_TOKEN:
                if (((token = get_token(&scan_buf, client->in_buf_len, abort_tokens, abort_strings, &num)) == '>')
                 && (client->parse_token == '/'))
                {
                    /*
                     * Abort all exposures.
                     */
                    abort_exposures(client);
                    /*
                     * Reset state machine to look for new message.
                     */
                    client->parse_msg[CCD_MSG_INDEX] = 0;
                    last_errorno = 0;
                }
                else if (token == -1)
                {
                    /*
                     * Ran out of buffer space.
                     */
                    last_errorno = -ENOMEM;
                    return (0);
                }
                break;
            default:
                /*
                 * Look for new message.
                 */
                switch ((token = get_token(&scan_buf, client->in_buf_len, msg_tokens, msg_strings, &num)))
                {
                    /*
                     * Ran out of buffer space.
                     */
                    case -1:
                        last_errorno = -ENOMEM;
                        return (0);
                    /*
                     * Known message tokens.
                     */
                    case CCD_MSG_EXP:
                        /*
                         * Default values for capture.
                         */
                        client->parse_msg[CCD_EXP_WIDTH_INDEX]   = client->device->width;
                        client->parse_msg[CCD_EXP_HEIGHT_INDEX]  = client->device->height;
                        client->parse_msg[CCD_EXP_XOFFSET_INDEX] = 0;
                        client->parse_msg[CCD_EXP_YOFFSET_INDEX] = 0;
                        client->parse_msg[CCD_EXP_XBIN_INDEX]    = 1;
                        client->parse_msg[CCD_EXP_YBIN_INDEX]    = 1;
                        client->parse_msg[CCD_EXP_DAC_INDEX]     = client->device->dac_bits;
                        client->parse_msg[CCD_EXP_FLAGS_INDEX]   = 0;
                        client->parse_msg[CCD_EXP_MSEC_LO_INDEX] = 1000;
                        client->parse_msg[CCD_EXP_MSEC_HI_INDEX] = 0;
                    case CCD_MSG_QUERY:
                    case CCD_MSG_ABORT:
                    case CCD_MSG_CTRL:
                    case CCD_MSG_ERRORNO:
                        if (client->parse_token == '<')
                            client->parse_msg[CCD_MSG_INDEX] = token;
                }
        }
        client->parse_token = token;
        if (scan_buf > client->in_buf)
            memcpy(client->in_buf, scan_buf, client->in_buf_len -= (unsigned int)(scan_buf - client->in_buf));
    }
    return (0);
}

/***************************************************************************\
*                                                                           *
*                          CCD class driver file ops.                       *
*                                                                           *
\***************************************************************************/

/*
 * Read.
 */
static ssize_t ccd_read(struct file *file, char *buf, size_t count, loff_t *offset)
{
    struct ccd_device   *dev;
    unsigned int         num    = NUM(file->f_dentry->d_inode->i_rdev);
    unsigned int         field  = FIELD(file->f_dentry->d_inode->i_rdev);
    unsigned int         mode   = MODE(file->f_dentry->d_inode->i_rdev);
    struct ccd_client   *client = (struct ccd_client *)file->private_data;
    ssize_t              rcount = 0;

    if ((num >= MAX_CCDS) || (field > MAX_FIELDS) || !client || !(dev = client->device) || !dev->registered)
        return (-ENODEV);
    if (!count)
        return (0);
    if (buf == 0 || !access_ok(VERIFY_READ, (void *)buf, count))
        return (-EFAULT);
    if (client->out_buf_len > 0)
    {
        /*
         * Something in the output buffer.
         */
        rcount = min_ccd(count, client->out_buf_len);
        memcpy(buf, client->out_buf, rcount);
        if ((client->out_buf_len -= rcount))
            memcpy(client->out_buf, client->out_buf + rcount, client->out_buf_len);
    }
    else if (client->exposure_list)
    {
        if ((dev->exposure_list->client != client) || !dev->exposure_list->complete)
        {
            if (file->f_flags & O_NONBLOCK)
                /*
                 * Don't block.
                 */
                return (-EAGAIN);
            /*
             * Exposure not ready for reading so block.
             */
            interruptible_sleep_on(&(client->read_wait));
            if (signal_pending(current))
                return (-EINTR);
        }
        if (dev->exposure_list && (dev->exposure_list->client == client) && dev->exposure_list->complete)
        {
            if (mode == MODE_BINARY)
                rcount = read_binary_exposure(dev->exposure_list, buf, count);
            else /* if (mode == MODE_TEXT) */
                rcount = read_text_exposure(dev->exposure_list, buf, count);
        }
    }
    return (rcount);
}
/*
 * Write.
 */
static ssize_t ccd_write(struct file *file, const char *buf, size_t count, loff_t *offset)
{
    struct ccd_device *dev;
    struct ccd_client *client = (struct ccd_client *)file->private_data;
    unsigned int       num    = NUM(file->f_dentry->d_inode->i_rdev);
    unsigned int       field  = FIELD(file->f_dentry->d_inode->i_rdev);
    unsigned int       mode   = MODE(file->f_dentry->d_inode->i_rdev);
    ssize_t            wcount = 0;
    int                err    = 0;

    if ((num >= MAX_CCDS) || (field > MAX_FIELDS) || !client || !(dev = client->device) || !dev->registered)
        return (-ENODEV);
    if (!count)
        return (0);
    if (buf == 0 || !access_ok(VERIFY_WRITE, (void *)buf, count))
        return (-EFAULT);
    /*
     * Copy buffer into in_buf until there is enough to parse the command.
     */
    wcount = min_ccd(count, (MAX_IN_BUF_SIZE - client->in_buf_len));
    memcpy(client->in_buf + client->in_buf_len, buf, wcount);
    client->in_buf_len += wcount;
    if (mode == MODE_BINARY)
        err = parse_binary_msg(client);
    else /* if (mode == MODE_TEXT) */
        err = parse_text_msg(client);
    return (err ? err : wcount);
}
/*
 * Poll.
 */
static unsigned int ccd_poll(struct file *file, poll_table *wait)
{
    struct ccd_device *dev;
    struct ccd_client *client = (struct ccd_client *)file->private_data;
    unsigned int       mask   = 0;

    if (client && (dev = client->device))
    {
        /*
         * Only poll_wait if there is nothing available to read.
         */
        if ((client->out_buf_len == 0) && (!client->exposure_list || (dev->exposure_list->client != client) || !dev->exposure_list->complete))
            poll_wait(file, &(client->read_wait), wait);
        /*
         * Check for completed exposure or something in the output buffer.
         */
        if ((client->out_buf_len > 0) || (client->exposure_list && (dev->exposure_list->client == client) && dev->exposure_list->complete))
            mask |= POLLIN | POLLRDNORM;	/* Readable */
    }
    return (mask);
}
/*
 * Open.
 */
static int ccd_open(struct inode *inode, struct file *file)
{
    int          err   = 0;
    unsigned int num   = NUM(inode->i_rdev);
    unsigned int field = FIELD(inode->i_rdev);
    unsigned int mode  = MODE(inode->i_rdev);

    if ((num >= MAX_CCDS) || (field > MAX_FIELDS) || !ccd_devices[num].registered)
        return (-ENODEV);
    /*
     * Check for interline capable device.
     */
    if ((field > 0) && ccd_devices[num].image_fields <= 1)
        return (-ENODEV);
    /*
     * Only allow one open per psuedo-device.
     */
    if (ccd_clients[num][field].opened)
        return (-EBUSY);
    if (ccd_devices[num].opened == 0)
        if ((err = ccd_devices[num].open(ccd_devices[num].param)))
            return (err);
    /*
     * Init client data structure.
     */
    if (mode == MODE_BINARY)
        /*
         * Allocate enough room for a few messages.
         * All pixel data is written directly to user buffer.
         */
        ccd_clients[num][field].out_buf_size = 256;
    else if (mode == MODE_TEXT)
        /*
         * Allocate enough room to hold a scanline
         * worth of textual data plus a little slop.
         */
        ccd_clients[num][field].out_buf_size = ccd_clients[num][field].device->width
                                             * ccd_clients[num][field].device->sizeof_pixel
                                             * 3
                                             + ccd_clients[num][field].device->width    /* Roughly 3 chars per byte of pixel data plus '\n' */
                                             + 32 /* Slop */;
    else
        return (-ENODEV);
    ccd_clients[num][field].opened                   = 1;
    ccd_clients[num][field].in_buf_len               = 0;
    ccd_clients[num][field].out_buf_len              = 0;
    ccd_clients[num][field].in_buf                   = vmalloc(MAX_IN_BUF_SIZE);
    ccd_clients[num][field].out_buf                  = vmalloc(ccd_clients[num][field].out_buf_size);
    ccd_clients[num][field].parse_msg[CCD_MSG_INDEX] = 0;
    ccd_clients[num][field].parse_token              = 0;
    ccd_clients[num][field].parse_attrib             = 0;
    ccd_clients[num][field].exposure_list            = 0;
    if (!ccd_clients[num][field].in_buf || !ccd_clients[num][field].out_buf)
    {
        ccd_clients[num][field].opened = 0;
        if (ccd_clients[num][field].in_buf)  vfree(ccd_clients[num][field].in_buf);
        if (ccd_clients[num][field].out_buf) vfree(ccd_clients[num][field].out_buf);
        return (-ENOMEM);
    }
    ccd_devices[num].opened++;
    file->private_data = (void *)&(ccd_clients[num][field]);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
    MOD_INC_USE_COUNT;
#endif
    return (0);
}
/*
 * Close.
 */
static int ccd_close(struct inode *inode, struct file *file)
{
    struct ccd_client   *client = (struct ccd_client *)file->private_data;
    unsigned int         num    = NUM(inode->i_rdev);
    unsigned int         field  = FIELD(inode->i_rdev);

    /*
     * Note commented out test: device may have been unplugged during use.
     */
    if ((num >= MAX_CCDS) || (field > MAX_FIELDS)/* || !ccd_devices[num].registered*/)
        return (-ENODEV);
    /*
     * If not opened, return.
     */
    if (!ccd_clients[num][field].opened)
        return (0);
    /*
     * Clear out any pending exposures.
     */
    abort_exposures(client);
    ccd_clients[num][field].opened = 0;
    if (ccd_devices[num].opened && (--ccd_devices[num].opened == 0))
        ccd_devices[num].close(ccd_devices[num].param);
    /*
     * Free up I/O buffers.
     */
    vfree(ccd_clients[num][field].in_buf);
    vfree(ccd_clients[num][field].out_buf);
    ccd_clients[num][field].in_buf       = NULL;
    ccd_clients[num][field].in_buf_len   = 0;
    ccd_clients[num][field].out_buf      = NULL;
    ccd_clients[num][field].out_buf_len  = 0;
    ccd_clients[num][field].out_buf_size = 0;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
    MOD_DEC_USE_COUNT;
#endif
    return (0);
}
/*
 * File operations structure.
 */
static struct file_operations ccd_fops =
{
#if 1
     read:      ccd_read,
     write:     ccd_write,
     poll:      ccd_poll,
     open:      ccd_open,
     release:   ccd_close
#else
    NULL,       /* lseek   */
    ccd_read,
    ccd_write,
    NULL,       /* readdir */
    ccd_poll,
    NULL,       /* ioctl   */
    NULL,       /* mmap    */
    ccd_open,
    NULL,       /* flush   */
    ccd_close,
    /* Fill rest with NULL */
#endif
};

/***************************************************************************\
*                                                                           *
*                         CCD mini driver entrypoints.                      *
*                                                                           *
\***************************************************************************/

/*
 * Failure entrypoints for unregistered devices.
 */
static int  fail_open(void *vp) {return (-ENODEV);}
static int  fail_control(void *vp, unsigned short c, unsigned long p) {return (-ENODEV);}
static int  fail_close(void *vp) {return (-ENODEV);}
static int  fail_read_row(void *vp, unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int xb, unsigned int yb, unsigned int f, unsigned char *b) {return (-ENODEV);}
static void fail_begin_read(void *vp, unsigned int r, unsigned int f) {}
static void fail_end_read(void *vp, unsigned int f) {}
static void fail_latch_frame(void *vp, unsigned int f) {}
static void fail_new_frame(void *vp, unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int xb, unsigned int yb, unsigned int db, unsigned int z, unsigned int f) {}
static void fail_temp_control (void *vp, unsigned short *fan, int *temp) {};
/*
 * Register a device.
 */
int ccd_register_device(struct ccd_mini *mini, void *param)
{
    int i, j;

    if (mini->version != CCD_VERSION)
    {
        printk(KERN_INFO "CCD mini-driver version mismatch\n");
        return (-1);
    }
    for (i = 0; i < MAX_CCDS; i++)
        if (!ccd_devices[i].registered)
        {
            /*
             * Copy name id, up to CCD_CCD_NAME_LEN chars, padded with spaces.
             */
            for (j = 0; j < CCD_CCD_NAME_LEN && mini->id_string[j]; j++)
                ccd_devices[i].id_string[j] = mini->id_string[j];
            while (j < CCD_CCD_NAME_LEN)
                ccd_devices[i].id_string[j++] = ' ';
            ccd_devices[i].id_string[CCD_CCD_NAME_LEN] = '\0';
            /*
             * Parameters.
             */
            ccd_devices[i].width        = mini->width;
            ccd_devices[i].height       = mini->height;
            ccd_devices[i].pixel_width  = mini->pixel_width;
            ccd_devices[i].pixel_height = mini->pixel_height;
            ccd_devices[i].image_fields = mini->image_fields;
            ccd_devices[i].image_depth  = mini->image_depth;
            ccd_devices[i].dac_bits     = mini->dac_bits;
            ccd_devices[i].color_format = mini->color_format;
            ccd_devices[i].flag_caps    = mini->flag_caps;
            ccd_devices[i].sizeof_pixel = (mini->image_depth + 7) / 8;
            /*
             * Functions.
             */
            ccd_devices[i].param = param;
            if (!(ccd_devices[i].open        = mini->open))        return (0);
            if (!(ccd_devices[i].control     = mini->control))     return (0);
            if (!(ccd_devices[i].close       = mini->close))       return (0);
            if (!(ccd_devices[i].read_row    = mini->read_row))    return (0);
            if (!(ccd_devices[i].begin_read  = mini->begin_read))  return (0);
            if (!(ccd_devices[i].end_read    = mini->end_read))    return (0);
            if (!(ccd_devices[i].latch_frame = mini->latch_frame)) return (0);
            if (!(ccd_devices[i].new_frame   = mini->new_frame))   return (0);
            ccd_devices[i].temp_control   = mini->temp_control;
            /*
             * OK, its done.
             */
            ccd_devices[i].registered    = 1;
            ccd_devices[i].exposure_list = NULL;
            printk(KERN_INFO "Registered CCD mini-driver: %s @ minor #%d and #%d\n", mini->id_string, i | MODE_BINARY, i | MODE_TEXT);
            return (0);
        }
    return (-1);
}
/*
 * Unregister the device.  Take care if it is currently open.
 */
int ccd_unregister_device(void *param)
{
    struct ccd_exposure *exposure;
    int                  dev, field;

    for (dev = 0; (dev < MAX_CCDS) &&  (ccd_devices[dev].param != param); dev++);
    if (dev >= MAX_CCDS)
        return (-1);
    if (ccd_devices[dev].registered)
    {
        if (ccd_devices[dev].opened)
            ccd_devices[dev].close(ccd_devices[dev].param);
        /*
         * Re-init devices.
         */
        ccd_devices[dev].id_string[0]     = '\0';
        ccd_devices[dev].registered       = 0;
        ccd_devices[dev].opened           = 0;
        ccd_devices[dev].exposure_list    = NULL;
        ccd_devices[dev].param            = NULL;
        ccd_devices[dev].current_read_row = CCD_END_FRAME_LOAD;
        ccd_devices[dev].width            = 0;
        ccd_devices[dev].height           = 0;
        ccd_devices[dev].pixel_width      = 0;
        ccd_devices[dev].pixel_height     = 0;
        ccd_devices[dev].image_fields     = 0;
        ccd_devices[dev].image_depth      = 0;
        ccd_devices[dev].dac_bits         = 0;
        ccd_devices[dev].color_format     = 0;
        ccd_devices[dev].flag_caps        = 0;
        ccd_devices[dev].sizeof_pixel     = 0;
        ccd_devices[dev].open             = fail_open;
        ccd_devices[dev].control          = fail_control;
        ccd_devices[dev].close            = fail_close;
        ccd_devices[dev].read_row         = fail_read_row;
        ccd_devices[dev].begin_read       = fail_begin_read;
        ccd_devices[dev].end_read         = fail_end_read;
        ccd_devices[dev].latch_frame      = fail_latch_frame;
        ccd_devices[dev].new_frame        = fail_new_frame;
        ccd_devices[dev].temp_control     = fail_temp_control;
        /*
         * Clear out any pending exposures.
         */
        for (field = 0; field < 3; field++)
        {
            while ((exposure = ccd_clients[dev][field].exposure_list))
            {
                del_exposure(exposure);
                ccd_clients[dev][field].exposure_list = ccd_clients[dev][field].exposure_list->client_next;
                kfree(exposure);
            }
            ccd_clients[dev][field].exposure_list = NULL;
        }
        return (0);
    }
    return (-1);
}

/***************************************************************************\
*                                                                           *
*                           Module entrypoints.                             *
*                                                                           *
\***************************************************************************/

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))
MODULE_AUTHOR("David Schmenk, dschmenk@earthlink.net");
MODULE_DESCRIPTION("CCD Astronomy camera class driver");
MODULE_LICENSE("GPL");
#endif

/*
 * Initialize module.
 */
int init_module(void)
{
    int i;

    /*
     * Register device.
     */
    if ((i = register_chrdev(ccd_major, ccd_string, &ccd_fops)) < 0)
        return (i);
    /*
     * If dynamic major #'s, use the one returned.
     */
    if (!ccd_major)
        ccd_major = i;
    printk(KERN_INFO "Registered %s @ major #%d\n", ccd_string, ccd_major);
    /*
     * Initialize data structures.
     */
    for (i = 0; i < MAX_CCDS; i++)
    {
        /*
         * Init devices.
         */
        ccd_devices[i].registered       = 0;
        ccd_devices[i].opened           = 0;
        ccd_devices[i].exposure_list    = NULL;
        ccd_devices[i].param            = NULL;
        ccd_devices[i].current_read_row = CCD_END_FRAME_LOAD;
        init_timer(&(ccd_devices[i].exposure_timer));
        ccd_devices[i].exposure_timer.function = complete_exposure;
        /*
         * Init clients.
         */
        ccd_clients[i][0].opened         = 0;
        ccd_clients[i][1].opened         = 0;
        ccd_clients[i][2].opened         = 0;
        ccd_clients[i][0].exposure_flags = CCD_FIELD_BOTH;
        ccd_clients[i][1].exposure_flags = CCD_FIELD_ODD;
        ccd_clients[i][2].exposure_flags = CCD_FIELD_EVEN;
        ccd_clients[i][0].in_buf_len     = 0;
        ccd_clients[i][1].in_buf_len     = 0;
        ccd_clients[i][2].in_buf_len     = 0;
        ccd_clients[i][0].out_buf_len    = 0;
        ccd_clients[i][1].out_buf_len    = 0;
        ccd_clients[i][2].out_buf_len    = 0;
        ccd_clients[i][0].device         = &ccd_devices[i];
        ccd_clients[i][1].device         = &ccd_devices[i];
        ccd_clients[i][2].device         = &ccd_devices[i];
        ccd_clients[i][0].exposure_list  = NULL;
        ccd_clients[i][1].exposure_list  = NULL;
        ccd_clients[i][2].exposure_list  = NULL;
        init_waitqueue_head(&(ccd_clients[i][0].read_wait));
        init_waitqueue_head(&(ccd_clients[i][1].read_wait));
        init_waitqueue_head(&(ccd_clients[i][2].read_wait));
    }
    return (0);
}
/*
 * Remove module:
 *      Delete device.
 */
void cleanup_module(void)
{
    /*
     * Unregister device.
     */
    unregister_chrdev(ccd_major, ccd_string);
}

