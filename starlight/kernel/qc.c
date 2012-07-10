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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))
#include <linux/slab.h>
#else
#include <linux/malloc.h>
#endif
#include <linux/mm.h>
#include <linux/parport.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include "ccd.h"
#ifndef min
#define min(a,b)        ((a) < (b)) ? (a) : (b)
#endif
/*
 * Set up some defines for the CCD macro functions
 */
#define DAT_OUT(io,v)   parport_write_data((io)->pport,v)
#define CTL_OUT(io,v)   parport_write_control((io)->pport,v)
#define DAT_IN(io)      parport_read_data((io)->pport)
#define STS_IN(io)      parport_read_status((io)->pport)
#define CTL_IN(io)      parport_read_control((io)->pport)

/***************************************************************************\
*                                                                           *
*                     Quickcam basic control macros                         *
*                                                                           *
\***************************************************************************/
/*
 * Quickcam Versions.
 */
#define QC_VER_BW					0x00/*0x01*/
#define QC_VER_COLOR				0x10
/*
 * Initial values.
 */
#define QC_EXPOSURE_BULB            255
#define QC_EXPOSURE_INIT            1
#define QC_CONTRAST_INIT            104
#define QC_OFFSET_INIT        	    255
#define QC_OFFSET_AUTO        	    255
#define QC_SATURATION_INIT          254
#define QC_BLACK_INIT               160
#define QC_WHITE_INIT               0
#define QC_HUE_INIT            		128
/*
 * Commands.
 */
#define QC_CMD_GET_IMAGE            7
#define QC_CMD_SET_EXPOSURE         11
#define QC_CMD_SET_TOP              13
#define QC_CMD_SET_LEFT             15
#define QC_CMD_SET_NUM_VERT         17
#define QC_CMD_SET_NUM_HORZ         19
#define QC_CMD_GET_VERSION          23
#define QC_CMD_BW_SET_CONTRAST      25
#define QC_CMD_BW_AUTOCALIBRATE_OFFSET 27
#define QC_CMD_BW_ECHO              29
#define QC_CMD_BW_SET_OFFSET        31
#define QC_CMD_BW_GET_OFFSET        33
#define QC_CMD_CLR_SET_BLACK        29
#define QC_CMD_CLR_SET_WHITE        31
#define QC_CMD_CLR_SET_HUE          33
#define QC_CMD_CLR_SET_SATURATION   35
#define QC_CMD_CLR_SET_CONTRAST     37
#define QC_CMD_CLR_GET_STATUS       41
#define QC_CMD_CLR_SET_SPEED        45
/*
 * Control commands.
 */
#define QC_CTRL_CMD_SET_BLACK       1
#define QC_CTRL_CMD_SET_WHITE       2
#define QC_CTRL_CMD_SET_HUE         3
#define QC_CTRL_CMD_SET_SATURATION  4
#define QC_CTRL_CMD_SET_CONTRAST    5
#define QC_CTRL_CMD_SET_OFFSET      6

/*
 * Timeout read count.
 */
#define QC_POLL_TIMEOUT             5000
/*
 * Port bits.
 */
#define QC_PORT_RESET               0x08
#define QC_PORT_ACK                 0x08
#define QC_PORT_ACK_HI              0x04
#define QC_PORT_ACK_LO              0x0C
#define QC_PORT_RDY1                0x08
#define QC_PORT_RDY1_HI             0x08
#define QC_PORT_RDY1_LO             0x00
#define QC_PORT_RDY2                0x01
#define QC_PORT_RDY2_HI             0x01
#define QC_PORT_RDY2_LO             0x00
#define QC_PORT_BIDIRECTIONAL       0x20
/*
 * Image options.
 */
#define QC_IMAGE_BYTE_XFER          0x0000
#define QC_IMAGE_WORD_XFER          0x0001
#define QC_IMAGE_BW_4BPP            0x0000
#define QC_IMAGE_BW_6BPP            0x0002
#define QC_IMAGE_BW_1X_XFER         0x0000
#define QC_IMAGE_BW_2X_XFER         0x0004
#define QC_IMAGE_BW_4X_XFER         0x0008
#define QC_IMAGE_CLR_1X_XFER        0x0000
#define QC_IMAGE_CLR_2X_XFER        0x0002
#define QC_IMAGE_CLR_4X_XFER        0x0004
#define QC_IMAGE_CLR_15BPP			0x0008
#define QC_IMAGE_CLR_24BPP			0x0018
#define QC_IMAGE_CLR_32BPP			0x0010
#define QC_IMAGE_FIRST_XFER       	0x0100
#define QC_IMAGE_FINAL_XFER       	0x0200
/*
 * Image size.
 */
#define QC_BW_WIDTH                 320
#define QC_BW_HEIGHT                240
#define QC_BW_PIX_WIDTH             (int)(6.3*256)
#define QC_BW_PIX_HEIGHT            (int)(5.5*256)
#define QC_CLR32_WIDTH              656
#define QC_CLR32_HEIGHT             496
#define QC_CLR32_PIX_WIDTH          (int)(6.3*256)
#define QC_CLR32_PIX_HEIGHT         (int)(5.5*256)
#define QC_CLR32_HFRONT_PORCH       24
#define QC_CLR32_VFRONT_PORCH       0
struct qc_device
{
    char             *id;
    unsigned int      version;
    unsigned int      width;
    unsigned int      height;
    unsigned int      depth;
    unsigned int      color_mode;
    unsigned int      black;
    unsigned int      white;
    unsigned int      hue;
    unsigned int      saturation;
    unsigned int      contrast;
    unsigned int      offset;
    unsigned int      last_read_row;
    unsigned char     clr32_row[QC_CLR32_WIDTH][2];
    struct pardevice *pdev;
    struct parport   *pport;
};

/***************************************************************************\
*                                                                           *
*                         Basic QC camera operations                        *
*                                                                           *
\***************************************************************************/
/*
 * Basic data movement routines.
 */
static int qc_write_byte(struct qc_device *qc, unsigned char data)
{
    int                    timeout;
    volatile unsigned char echo, heartbeat;

    /*
     * Send data out parallel port.
     */
    DAT_OUT(qc, data);
    CTL_OUT(qc, QC_PORT_ACK_HI);
    CTL_OUT(qc, QC_PORT_ACK_HI); // Delay
    /*
     * Wait for first nybble to be echoed.
     */
    timeout = QC_POLL_TIMEOUT;
    echo    = STS_IN(qc);
    while (((echo & QC_PORT_RDY1) == QC_PORT_RDY1_LO) && --timeout)
    {
        heartbeat = echo;
        echo      = STS_IN(qc);
        if ((heartbeat ^ echo) & 0xF0)
            timeout = QC_POLL_TIMEOUT;
    }
    //if (timeout == 0 || (echo & 0xF0) != (data & 0xF0))
    //    return (-1);
    /*
     * Acknowledge first nybble.
     */
    CTL_OUT(qc, QC_PORT_ACK_LO);
    CTL_OUT(qc, QC_PORT_ACK_LO); // Delay
    /*
     * Wait for second nybble to be echoed.
     */
    timeout = QC_POLL_TIMEOUT;
    echo    = STS_IN(qc);
    while (((echo & QC_PORT_RDY1) == QC_PORT_RDY1_HI) && --timeout)
    {
        heartbeat = echo;
        echo      = STS_IN(qc);
        if ((heartbeat ^ echo) & 0xF0)
            timeout = QC_POLL_TIMEOUT;
    }
    /*
     * Check for errors.
     */
    //if (timeout == 0 || (echo >> 4) != (data & 0x0F))
    //    return (-1);
    return (0);
}
static int qc_read_byte(struct qc_device *qc)
{
    int                    data, timeout;
    volatile unsigned char nybble, heartbeat;

    CTL_OUT(qc, QC_PORT_ACK_HI);
    CTL_OUT(qc, QC_PORT_ACK_HI); // Delay
    /*
     * Wait for first nybble to be output.
     */
    timeout = QC_POLL_TIMEOUT;
    nybble  = STS_IN(qc);
    while (((nybble & QC_PORT_RDY1) == QC_PORT_RDY1_LO) && --timeout)
    {
        heartbeat = nybble;
        nybble    = STS_IN(qc);
        if ((heartbeat ^ nybble) & 0xF0)
            timeout = QC_POLL_TIMEOUT;
    }
    if (timeout == 0)
        return (-1);
    /*
     * Acknowledge first nybble.
     */
    CTL_OUT(qc, QC_PORT_ACK_LO);
    CTL_OUT(qc, QC_PORT_ACK_LO); // Delay
    /*
     * Save upper nybble.
     */
    data = nybble & 0xF0;
    /*
     * Wait for second nybble to be output.
     */
    timeout = QC_POLL_TIMEOUT;
    nybble  = STS_IN(qc);
    while (((nybble & QC_PORT_RDY1) == QC_PORT_RDY1_HI) && --timeout)
    {
        heartbeat = nybble;
        nybble    = STS_IN(qc);
        if ((heartbeat ^ nybble) & 0xF0)
            timeout = QC_POLL_TIMEOUT;
    }
    if (timeout == 0)
        return (-1);
    data |= nybble >> 4;
    return (data);
}
static void qc_set_black(struct qc_device *qc)
{
    int timeout;
    int data;

    if (qc->version == QC_VER_COLOR)
    {
        qc_write_byte(qc, QC_CMD_CLR_SET_BLACK);
        qc_write_byte(qc, qc->black);
        timeout = QC_POLL_TIMEOUT;
        do
        {
            qc_write_byte(qc, QC_CMD_CLR_GET_STATUS);
            data = qc_read_byte(qc);
        } while ((data & 0x40) && --timeout);
    }
}
static void qc_set_white(struct qc_device *qc)
{
    if (qc->version == QC_VER_COLOR)
    {
        qc_write_byte(qc, QC_CMD_CLR_SET_WHITE);
        qc_write_byte(qc, qc->white);
    }
}
static void qc_set_hue(struct qc_device *qc)
{
    if (qc->version == QC_VER_COLOR)
    {
        qc_write_byte(qc, QC_CMD_CLR_SET_HUE);
        qc_write_byte(qc, qc->hue);
    }
}
static void qc_set_contrast(struct qc_device *qc)
{
    qc_write_byte(qc, qc->version == QC_VER_BW ? QC_CMD_BW_SET_CONTRAST : QC_CMD_CLR_SET_CONTRAST);
    qc_write_byte(qc, qc->contrast);
}
static void qc_set_saturation(struct qc_device *qc)
{
    if (qc->version == QC_VER_COLOR)
    {
        qc_write_byte(qc, QC_CMD_CLR_SET_SATURATION);
        qc_write_byte(qc, qc->saturation);
    }
}
static void qc_set_offset(struct qc_device *qc)
{
    int timeout;
    int data;

    if (qc->version == QC_VER_BW)
    {
        if (qc->offset >= QC_OFFSET_AUTO)
        {
            /*
             * Autocalibrate.
             */
            qc_write_byte(qc, QC_CMD_BW_AUTOCALIBRATE_OFFSET);
            qc_write_byte(qc, 0);
            timeout = QC_POLL_TIMEOUT*10;
            do
            {
                qc_write_byte(qc, QC_CMD_BW_GET_OFFSET);
                data = qc_read_byte(qc);
            } while ((data == 0xFF) && --timeout);
            if (timeout == 0)
                return;
            /*
             * Get autocalibrated offset.
             */
            qc->offset = data;
        }
        else
        {
            qc_write_byte(qc, QC_CMD_BW_SET_OFFSET);
            qc_write_byte(qc, qc->offset);
        }
    }
}
static int qc_reset(struct qc_device *qc)
{
    int timeout;
    volatile int data;

    CTL_OUT(qc, QC_PORT_ACK_LO);
    CTL_OUT(qc, QC_PORT_ACK_LO);
    CTL_OUT(qc, QC_PORT_ACK_LO);
    CTL_OUT(qc, QC_PORT_ACK_LO);
    CTL_OUT(qc, QC_PORT_RESET);
    CTL_OUT(qc, QC_PORT_RESET);
    CTL_OUT(qc, QC_PORT_RESET);
    CTL_OUT(qc, QC_PORT_RESET);
    CTL_OUT(qc, QC_PORT_ACK_LO);
    CTL_OUT(qc, QC_PORT_ACK_LO);
    CTL_OUT(qc, QC_PORT_ACK_LO);
    CTL_OUT(qc, QC_PORT_ACK_LO);
    /*
     * Wait for QuickCam to respond.
     */
    timeout = QC_POLL_TIMEOUT * 10;
    do
    {
        data = STS_IN(qc);
    } while (((data & QC_PORT_RDY1) != QC_PORT_RDY1_LO) && --timeout);
    /*
     * Check for errors.
     */
    if (timeout == 0)
        return (-1);
    /*
     * Get version.
     */
    qc_write_byte(qc, QC_CMD_GET_VERSION);
    qc->version = qc_read_byte(qc);
    if (qc->version == QC_VER_COLOR)
    {
        /*
         * Read port version and set speed to 2 (2.5 Mb/s).
         */
        data = qc_read_byte(qc);
        qc_write_byte(qc, QC_CMD_CLR_SET_SPEED);
        qc_write_byte(qc, 2);
        qc_set_black(qc);
        qc_set_white(qc);
        qc_set_hue(qc);
        qc_set_saturation(qc);
    }
    else if (qc->version == QC_VER_BW)
    {
        qc_set_offset(qc);
    }
    else
        return (-1);
    qc_set_contrast(qc);
    /*
     * Set to quick exposure.
     */
    qc_write_byte(qc, QC_CMD_SET_EXPOSURE);
    qc_write_byte(qc, 1);
    return(0);
}

/***************************************************************************\
*                                                                           *
*                         Basic QC camera functions                         *
*                                                                           *
\***************************************************************************/
static int qc_read_row_bw(void *vp, unsigned int offset, unsigned int row, unsigned int width, unsigned int xbin, unsigned int ybin, unsigned int dac_bits, unsigned int flags, unsigned char *buf)
{
    int               i, j, tmp;
    unsigned char     data, tmp_row[QC_BW_WIDTH];
    struct qc_device *qc = (struct qc_device *)vp;

    /*
     * Transfer image.
     */
    qc->last_read_row = row + ybin;
    offset           &= 1;
    for (i = 0; i < width + offset; i += 4)
    {
        data = ~qc_read_byte(qc);
        tmp_row[i + 0]  = (data & 0xFC) >> 2;
        tmp_row[i + 1]  = (data & 0x03) << 4;
        data = ~qc_read_byte(qc);
        tmp_row[i + 1] |= (data & 0xF0) >> 4;
        tmp_row[i + 2]  = (data & 0x0F) << 2;
        data = ~qc_read_byte(qc);
        tmp_row[i + 2] |= (data & 0xC0) >> 6;
        tmp_row[i + 3]  =  data & 0x3F;
    }
    /*
     * Vertically bin pixels.
     */
    while (--ybin)
        for (i = 0; i < width + offset; i += 4)
        {
            data = ~qc_read_byte(qc);
            tmp             = (data & 0xFC) >> 2;
            tmp_row[i + 0]  = min(tmp + tmp_row[i + 0], 255);
            tmp             = (data & 0x03) << 4;
            data = ~qc_read_byte(qc);
            tmp            |= (data & 0xF0) >> 4;
            tmp_row[i + 1]  = min(tmp + tmp_row[i + 1], 255);
            tmp             = (data & 0x0F) << 2;
            data = ~qc_read_byte(qc);
            tmp            |= (data & 0xC0) >> 6;
            tmp_row[i + 2]  = min(tmp + tmp_row[i + 2], 255);
            tmp             = data & 0x3F;
            tmp_row[i + 3]  = min(tmp + tmp_row[i + 3], 255);
        }
    /*
     * Horizontally bin pixels from row array.
     */
    for (i = 0; i < width; i+= xbin)
    {
        *buf = tmp_row[i + offset];
        for (j = xbin - 1; j; j--)
            *buf = min((int)tmp_row[i + j + (offset & 1)] + *buf, 255);
        buf++;
    }
    return(width / xbin);
}
static int qc_read_row_clr32(void *vp, unsigned int offset, unsigned int row, unsigned int width, unsigned int xbin, unsigned int ybin, unsigned int dac_bits, unsigned int flags, unsigned char *buf)
{
    int               i;
    struct qc_device *qc = (struct qc_device *)vp;
    unsigned int      red, green1, green2, blue;

    if (!(row & 1))
    {
        /*
         * Read two rows at a time.
         */
        for (i = 0; i < QC_CLR32_WIDTH; i += 2)
        {
            green1  = qc_read_byte(qc) ^ 0x88;
            blue    = qc_read_byte(qc) ^ 0x88;
            green2  = qc_read_byte(qc) ^ 0x88;
            red     = qc_read_byte(qc) ^ 0x88;
            qc->clr32_row[i    ][0] = green2;
            qc->clr32_row[i + 1][0] = red;
            qc->clr32_row[i    ][1] = blue;
            qc->clr32_row[i + 1][1] = green1;
        }
    }
    /*
     * Pull pixels from clr_row array.
     */
    for (i = 0; i < width; i+= xbin)
        *buf++ = qc->clr32_row[i + offset][row & 1];
    /*
     * Skip ybin rows.
     */
    if (ybin > 1)
    {
        for (ybin = (ybin >> 1) - ((row & 1) ^ 1); ybin; ybin--)
        {
            for (i = 0; i < QC_CLR32_WIDTH; i += 2)
            {
		        green1  = qc_read_byte(qc) ^ 0x88;
		        blue    = qc_read_byte(qc) ^ 0x88;
		        green2  = qc_read_byte(qc) ^ 0x88;
		        red     = qc_read_byte(qc) ^ 0x88;
                qc->clr32_row[i    ][0] = green2;
                qc->clr32_row[i + 1][0] = red;
                qc->clr32_row[i    ][1] = blue;
                qc->clr32_row[i + 1][1] = green1;
            }
        }
    }
    /*
     * Return # of bytes read.
     */
    return((width / xbin) * sizeof(unsigned short));
}
/*
 * Begin reading frame at row offset.
 */
static void qc_begin_read(void *vp, unsigned int offset, unsigned int flags)
{
    int               i;
    struct qc_device *qc = (struct qc_device *)vp;

    /*
     * Skip yoffset rows.
     */
    if (qc->version == QC_VER_BW)
    {
        qc->last_read_row = offset;
    }
    else
    {
        unsigned int red, green1, green2, blue;

        /*
         * Read two rows at a time.
         */
        offset = (offset + 1) >> 1;
        while (offset--)
        {
            for (i = 0; i < QC_CLR32_WIDTH; i += 2)
            {
                green1  = qc_read_byte(qc) ^ 0x88;
                blue    = qc_read_byte(qc) ^ 0x88;
                green2  = qc_read_byte(qc) ^ 0x88;
                red     = qc_read_byte(qc) ^ 0x88;
                /*
                 * Save last odd row.
                 */
                if (!offset)
                {
                    qc->clr32_row[i    ][1] = blue;
                    qc->clr32_row[i + 1][1] = green1;
                }
            }
        }
    }
}
/*
 * Complete reading a frame (turn off amplifier).
 */
static void qc_end_read(void *vp, unsigned int flags)
{
    int data, timeout;
    struct qc_device *qc = (struct qc_device *)vp;

    if (qc->version == QC_VER_BW)
    {
        CTL_OUT(qc, QC_PORT_ACK_HI);
        CTL_OUT(qc, QC_PORT_ACK_HI);
        CTL_OUT(qc, QC_PORT_ACK_LO);
        CTL_OUT(qc, QC_PORT_ACK_LO);
    }
    else
    {
        /*
         * Handle frame completion by just reseting the camera.
         */
        CTL_OUT(qc, QC_PORT_ACK_LO);
        CTL_OUT(qc, QC_PORT_ACK_LO);
        CTL_OUT(qc, QC_PORT_ACK_LO);
        CTL_OUT(qc, QC_PORT_ACK_LO);
        CTL_OUT(qc, QC_PORT_RESET);
        CTL_OUT(qc, QC_PORT_RESET);
        CTL_OUT(qc, QC_PORT_RESET);
        CTL_OUT(qc, QC_PORT_RESET);
        CTL_OUT(qc, QC_PORT_ACK_LO);
        CTL_OUT(qc, QC_PORT_ACK_LO);
        CTL_OUT(qc, QC_PORT_ACK_LO);
        CTL_OUT(qc, QC_PORT_ACK_LO);
        /*
         * Wait for QuickCam to respond.
         */
        timeout = QC_POLL_TIMEOUT * 10;
        do
        {
            data = STS_IN(qc);
        } while (((data & QC_PORT_RDY1) != QC_PORT_RDY1_LO) && --timeout);
    }
    /*
     * Set to quick exposure.
     */
    qc_write_byte(qc, QC_CMD_SET_EXPOSURE);
    qc_write_byte(qc, 1);
}
/*
 * Clear out frame and start integrating.
 */
static void qc_new_frame(void *vp, unsigned int xoffset, unsigned int yoffset, unsigned int width, unsigned int height, unsigned int xbin, unsigned int ybin, unsigned int dac_bits, unsigned int msec, unsigned int flags)
{
    struct qc_device *qc = (struct qc_device *)vp;

    if (qc->version == QC_VER_BW)
    {
        /*
         * Subframe the QC for faster guide downloads.
         */
        qc_write_byte(qc, QC_CMD_SET_LEFT);
        qc_write_byte(qc, 8 + xoffset / 2);
        qc_write_byte(qc, QC_CMD_SET_TOP);
        qc_write_byte(qc, 1 + yoffset);
        qc_write_byte(qc, QC_CMD_SET_NUM_VERT);
        qc_write_byte(qc, height);
        qc_write_byte(qc, QC_CMD_SET_NUM_HORZ);
        qc_write_byte(qc, ((width + (xoffset & 1)) * 6 + 23)/24);
    }
    else
    {
        qc_write_byte(qc, QC_CMD_SET_LEFT);
        qc_write_byte(qc, QC_CLR32_HFRONT_PORCH/4);
        qc_write_byte(qc, QC_CMD_SET_TOP);
        qc_write_byte(qc, QC_CLR32_VFRONT_PORCH/2+1);
        qc_write_byte(qc, QC_CMD_SET_NUM_VERT);
        qc_write_byte(qc, QC_CLR32_HEIGHT/2);
        qc_write_byte(qc, QC_CMD_SET_NUM_HORZ);
        qc_write_byte(qc, QC_CLR32_WIDTH/4);
    }
    /*
     * Switch to bulb mode.
     */
    qc_write_byte(qc, QC_CMD_SET_EXPOSURE);
    qc_write_byte(qc, QC_EXPOSURE_BULB);
}
/*
 * Latch a frame and prepare for reading.
 */
static void qc_latch_frame(void *vp, unsigned int flags)
{
    struct qc_device *qc = (struct qc_device *)vp;

    if (qc->version == QC_VER_BW)
    {
        /*
         * Use nybble xfer mode.
         */
        qc_write_byte(qc, QC_CMD_GET_IMAGE);
        qc_write_byte(qc, QC_IMAGE_BW_6BPP);
    }
    else
    {
        /*
         * Use nybble xfer mode.
         */
        qc_write_byte(qc, QC_CMD_GET_IMAGE);
        qc_write_byte(qc, QC_IMAGE_CLR_32BPP+1);
    }
}

/***************************************************************************\
*                                                                           *
*                   Open, close and control functions                       *
*                                                                           *
\***************************************************************************/

/*
 * Prepare to use CCD device.
 */
static int qc_open(void *vp)
{
    struct qc_device *qc = (struct qc_device *)vp;

    parport_claim_or_block(qc->pdev);
    qc_reset(qc);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
    MOD_INC_USE_COUNT;
#endif
    return(0);
}
/*
 * Control CCD device. This is device specific, stuff like temperature control, etc.
 */
static int qc_control(void *vp, unsigned short cmd, unsigned long param)
{
    struct qc_device *qc = (struct qc_device *)vp;

    switch (cmd)
    {
        /*
         * Well-known commands.
         */
        case CCD_CTRL_CMD_RESET:
            qc_reset(qc);
            break;
        /*
         * Quickcam specific commands.
         */
        case QC_CTRL_CMD_SET_BLACK:
            qc->black = param;
            qc_set_black(qc);
            break;
        case QC_CTRL_CMD_SET_WHITE:
            qc->white = param;
            qc_set_white(qc);
            break;
        case QC_CTRL_CMD_SET_HUE:
            qc->hue = param;
            qc_set_hue(qc);
            break;
        case QC_CTRL_CMD_SET_SATURATION:
            qc->saturation = param;
            qc_set_saturation(qc);
            break;
        case QC_CTRL_CMD_SET_CONTRAST:
            qc->contrast = param;
            qc_set_contrast(qc);
            break;
        case QC_CTRL_CMD_SET_OFFSET:
            qc->offset = param;
            qc_set_offset(qc);
            break;
        /*
         * Major hack while debugging.
         */
        case CCD_CTRL_CMD_DEC_MOD:
            parport_release(qc->pdev);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
            MOD_DEC_USE_COUNT;
#endif
            break;
    }
    return(0);
}
/*
 * Release CCD device.
 */
static int qc_close(void *vp)
{
    struct qc_device *qc = (struct qc_device *)vp;

    parport_release(qc->pdev);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
    MOD_DEC_USE_COUNT;
#endif
    return(0);
}

/***************************************************************************\
*                                                                           *
*                          QuickCam initialization                          *
*                                                                           *
\***************************************************************************/

int qc_init(struct qc_device *qc)
{
    /*
     * Set initial default values.
     */
    qc->version    = 0xFF;
    qc->black      = QC_BLACK_INIT;
    qc->white      = QC_WHITE_INIT;
    qc->hue        = QC_HUE_INIT;
    qc->saturation = QC_SATURATION_INIT;
    qc->contrast   = QC_CONTRAST_INIT;
    qc->offset     = QC_OFFSET_AUTO;
    return (qc_reset(qc));
}

/***************************************************************************\
*                                                                           *
*                          Module initialization                            *
*                                                                           *
\***************************************************************************/

#define QC_NO 2
static struct qc_device qc_devices[QC_NO];
static int               parport_nr[QC_NO] = { [0 ... QC_NO-1] = -1 };
static char             *parport_str[QC_NO] = { NULL,  };

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))
MODULE_AUTHOR("David Schmenk, dschmenk@earthlink.net");
MODULE_DESCRIPTION("Connectix parallel port QuickCam driver");
MODULE_LICENSE("GPL");
MODULE_PARM_DESC(parport_str,"QuickCam camera printer port #");
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
MODULE_PARM(parport_str, "1-" __MODULE_STRING(QC_NO) "s");
#else
module_param_array(parport_str, charp, NULL, 0);
#endif

static unsigned int count = 0;

static void qc_attach (struct parport *port)
{
    int i;
    for (i = 0; i < QC_NO; i++)
    {
        if (port->number == parport_nr[i])
        {
            qc_devices[count].pdev  = parport_register_device(port, "qc", NULL, NULL, NULL, 0, NULL);
            qc_devices[count].pport = port;
            if (!qc_init(&qc_devices[count]))
            {
                count++;
                break;
            }
            else
            {
                parport_unregister_device(qc_devices[count].pdev);
            }
        }
    }
}

static void qc_detach (struct parport *port)
{
    printk (KERN_INFO "qc: detaching parport %i\n", port->number);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
static struct list_head qc_driver_head = LIST_HEAD_INIT(qc_driver_head);
#endif

static struct parport_driver qc_driver;

int init_module(void)
{
    unsigned int i;

    for (i = 0; i < QC_NO; i++)
        qc_devices[i].pdev = NULL;
    if (parport_str[0])
    {
        /*
         *The user gave some parameters.  Let's see what they were.
         */
        for (i = 0; i < QC_NO && parport_str[i]; i++)
        {
            char *ep;
            unsigned long r = simple_strtoul(parport_str[i], &ep, 0);
            if (ep != parport_str[i])
                parport_nr[i] = r;
            else
            {
                printk(KERN_ERR "qc: bad port specifier `%s'\n", parport_str[i]);
                return(-ENODEV);
            }
        }
    }
    else
    {
        /*
         * Default to parport0.
         */
        parport_nr[0] = 0;
    }

    qc_driver.name = "qc";
    qc_driver.attach = qc_attach;
    qc_driver.detach = qc_detach;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,3))
    qc_driver.next = NULL;
#else
    qc_driver.list = qc_driver_head;
#endif

    // that will call attach for every founded parport
    if (parport_register_driver (&qc_driver))
        return -EIO;

    if (count)
    {
        for (i = 0; i < count; i++)
        {
            struct ccd_mini mini_device;

            strcpy(mini_device.id_string, (qc_devices[i].version == QC_VER_BW) ? "Connectix B/W QuickCam" : "Connectix Color QuickCam");
            mini_device.version      = CCD_VERSION;
            mini_device.width        = qc_devices[i].version == QC_VER_BW ? QC_BW_WIDTH : QC_CLR32_WIDTH;
            mini_device.height       = qc_devices[i].version == QC_VER_BW ? QC_BW_HEIGHT : QC_CLR32_HEIGHT;
            mini_device.pixel_width  = qc_devices[i].version == QC_VER_BW ? QC_BW_PIX_WIDTH : QC_CLR32_PIX_WIDTH;
            mini_device.pixel_height = qc_devices[i].version == QC_VER_BW ? QC_BW_PIX_HEIGHT : QC_CLR32_PIX_HEIGHT;
            mini_device.image_fields = 1;
            mini_device.image_depth  = 8;
            mini_device.dac_bits     = qc_devices[i].version == QC_VER_BW ? 6 : 8;
            mini_device.color_format = qc_devices[i].version == QC_VER_BW ? CCD_COLOR_MONOCHROME : CCD_COLOR_MATRIX_2X2 | 0x0294;
            mini_device.open         = qc_open;
            mini_device.control      = qc_control;
            mini_device.close        = qc_close;
            mini_device.read_row     = qc_devices[i].version == QC_VER_BW ? qc_read_row_bw : qc_read_row_clr32;
            mini_device.begin_read   = qc_begin_read;
            mini_device.end_read     = qc_end_read;
            mini_device.latch_frame  = qc_latch_frame;
            mini_device.new_frame    = qc_new_frame;
            if (ccd_register_device(&mini_device, (void *)&qc_devices[i]) < 0)
            {
                printk("qc: unable to register\n");
                return(-EIO);
            }
        }
    }
    else
    {
        printk(KERN_INFO "qc: no parport devices found\n");
        return(-ENODEV);
    }
    return(0);
}
void cleanup_module(void)
{
    int i;

    for (i = 0; i < QC_NO; i++)
    {
        if (qc_devices[i].pdev)
        {
            parport_unregister_device(qc_devices[i].pdev);
            ccd_unregister_device((void *)&qc_devices[i]);
        }
    }
}

