/***************************************************************************\

    Copyright (c) 2001, 2002 David Schmenk

    All rights reserved.

    The EZ-USB download code was based on Tony Cureington's driver.
    For more info go to ezusb.sourceforge.net

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
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))
#include <linux/slab.h>
#else
#include <linux/malloc.h>
#endif
#include <linux/vmalloc.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/mm.h>
#include <linux/usb.h>
#include <linux/serial.h>
#include <linux/sched.h>
#include <linux/tqueue.h>
#include <linux/version.h>
#include <asm/uaccess.h>
#include "ccd.h"
#include "sx_usb.h"
#ifndef min
#define min(a,b)                    ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a,b)                    ((a) > (b) ? (a) : (b))
#endif
/*
 * IOCTL commands specific to this device.
 */
#define CCD_CTRL_CMD_WRITE_SERIAL   0x0010
#define CCD_CTRL_CMD_READ_SERIAL    0x0011
#define CCD_CTRL_CMD_SET_SERIAL     0x0012
#define CCD_CTRL_CMD_GET_SERIAL     0x0013
#define CCD_CTRL_CMD_WRITE_EEPROM_ADDR 0x0014
#define CCD_CTRL_CMD_WRITE_EEPROM_DATA 0x0015
#define CCD_CTRL_CMD_READ_EEPROM    0x0016
/*
 * State flag values.
 */
#define EZUSB_STATE_QUEUED          0x0001
#define EZUSB_STATE_DOWNLOADING     0x0002
#define SX_STATE_CONNECTED          0x0001
#define SX_STATE_CCD_OPEN           0x0002
#define SX_STATE_SND                0x0010
#define SX_STATE_RCV                0x0020
#define SX_STATE_CTRL               0x0040
#define SX_STATE_BUSY               (SX_STATE_SND|SX_STATE_RCV|SX_STATE_CTRL)
/*
 * Vendor specific control commands.
 */
#define SX_USB_ECHO                 0
#define SX_USB_CLEAR_PIXELS         1
#define SX_USB_READ_PIXELS_DELAYED  2
#define SX_USB_READ_PIXELS          3
#define SX_USB_SET_TIMER            4
#define SX_USB_GET_TIMER            5
#define SX_USB_RESET                6
#define SX_USB_SET_CCD              7
#define SX_USB_GET_CCD              8
#define SX_USB_SET_GUIDE_PORT       9
#define SX_USB_WRITE_SERIAL_PORT    10
#define SX_USB_READ_SERIAL_PORT     11
#define SX_USB_SET_SERIAL           12
#define SX_USB_GET_SERIAL           13
#define SX_USB_CAMERA_MODEL         14
#define SX_USB_LOAD_EEPROM          15
/*
 * Caps bit definitions.
 */
#define SX_USB_CAPS_STAR2K          0x01
#define SX_USB_CAPS_COMPRESS        0x02
#define SX_USB_CAPS_EEPROM          0x04
#define SX_USB_CAPS_GUIDER          0x08
/*
 * CCD model parameters.
 */
#define HX5                         -5
#define HX9                         -9
#define MX5                         5
#define MX7                         7
#define MX9                         9
#define HX_IMAGE_FIELDS             1
#define MX_IMAGE_FIELDS             2
/*
 * USB bulk block size to read at a time.
 * Must be multiple of bulk transfer buffer size.
 */
#define SX_BULK_READ_SIZE           (1L<<13)
/*
 * Minimum time (msec) before letting the camera time the exposure.
 */
#define SX_MIN_EXP_TIME             1000
/*
 * Device structures.
 */
struct ezusb_device_t
{
    volatile unsigned int state_flags;
    struct usb_device    *usbdev;
};
struct sx_frame_t
{
    unsigned int xoffset;
    unsigned int yoffset;
    unsigned int width;
    unsigned int height;
    unsigned int xbin;
    unsigned int ybin;
    unsigned int flags;
    unsigned int msec;
    unsigned int row_size;
    unsigned int row_count;
};
typedef struct
{
    __u16   xoffset;
    __u16   yoffset;
    __u16   width;
    __u16   height;
    __u8    xbin;
    __u8    ybin;
} ccdload __attribute__ ((packed));
typedef struct
{
    __u16   xoffset;
    __u16   yoffset;
    __u16   width;
    __u16   height;
    __u8    xbin;
    __u8    ybin;
    __u16   delay_lo;
    __u16   delay_hi;
} ccdloaddelay __attribute__ ((packed));

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,20))
typedef struct usb_ctrlrequest devrequest;
#endif

struct sx_device_t
{
    int                   in_use;
    int                   num;
    volatile unsigned int state_flags;
    spinlock_t            lock;
    char                  id_string[CCD_CCD_NAME_LEN + 1];
    unsigned int          width, height;
    unsigned int          pix_width, pix_height;
    unsigned int          hfront_porch, hback_porch;
    unsigned int          vfront_porch, vback_porch;
    unsigned int          ser_ports;
    unsigned int          caps;
    struct sx_frame_t     frame[2];
    unsigned char        *rcv_buf;
    unsigned int          pixel_size;
    unsigned char        *pixel_buf;
    unsigned int          read_offset;
    volatile unsigned int bytes_rcved;
    volatile unsigned int bytes_remaining;
    unsigned int          xfer_size;
    unsigned int          read_size;
    devrequest            ctrl_setup;
    ccdloaddelay          ctrl_load_params;
    struct urb            ctrl_urb;
    int                   rcv_endpoint;
    unsigned int          rcv_data_size;
    struct urb            rcv_urb;
    int                   snd_endpoint;
    unsigned int          snd_data_size;
    struct urb            snd_urb;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,17))
    wait_queue_head_t     read_wait;
#else
    struct wait_queue    *read_wait;
#endif
    struct usb_device    *usbdev;
    struct tty_struct    *tty_ser[2];   // 8051 serial ports
    struct tty_struct    *tty_s2k;      // STAR2000 port
};

/***************************************************************************\
*                                                                           *
*                                 Global data                               *
*                                                                           *
\***************************************************************************/

#define NR_SX  3
static struct ezusb_device_t ezusb_device;
static struct sx_device_t   *sx_devices;
/*
 * Optional parameters.
 */
static int model  = MX5;
static int color  = 0;
static int serial = 1;
static int force  = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))
MODULE_AUTHOR("David Schmenk, dschmenk@earthlink.net");
MODULE_DESCRIPTION("Starlight Xpress USB astronomy camera driver v1.8a");
MODULE_LICENSE("GPL");
MODULE_PARM_DESC(model, "SX camera model");
MODULE_PARM_DESC(color, "SX camera one-shot color flag");
MODULE_PARM_DESC(serial,"SX camera serial port support flag");
MODULE_PARM_DESC(poll,  "SX camera serial port read poll in Hz");
MODULE_PARM_DESC(force, "SX camera force model update");
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))  //was 2,4,20 brent@mbari.org
/*
 * Table of devices that work with this driver.
 */
static struct usb_device_id sx_usb_id_table[] =
{
    {USB_DEVICE(EZUSB_VENDOR_ID,  EZUSB_PRODUCT_ID)},
    {USB_DEVICE(EZUSB2_VENDOR_ID, EZUSB2_PRODUCT_ID)},
    {USB_DEVICE(ECHO2_VENDOR_ID,  ECHO2_PRODUCT_ID)},
    {USB_DEVICE(ECHO2a_VENDOR_ID, ECHO2a_PRODUCT_ID)},
    {}
};
MODULE_DEVICE_TABLE(usb, sx_usb_id_table);
#endif
#endif
MODULE_PARM(model,   "1-" __MODULE_STRING(NR_SX) "i");
MODULE_PARM(color,   "1-" __MODULE_STRING(NR_SX) "i");
MODULE_PARM(serial,  "1-" __MODULE_STRING(NR_SX) "i");
MODULE_PARM(poll,    "1-" __MODULE_STRING(NR_SX) "i");
MODULE_PARM(force,   "1-" __MODULE_STRING(NR_SX) "i");
/*
 * Alloc and free device structure.
 */
static struct sx_device_t *sx_new_struct(void)
{
    int i;

    for (i = 0; i < NR_SX; i++)
    {
        if (!sx_devices[i].in_use)
        {
            memset(&sx_devices[i], 0, sizeof(struct sx_device_t));
            sx_devices[i].in_use = 1;
            return (&sx_devices[i]);
        }
    }
    return (NULL);
}
static void sx_free_struct(struct sx_device_t *sx)
{
    sx->in_use = 0;
}
/*
 * Safe access to camera.
 */
static unsigned int sx_open_acquire(struct sx_device_t *sx, unsigned int new_flags, int timeout)
{
    unsigned long cpu_flags, acquired = 0;

    spin_lock_irqsave(&sx->lock, cpu_flags);
    while ((sx->state_flags & new_flags)
        && (sx->state_flags & SX_STATE_CONNECTED)
        && (sx->state_flags & SX_STATE_CCD_OPEN)
        && timeout--)
    {
        spin_unlock_irqrestore(&sx->lock, cpu_flags);
        current->state = TASK_INTERRUPTIBLE;
        schedule_timeout(1);
        spin_lock_irqsave(&sx->lock, cpu_flags);
    }
    if (!(sx->state_flags & new_flags) && (sx->state_flags & SX_STATE_CONNECTED) && (sx->state_flags & SX_STATE_CCD_OPEN))
    {
        sx->state_flags |= new_flags;
        acquired         = 1;
    }
    spin_unlock_irqrestore(&sx->lock, cpu_flags);
    return (acquired);
}
static unsigned int sx_acquire(struct sx_device_t *sx, unsigned int new_flags, int timeout)
{
    unsigned long cpu_flags, acquired = 0;

    spin_lock_irqsave(&sx->lock, cpu_flags);
    while ((sx->state_flags & new_flags)
        && (sx->state_flags & SX_STATE_CONNECTED)
        && timeout--)
    {
        spin_unlock_irqrestore(&sx->lock, cpu_flags);
        current->state = TASK_INTERRUPTIBLE;
        schedule_timeout(1);
        spin_lock_irqsave(&sx->lock, cpu_flags);
    }
    if (!(sx->state_flags & new_flags) && (sx->state_flags & SX_STATE_CONNECTED))
    {
        sx->state_flags |= new_flags;
        acquired         = 1;
    }
    spin_unlock_irqrestore(&sx->lock, cpu_flags);
    return (acquired);
}
static void sx_release(struct sx_device_t *sx, unsigned int mask_flags)
{
    unsigned long cpu_flags;

    spin_lock_irqsave(&sx->lock, cpu_flags);
    sx->state_flags &= ~mask_flags;
    spin_unlock_irqrestore(&sx->lock, cpu_flags);
}
/*
 * Reset camera and outstanding URBs.
 */
static void sx_reset(struct sx_device_t *sx)
{
#if 0
    unsigned long       cpu_flags;
    unsigned char       setup_data[8];
    int                 len;
    if (sx->state_flags & SX_STATE_CONNECTED)
    {
        setup_data[0] = USB_TYPE_VENDOR | USB_DIR_OUT;
        setup_data[1] = SX_USB_RESET;
        setup_data[2] = 0x00;
        setup_data[3] = 0x00;
        setup_data[4] = 0x00;
        setup_data[5] = 0x00;
        setup_data[6] = 0x00;
        setup_data[7] = 0x00;
        usb_bulk_msg(sx->usbdev, usb_sndbulkpipe(sx->usbdev, sx->snd_endpoint), setup_data, 8, &len, 1*HZ);
    }
    if (sx->state_flags & SX_STATE_RCV)
    {
        /*
         * Abort current transfer.
         */
        spin_lock_irqsave(&sx->lock, cpu_flags);
        sx->bytes_rcved    += sx->bytes_remaining;
        sx->bytes_remaining = 0;
        sx->state_flags    &= ~SX_STATE_RCV;
        spin_unlock_irqrestore(&sx->lock, cpu_flags);
//        usb_unlink_urb(sx->rcv_urb);
        wake_up_interruptible(&sx->read_wait);
        printk(KERN_ERR "%s: aborting RCV during reset!\n", sx->id_string);
    }
    if (sx->state_flags & SX_STATE_SND)
    {
        /*
         * Abort current send request.
         */
        spin_lock_irqsave(&sx->lock, cpu_flags);
        sx->state_flags &= ~SX_STATE_SND;
        spin_unlock_irqrestore(&sx->lock, cpu_flags);
//        usb_unlink_urb(sx->snd_urb);
        printk(KERN_ERR "%s: aborting SND during reset!\n", sx->id_string);
    }
#else
    if (sx->state_flags & SX_STATE_RCV)
    {
        //
        // Wait for data to download.
        //
        while ((sx->state_flags & SX_STATE_RCV) && (sx->state_flags & SX_STATE_CONNECTED))
        {
            current->state = TASK_INTERRUPTIBLE;
            schedule_timeout(HZ/4);
        }
    }
    if (sx->state_flags & SX_STATE_SND)
    {
        //
        // Wait while data being sent.
        //
        while ((sx->state_flags & SX_STATE_SND) && (sx->state_flags & SX_STATE_CONNECTED))
        {
            current->state = TASK_INTERRUPTIBLE;
            schedule_timeout(HZ/4);
        }
    }
#endif
}
/***************************************************************************\
*                                                                           *
*                          Basic SX camera functions                        *
*                                                                           *
\***************************************************************************/

static void sx_usb_dbg(struct usb_device *usbdev);
static unsigned char *sx_read_pixels(struct sx_device_t *sx, unsigned int bytes)
{
    if (sx->read_offset + bytes > sx->bytes_rcved + sx->bytes_remaining)
    {
        printk(KERN_ERR "%s: requesting pixels beyond end of buffer\n", sx->id_string);
        return (sx->pixel_buf);
    }
    while ((sx->read_offset + bytes > sx->bytes_rcved) && (sx->state_flags & SX_STATE_CONNECTED))
        interruptible_sleep_on(&sx->read_wait);
    sx->read_offset += bytes;
    return (sx->pixel_buf + sx->read_offset - bytes);
}
/*
 * Does windowing and binning.
 */
static int sx_read_row(void *vp, unsigned int offset, unsigned int row, unsigned int width, unsigned int xbin, unsigned int ybin, unsigned int dac_bits, unsigned int flags, unsigned char *buf)
{
    struct sx_device_t *sx        = (struct sx_device_t *)vp;
    int                 row_size = (width / xbin) * sx->pixel_size;
    /*
     * Copy data into driver read buffer.
     */
    {
        /*
         * Move the raw pixels over.
         */
        buf[row_size-1] = 0; // I think (know) there is a bug in the 3dnow memcpy that doesn't always copy the last few words unless they are touched first.
        memcpy(buf, sx_read_pixels(sx, row_size), row_size);
    }
    return (row_size);
}
/*
 * Begin reading frame.
 */
static void sx_begin_read(void *vp, unsigned int offset, unsigned int flags)
{
    struct sx_device_t *sx = (struct sx_device_t *)vp;

    sx = sx;
}
/*
 * Complete reading a frame.
 */
static void sx_end_read(void *vp, unsigned int flags)
{
    struct sx_device_t *sx = (struct sx_device_t *)vp;

    sx->bytes_remaining = 0;
    sx_release(sx, SX_STATE_RCV);
}
/*
 * Latch a frame and prepare for reading.
 */
static void sx_latch_frame(void *vp, unsigned int flags)
{
    struct sx_device_t *sx = (struct sx_device_t *)vp;

    if ((sx->frame[flags & CCD_FIELD_ODD].msec >= SX_MIN_EXP_TIME) || sx->num || ((flags & CCD_NOCLEAR_FRAME) && !(flags & CCD_TDI)))
    {
        if (!sx_open_acquire(sx, SX_STATE_SND | SX_STATE_RCV, HZ/2))
        {
            printk(KERN_ERR "%s: unable to acquire in latch_frame!\n", sx->id_string);
            return;
        }
        /*
         * Prepare for reading pixels.
         */
        sx->bytes_rcved                    = 0;
        sx->read_offset                    = 0;
        sx->bytes_remaining                = sx->frame[flags & CCD_FIELD_ODD].row_size
                                           * sx->frame[flags & CCD_FIELD_ODD].row_count;
        sx->rcv_urb.transfer_buffer        = sx->rcv_buf;
        sx->rcv_urb.transfer_buffer_length = min((unsigned int)SX_BULK_READ_SIZE, (unsigned int)sx->bytes_remaining);
        sx->rcv_urb.dev                    = sx->usbdev;
        /*
         * Send read pixels command.
         */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,20))
        sx->ctrl_setup.bRequestType         = USB_TYPE_VENDOR | USB_DIR_OUT;
        sx->ctrl_setup.bRequest             = SX_USB_READ_PIXELS;
        sx->ctrl_setup.wValue               = flags | (sx->frame[flags & CCD_FIELD_ODD].msec < 2000 ? CCD_NOCLEAR_FRAME : 0);
        sx->ctrl_setup.wIndex               = sx->num;
        sx->ctrl_setup.wLength              = sizeof(ccdload);
#else
        sx->ctrl_setup.requesttype         = USB_TYPE_VENDOR | USB_DIR_OUT;
        sx->ctrl_setup.request             = SX_USB_READ_PIXELS;
        sx->ctrl_setup.value               = flags | (sx->frame[flags & CCD_FIELD_ODD].msec < 2000 ? CCD_NOCLEAR_FRAME : 0);
        sx->ctrl_setup.index               = sx->num;
        sx->ctrl_setup.length              = sizeof(ccdload);
#endif
        sx->ctrl_load_params.xoffset       = sx->frame[flags & CCD_FIELD_ODD].xoffset;
        sx->ctrl_load_params.yoffset       = sx->frame[flags & CCD_FIELD_ODD].yoffset;
        sx->ctrl_load_params.width         = sx->frame[flags & CCD_FIELD_ODD].width;
        sx->ctrl_load_params.height        = sx->frame[flags & CCD_FIELD_ODD].height;
        sx->ctrl_load_params.xbin          = sx->frame[flags & CCD_FIELD_ODD].xbin;
        sx->ctrl_load_params.ybin          = sx->frame[flags & CCD_FIELD_ODD].ybin;
        /*
         * Submit the URB.
         */
        sx->snd_urb.dev = sx->usbdev;
        usb_submit_urb(&sx->snd_urb);
    }
}
/*
 * Clear frame and start integrating.
 */
static void sx_new_frame(void *vp, unsigned int xoffset, unsigned int yoffset, unsigned int width, unsigned int height, unsigned int xbin, unsigned int ybin, unsigned int dac_bits, unsigned int msec, unsigned int flags)
{
    unsigned char       setup_data[8];
    int                 len;
    struct sx_device_t *sx = (struct sx_device_t *)vp;

    /*
     * Save frame parameters for latch_frame function.
     */
    sx->frame[flags & CCD_FIELD_ODD].xoffset   = xoffset;
    sx->frame[flags & CCD_FIELD_ODD].yoffset   = yoffset;
    sx->frame[flags & CCD_FIELD_ODD].width     = width;
    sx->frame[flags & CCD_FIELD_ODD].height    = height;
    sx->frame[flags & CCD_FIELD_ODD].xbin      = xbin;
    sx->frame[flags & CCD_FIELD_ODD].ybin      = ybin;
    sx->frame[flags & CCD_FIELD_ODD].msec      = msec;
    sx->frame[flags & CCD_FIELD_ODD].row_count = height / ybin;
    sx->frame[flags & CCD_FIELD_ODD].row_size  = (width / xbin) * sx->pixel_size;
    if ((msec < SX_MIN_EXP_TIME) && (sx->num == 0) && (!(flags & CCD_NOCLEAR_FRAME) || (flags & CCD_TDI)))
    {
        if (!sx_open_acquire(sx, SX_STATE_SND | SX_STATE_RCV, HZ/2))
        {
            printk(KERN_ERR "%s: unable to acquire in read_pixels_delayed!\n", sx->id_string);
            return;
        }
        /*
         * Prepare for reading pixels. A clear frame operation is part of READ_PIXELS_DELAYED.
         */
        sx->bytes_rcved                    = 0;
        sx->read_offset                    = 0;
        sx->bytes_remaining                = sx->frame[flags & CCD_FIELD_ODD].row_size
                                           * sx->frame[flags & CCD_FIELD_ODD].row_count;
        sx->rcv_urb.transfer_buffer        = sx->rcv_buf;
        sx->rcv_urb.transfer_buffer_length = min((unsigned int)SX_BULK_READ_SIZE, (unsigned int)sx->bytes_remaining);
        sx->rcv_urb.dev                    = sx->usbdev;
        /*
         * Send read pixels command.
         */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,20))
        sx->ctrl_setup.bRequestType         = USB_TYPE_VENDOR | USB_DIR_OUT;
        sx->ctrl_setup.bRequest             = SX_USB_READ_PIXELS_DELAYED;
        sx->ctrl_setup.wValue               = flags;
        sx->ctrl_setup.wIndex               = sx->num;
        sx->ctrl_setup.wLength              = sizeof(ccdloaddelay);
#else
        sx->ctrl_setup.requesttype         = USB_TYPE_VENDOR | USB_DIR_OUT;
        sx->ctrl_setup.request             = SX_USB_READ_PIXELS_DELAYED;
        sx->ctrl_setup.value               = flags;
        sx->ctrl_setup.index               = sx->num;
        sx->ctrl_setup.length              = sizeof(ccdloaddelay);
#endif
        sx->ctrl_load_params.xoffset       = xoffset;
        sx->ctrl_load_params.yoffset       = yoffset;
        sx->ctrl_load_params.width         = width;
        sx->ctrl_load_params.height        = height;
        sx->ctrl_load_params.xbin          = xbin;
        sx->ctrl_load_params.ybin          = ybin;
        sx->ctrl_load_params.delay_lo      = msec;
        sx->ctrl_load_params.delay_hi      = msec >> 16;
        /*
         * Submit the URB.
         */
        sx->snd_urb.dev = sx->usbdev;
        usb_submit_urb(&sx->snd_urb);
    }
    else
    {
        /*
         * Clear pixels if not in TDI or NOCLEAR mode.
         */
        if (!(flags & (CCD_TDI | CCD_NOCLEAR_FRAME)))
        {
            if (sx_open_acquire(sx, SX_STATE_SND, HZ/2))
            {
                setup_data[0] = USB_TYPE_VENDOR | USB_DIR_OUT;
                setup_data[1] = SX_USB_CLEAR_PIXELS;
                setup_data[2] = flags;
                setup_data[3] = flags >> 8;
                setup_data[4] = sx->num;
                setup_data[5] = 0x00;
                setup_data[6] = 0x00;
                setup_data[7] = 0x00;
                if (usb_bulk_msg(sx->usbdev, usb_sndbulkpipe(sx->usbdev, sx->snd_endpoint), setup_data, 8, &len, 1*HZ))
                {
                    printk(KERN_ERR "%s: clear pixels failed\n", sx->id_string);
                    sx_usb_dbg(sx->usbdev);
                }
                sx_release(sx, SX_STATE_SND);
            }
            else
            {
                printk(KERN_ERR "%s: unable to acquire in clear_pixels!\n", sx->id_string);
            }
        }
    }
}

/***************************************************************************\
*                                                                           *
*                  CCD open, close and control functions                    *
*                                                                           *
\***************************************************************************/

/*
 * Prepare to use CCD device.
 */
static int sx_open(void *vp)
{
    int                 err = 0;
    struct sx_device_t *sx  = (struct sx_device_t *)vp;

    if ((sx->state_flags & SX_STATE_CCD_OPEN) || !sx_acquire(sx, SX_STATE_SND | SX_STATE_RCV | SX_STATE_CCD_OPEN, 1*HZ))
    {
        printk(KERN_ERR "%s: unable to acquire camera during open! Flags = 0x%08X\n", sx->id_string, sx->state_flags);
        err = -EBUSY;
    }
    else
    {
        sx_release(sx, SX_STATE_SND | SX_STATE_RCV);
        sx_reset(sx);
        MOD_INC_USE_COUNT;
    }
    return (err);
}
/*
 * Control CCD device. This is device specific, stuff like temperature control, serial port IO, etc.
 */
static int sx_control(void *vp, unsigned short cmd, unsigned long param)
{
    static int          eeprom_addr = 0;
    struct sx_device_t *sx       = (struct sx_device_t *)vp;
    unsigned char      *parambuf = (unsigned char *)param;
    unsigned char       data[64];
    int                 len, retval = 0;

    if (sx_open_acquire(sx, SX_STATE_CTRL, HZ/2))
    {
        switch (cmd)
        {
            case CCD_CTRL_CMD_WRITE_SERIAL:
                for (len = 0; len < parambuf[0]; len++)
                    data[len] = parambuf[len + 1];
                usb_control_msg(sx->usbdev, usb_sndctrlpipe(sx->usbdev, 0), SX_USB_WRITE_SERIAL_PORT, USB_TYPE_VENDOR | USB_DIR_OUT, 0, 0, data, len, 1*HZ);
                break;
            case CCD_CTRL_CMD_READ_SERIAL:
                len = usb_control_msg(sx->usbdev, usb_rcvctrlpipe(sx->usbdev, 0), SX_USB_READ_SERIAL_PORT, USB_TYPE_VENDOR | USB_DIR_OUT, 1, 1, data, 64, 1*HZ);
                *parambuf++ = len;
                while (len--)
                    parambuf[len] = data[len];
                break;
            case CCD_CTRL_CMD_SET_SERIAL:
                break;
            case CCD_CTRL_CMD_GET_SERIAL:
                usb_control_msg(sx->usbdev, usb_rcvctrlpipe(sx->usbdev, 0), SX_USB_GET_SERIAL, USB_TYPE_VENDOR | USB_DIR_OUT, parambuf[0], 0, data, 2, 1*HZ);
                parambuf[1] = data[0];
                break;
            case CCD_CTRL_CMD_WRITE_EEPROM_ADDR:
                eeprom_addr = param;
                break;
            case CCD_CTRL_CMD_WRITE_EEPROM_DATA:
                data[0] = param;
                usb_control_msg(sx->usbdev, usb_sndctrlpipe(sx->usbdev, 0), SX_USB_LOAD_EEPROM, USB_TYPE_VENDOR | USB_DIR_OUT, eeprom_addr++, 0, data, 1, 1*HZ);
                break;
            case CCD_CTRL_CMD_READ_EEPROM:
                usb_control_msg(sx->usbdev, usb_rcvctrlpipe(sx->usbdev, 0), SX_USB_LOAD_EEPROM, USB_TYPE_VENDOR | USB_DIR_IN, param, 0, data, 1, 1*HZ);
                retval = data[0];
                break;
            case CCD_CTRL_CMD_GET_CFW:
                parambuf[0] = 0xFF;
                parambuf[1] = 0x00;
                break;
            case CCD_CTRL_CMD_SET_CFW:
                break;
            case CCD_CTRL_CMD_RESET:
                sx_reset(sx);
                break;
            default:
                break;
        }
        sx_release(sx, SX_STATE_CTRL);
    }
    return (retval);
}
/*
 * Release CCD device.
 */
static int sx_close(void *vp)
{
    struct sx_device_t *sx = (struct sx_device_t *)vp;

    if (sx_open_acquire(sx, SX_STATE_SND | SX_STATE_RCV, 5*HZ))
    {
        /*
         * Clear open state flag.
         */
        sx_release(sx, SX_STATE_SND | SX_STATE_RCV | SX_STATE_CCD_OPEN);
    }
    else
    {
        unsigned long cpu_flags;

        /*
         * Force closure and respect the open flag all over the driver.
         */
        printk(KERN_ERR "%s: unable to acquire camera during close!\n", sx->id_string);
        sx_reset(sx);
        /*
         * Check for disconnection.
         */
        if (!(sx->state_flags & SX_STATE_CONNECTED))
        {
            ccd_unregister_device((void *)sx);
            if (sx->rcv_buf)
            {
                kfree(sx->rcv_buf);
                sx->rcv_buf = NULL;
            }
            if (sx->pixel_buf)
            {
                vfree(sx->pixel_buf);
                sx->pixel_buf = NULL;
            }
            sx->usbdev = NULL;
            sx_free_struct(sx);
        }
        spin_lock_irqsave(&sx->lock, cpu_flags);
        sx->state_flags &= ~(SX_STATE_SND | SX_STATE_RCV | SX_STATE_CCD_OPEN);
        spin_unlock_irqrestore(&sx->lock, cpu_flags);
    }
    MOD_DEC_USE_COUNT;
    return (0);
}

/***************************************************************************\
*                                                                           *
*                       Simple serial port functions                        *
*                                                                           *
\***************************************************************************/

#define SX_TTY_SER_PORT0    0
#define SX_TTY_SER_PORT1    1
#define SX_TTY_SER_PORT2    2
#define SX_TTY_S2K_PORT     3
#define GET_OUPUT_FREE      0
#define GET_INPUT_AVAIL     1
/*
 * TTY device data.
 */
static struct tty_struct *sx_tty_table[NR_SX*4];
static struct termios    *sx_tty_termios[NR_SX*4];
static struct termios    *sx_tty_termios_locked[NR_SX*4];
static int                sx_tty_refcount;
static int                sx_tty_ser_opened;
static struct tq_struct   do_sx_tty_poll;
static struct timer_list  do_sx_tty_timer;
static int                poll = 1;
/*
 * Check for available input.
 */
static void sx_tty_check_input(void *param)
{
    unsigned char data[64];
    int           i, j, c, len, next_poll = poll;
    if (sx_tty_ser_opened)
    {
        for (i = 0; i < NR_SX; i++)
        {
            if ((sx_devices[i].tty_ser[0] || sx_devices[i].tty_ser[1]) && !(sx_devices[i].state_flags & SX_STATE_RCV) && sx_acquire(&sx_devices[i], SX_STATE_CTRL, 0))
            {
                for (j = 0; j < 2; j++) // Max of two serial ports per camera
                {
                    if ((len = usb_control_msg(sx_devices[i].usbdev, usb_rcvctrlpipe(sx_devices[i].usbdev, 0), SX_USB_GET_SERIAL, USB_TYPE_VENDOR | USB_DIR_IN, GET_INPUT_AVAIL, j, data, 2, 1*HZ)) != 2)
                    {
                        printk(KERN_ERR "%s: get_serial ctl failed, returned %d\n", sx_devices[i].id_string, len);
                        sx_release(&sx_devices[i], SX_STATE_CTRL);
                        return;
                    }
                    len = data[0] | (data[1] << 8);
                    if (len)
                    {
                        /*
                         * Read available data.
                         */
                        len = min(63, len);
                        if ((c = usb_control_msg(sx_devices[i].usbdev, usb_rcvctrlpipe(sx_devices[i].usbdev, 0), SX_USB_READ_SERIAL_PORT, USB_TYPE_VENDOR | USB_DIR_IN, 0, j, data, len, 1*HZ)) < len)
                        {
                            printk(KERN_ERR "%s: read_serial ctl failed, returned %d\n", sx_devices[i].id_string, c);
                            sx_release(&sx_devices[i], SX_STATE_CTRL);
                            return;
                        }
                        /*
                         * Add to TTY input buffer.
                         */
                        for (c = 0; c < len; c++)
                        {
                            if (sx_devices[i].tty_ser[j]->flip.count >= TTY_FLIPBUF_SIZE)
                                tty_flip_buffer_push(sx_devices[i].tty_ser[j]);
                            tty_insert_flip_char(sx_devices[i].tty_ser[j], data[c], 0);
                        }
                        tty_flip_buffer_push(sx_devices[i].tty_ser[j]);
                        /*
                         * Poll again real soon in case more data is available and get the buffer flipped.
                         */
                        next_poll = HZ / 50;
                    }
                }
                sx_release(&sx_devices[i], SX_STATE_CTRL);
            }
        }
        /*
         * Start the read characters timer.
         */
        init_timer(&do_sx_tty_timer);
        do_sx_tty_timer.expires = jiffies + next_poll;
        add_timer(&do_sx_tty_timer);
    }
}
static void sx_tty_timer(unsigned long param)
{
    if (sx_tty_ser_opened)
    {
        do_sx_tty_poll.sync    = 0;
        do_sx_tty_poll.routine = sx_tty_check_input;
        do_sx_tty_poll.data    = NULL;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))
        schedule_task(&do_sx_tty_poll);
#else
        do_sx_tty_poll.next    = NULL;
        queue_task(&do_sx_tty_poll, &tq_scheduler);
#endif
    }
}
/*
 * Write characters to serial port.
 */
static int sx_tty_write_room(struct tty_struct *tty)
{
    return (64);
}
static int sx_tty_chars_in_buffer(struct tty_struct *tty)
{
    return (0);
}
static int sx_tty_write(struct tty_struct *tty, int from_user, const unsigned char *buf, int count)
{
    unsigned char       data[64];
    int                 written;
    int                 port = (MINOR(tty->device) - tty->driver.minor_start) & 3;
    struct sx_device_t *sx   = (struct sx_device_t *)tty->driver_data;
    if (sx_acquire(sx, SX_STATE_CTRL, HZ/10) && count)
    {
        if (from_user)
        {
            written = min(count, 64);
            copy_from_user(data, buf, written);
        }
        else
        {
            for (written = 0; written < count && written < 64; written++)
                data[written] = buf[written];
        }
        /*
         * Check if serial port or STAR2000 port.
         */
        if (port == SX_TTY_S2K_PORT)
        {
            for (count = 0; count < written; count++)
            {
                if (usb_control_msg(sx->usbdev, usb_sndctrlpipe(sx->usbdev, 0), SX_USB_SET_GUIDE_PORT, USB_TYPE_VENDOR | USB_DIR_OUT, data[count], 0, NULL, 0, 1*HZ) < 0)
                {
                    printk(KERN_ERR "%s: set_guide_port failed\n", sx->id_string);
                    written = count;
                    break;
                }
            }
        }
        else
        {
            if (usb_control_msg(sx->usbdev, usb_sndctrlpipe(sx->usbdev, 0), SX_USB_WRITE_SERIAL_PORT, USB_TYPE_VENDOR | USB_DIR_OUT, 1, port, data, written, 1*HZ) != written)
            {
                printk(KERN_ERR "%s: write_serial_port failed\n", sx->id_string);
                written = count;
            }
        }
        sx_release(sx, SX_STATE_CTRL);
    }
    else if (sx->state_flags & SX_STATE_CONNECTED)
    {
        printk(KERN_ERR "%s: unable to acquire in tty_write\n", sx->id_string);
        written = 0;
    }
    else
        written = count;
    if (written)
    {
        if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) && tty->ldisc.write_wakeup)
            tty->ldisc.write_wakeup(tty);
        wake_up_interruptible(&tty->write_wait);
    }
    return (written);
}
/*
 * TTY open and close and control.
 */
static int sx_tty_open(struct tty_struct *tty, struct file *filp)
{
    int line = MINOR(tty->device) - tty->driver.minor_start;
    int dev  = line >> 2;
    int port = line & 3;
        if ((line < 0) || (dev >= NR_SX))
                return (-ENODEV);
    if ((port == SX_TTY_S2K_PORT) && (sx_devices[dev].caps & SX_USB_CAPS_STAR2K))
    {
        if (sx_devices[dev].tty_s2k)
            return (-EBUSY);
        sx_devices[dev].tty_s2k = tty;
    }
    else if (port > sx_devices[dev].ser_ports)
    {
        return (-ENODEV); // Only serial ports 0 & 1 allowed.
    }
    else
    {
        if (sx_devices[dev].tty_ser[port])
            return (-EBUSY);
        sx_devices[dev].tty_ser[port] = tty;
        /*
         * Trigger serial port polling.
         */
        if (!sx_tty_ser_opened++)
        {
            /*
             * Start the read characters timer.
             */
            init_timer(&do_sx_tty_timer);
            do_sx_tty_timer.function = sx_tty_timer;
            do_sx_tty_timer.data     = 0;
            do_sx_tty_timer.expires  = jiffies + poll;
            add_timer(&do_sx_tty_timer);
        }
    }
    tty->low_latency = 1;
    tty->driver_data = &sx_devices[dev];
    MOD_INC_USE_COUNT;
    return (0);
}
static int sx_tty_ioctl(struct tty_struct *tty, struct file *file, unsigned int cmd, unsigned long arg)
{
    return (-ENOIOCTLCMD);
}
static void sx_tty_close(struct tty_struct *tty, struct file *filp)
{
    int                 port = (MINOR(tty->device) - tty->driver.minor_start) & 3;
    struct sx_device_t *sx   = (struct sx_device_t *)tty->driver_data;
    if (sx)
    {
        if (port == SX_TTY_S2K_PORT)
        {
            sx->tty_s2k = NULL;
        }
        else
        {
            sx->tty_ser[port] = NULL;
            if (!--sx_tty_ser_opened)
                del_timer(&do_sx_tty_timer);
        }
        MOD_DEC_USE_COUNT;
    }
}

/***************************************************************************\
*                                                                           *
*                              USB URB handlers                             *
*                                                                           *
\***************************************************************************/

static void sx_usb_dbg(struct usb_device *usbdev)
{
    unsigned char data[16];
    unsigned int  ezusb_dbg_buf;

    if (usbdev->descriptor.idVendor  == EZUSB_VENDOR_ID  && usbdev->descriptor.idProduct == EZUSB_PRODUCT_ID)
    {
        ezusb_dbg_buf = SX_EZUSB_DEBUG_BUF;
    }
    else //if (usbdev->descriptor.idVendor  == EZUSB2_VENDOR_ID && usbdev->descriptor.idProduct == EZUSB2_PRODUCT_ID)
    {
        ezusb_dbg_buf = SX_EZUSB2_DEBUG_BUF;
    }
    if (usb_control_msg(usbdev, usb_rcvctrlpipe(usbdev, 0), EZUSB_FIRMWARE_LOAD, USB_TYPE_VENDOR | USB_DIR_IN, ezusb_dbg_buf, 0, data, 10, 1*HZ) != 10)
        printk(KERN_INFO "starlight xpress: unable to download dbg data\n");
    else
        printk(KERN_INFO "starlight xpress: usb debug data: 0x%02X 0x%02X 0x%02X\n", data[0], data[1], data[2]);
}
static void sx_usb_ctrl_complete(struct urb *ctrl_urb)
{
    unsigned long       cpu_flags;
    struct sx_device_t *sx = (struct sx_device_t *)ctrl_urb->context;

    spin_lock_irqsave(&sx->lock, cpu_flags);
    sx->state_flags &= ~SX_STATE_CTRL;
    spin_unlock_irqrestore(&sx->lock, cpu_flags);
}
static void sx_usb_rcv_complete(struct urb *rcv_urb)
{
    unsigned long       cpu_flags;
    struct sx_device_t *sx = (struct sx_device_t *)rcv_urb->context;

    if ((sx->rcv_urb.actual_length == sx->rcv_urb.transfer_buffer_length)
     && (sx->bytes_remaining)
     && (sx->state_flags & SX_STATE_RCV)
     && (sx->state_flags & SX_STATE_CONNECTED))
    {
        memcpy(sx->pixel_buf + sx->bytes_rcved, sx->rcv_buf, sx->rcv_urb.actual_length);
        sx->bytes_rcved     += sx->rcv_urb.actual_length;
        sx->bytes_remaining -= sx->rcv_urb.actual_length;
    }
    else // Some sort of error
    {
        sx->bytes_rcved    += sx->bytes_remaining;
        sx->bytes_remaining = 0;
        if (sx->rcv_urb.transfer_buffer_length != sx->rcv_urb.actual_length)
            printk(KERN_ERR "%s: different transfer length in RCV callback - wanted %d, got %d\n", sx->id_string, sx->rcv_urb.transfer_buffer_length, sx->rcv_urb.actual_length);
        else
            printk(KERN_ERR "%s: DISCONNECT in RCV callback\n", sx->id_string);
    }
    if (sx->bytes_remaining)
    {
        sx->rcv_urb.transfer_buffer        = sx->rcv_buf;
        sx->rcv_urb.transfer_buffer_length = min((unsigned int)SX_BULK_READ_SIZE, (unsigned int)sx->bytes_remaining);
        sx->rcv_urb.dev                    = sx->usbdev;
        usb_submit_urb(&sx->rcv_urb);
    }
    else
    {
        spin_lock_irqsave(&sx->lock, cpu_flags);
        sx->state_flags &= ~SX_STATE_RCV;
        spin_unlock_irqrestore(&sx->lock, cpu_flags);
    }
    wake_up_interruptible(&sx->read_wait);
}
static void sx_usb_snd_complete(struct urb *snd_urb)
{
    unsigned long       cpu_flags;
    struct sx_device_t *sx = (struct sx_device_t *)snd_urb->context;

    spin_lock_irqsave(&sx->lock, cpu_flags);
    sx->state_flags &= ~SX_STATE_SND;
    spin_unlock_irqrestore(&sx->lock, cpu_flags);
    /*
     * Submit read request.
     */
    if ((sx->state_flags & SX_STATE_RCV) && (sx->state_flags & SX_STATE_CONNECTED))
        usb_submit_urb(&sx->rcv_urb);
}

/***************************************************************************\
*                                                                           *
*                            USB probe/disconnect                           *
*                                                                           *
\***************************************************************************/

/*
 * Download code to EZ-USB device.
 */
static struct tq_struct do_ezusb_download;
static void sx_ezusb_download(void *param)
{
    unsigned int                     i, j, ezusb_cpucs_reg, code_rec_count;
    unsigned char                    setup_data[20];
    struct usb_device               *usbdev = (struct usb_device *)param;
    struct sx_ezusb_download_record *code_rec;

    if (usbdev->descriptor.idVendor  == EZUSB_VENDOR_ID  && usbdev->descriptor.idProduct == EZUSB_PRODUCT_ID)
    {
        code_rec        = sx_ezusb_code;
        code_rec_count  = sizeof(sx_ezusb_code)/sizeof(struct sx_ezusb_download_record);
        ezusb_cpucs_reg = EZUSB_CPUCS_REG;
    }
    else //if (usbdev->descriptor.idVendor  == EZUSB2_VENDOR_ID && usbdev->descriptor.idProduct == EZUSB2_PRODUCT_ID)
    {
        code_rec        = sx_ezusb2_code;
        code_rec_count  = sizeof(sx_ezusb2_code)/sizeof(struct sx_ezusb_download_record);
        ezusb_cpucs_reg = EZUSB2_CPUCS_REG;
    }
    ezusb_device.state_flags &= ~EZUSB_STATE_QUEUED;
    if (ezusb_device.state_flags & EZUSB_STATE_DOWNLOADING)
    {
        /*
         * Put 8051 into RESET.
         */
        setup_data[0] = CPUCS_RESET;
        if (usb_control_msg(usbdev, usb_sndctrlpipe(usbdev, 0), EZUSB_FIRMWARE_LOAD, USB_TYPE_VENDOR | USB_DIR_OUT, ezusb_cpucs_reg, 0, setup_data, 1, 1*HZ) != 1)
        {
           printk(KERN_ERR "starlight-xpress: could not put 8051 into RESET\n");
           ezusb_device.state_flags &= ~EZUSB_STATE_DOWNLOADING;
           return;
        }
        printk(KERN_INFO "starlight-xpress: RESET 8051\n");
    }
    if (ezusb_device.state_flags & EZUSB_STATE_DOWNLOADING) printk(KERN_INFO "starlight-xpress: Download %d code records\n", code_rec_count);
    for (i = 0; i < code_rec_count && ezusb_device.state_flags & EZUSB_STATE_DOWNLOADING; i++)
    {
        /*
         * Download code record.
         */
        j = code_rec[i].len;
        memcpy(setup_data, code_rec[i].data, j);
        if (usb_control_msg(usbdev, usb_sndctrlpipe(usbdev, 0), EZUSB_FIRMWARE_LOAD, USB_TYPE_VENDOR | USB_DIR_OUT, code_rec[i].addr, 0, setup_data, j, 1*HZ) != j)
        {
            printk(KERN_ERR "starlight-xpress: could not download code into 8051\n");
            ezusb_device.state_flags &= ~EZUSB_STATE_DOWNLOADING;
            return;
        }
    }
    if (ezusb_device.state_flags & EZUSB_STATE_DOWNLOADING)
    {
        /*
         * Take 8051 out of RESET.
         */
        setup_data[0] = CPUCS_RUN;
        if (usb_control_msg(usbdev, usb_sndctrlpipe(usbdev, 0), EZUSB_FIRMWARE_LOAD, USB_TYPE_VENDOR | USB_DIR_OUT, ezusb_cpucs_reg, 0, setup_data, 1, 1*HZ) != 1)
        {
           printk(KERN_ERR "Could not take 8051 out of RESET\n");
           ezusb_device.state_flags &= ~EZUSB_STATE_DOWNLOADING;
           return;
        }
        printk(KERN_INFO "starlight-xpress: un-RESET 8051\n");
        ezusb_device.state_flags &= ~EZUSB_STATE_DOWNLOADING;
    }
}
/*
 * Probe for device.
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))
static void *sx_usb_probe(struct usb_device *usbdev, unsigned int interface, const struct usb_device_id *id)
#else
static void *sx_usb_probe(struct usb_device *usbdev, unsigned int interface)
#endif
{
    unsigned char setup_data[64];

    printk(KERN_INFO "starlight-xpress: probing usb_device 0x%04x:0x%04x\n", usbdev->descriptor.idVendor, usbdev->descriptor.idProduct);
    if (id) printk(KERN_INFO "starlight-xpress: probing device_id  0x%04x:0x%04x\n", id->idVendor, id->idProduct);
    if ((usbdev->descriptor.idVendor  == EZUSB_VENDOR_ID  && usbdev->descriptor.idProduct == EZUSB_PRODUCT_ID)
     || (usbdev->descriptor.idVendor  == EZUSB2_VENDOR_ID && usbdev->descriptor.idProduct == EZUSB2_PRODUCT_ID))

    {
        printk(KERN_INFO "starlight-xpress: found EZUSB device\n");
        if (usb_set_configuration(usbdev, usbdev->config[0].bConfigurationValue) < 0)
        {
            printk(KERN_ERR "starlight-xpress: set_configuration failed\n");
            return(NULL);
        }
        /*
         * Trigger software download.
         */
        if (!(ezusb_device.state_flags & EZUSB_STATE_QUEUED))
        {
            ezusb_device.state_flags |= EZUSB_STATE_DOWNLOADING | EZUSB_STATE_QUEUED;
            do_ezusb_download.sync    = 0;
            do_ezusb_download.routine = sx_ezusb_download;
            do_ezusb_download.data    = (void *)usbdev;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))
            schedule_task(&do_ezusb_download);
#else
            do_ezusb_download.next    = NULL;
            queue_task(&do_ezusb_download, &tq_scheduler);
#endif
        }
        else
            printk(KERN_INFO "starlight-xpress: download task already scheduled\n");
        MOD_INC_USE_COUNT;
        return (&ezusb_device);
    }
    else if((usbdev->descriptor.idVendor  == ECHO2_VENDOR_ID &&
             usbdev->descriptor.idProduct == ECHO2_PRODUCT_ID) || 
            (usbdev->descriptor.idVendor  == ECHO2a_VENDOR_ID &&
             usbdev->descriptor.idProduct == ECHO2a_PRODUCT_ID))
    {
        struct ccd_mini     mini_device;
        int                 sx_model, sx_color;
        struct sx_device_t *sx = sx_new_struct();
        printk(KERN_INFO "starlight-xpress: found ECHO2 device\n");
        /*
         * Get new device structure.
         */
        if (sx)
        {
            /*
             * Register with CCD class driver.
             */
            if (usb_set_configuration(usbdev, usbdev->config[0].bConfigurationValue) < 0)
            {
                printk(KERN_ERR "starlight-xpress: set_configuration failed\n");
                return (NULL);
            }
            sx->rcv_buf       = NULL;
            sx->pixel_buf     = NULL;
            sx->state_flags   = 0;
            sx->snd_endpoint  = usbdev->config[0].interface[0].altsetting[usbdev->config[0].interface[0].act_altsetting].endpoint[1].bEndpointAddress;
            sx->snd_data_size = usbdev->config[0].interface[0].altsetting[usbdev->config[0].interface[0].act_altsetting].endpoint[1].wMaxPacketSize;
            sx->rcv_endpoint  = usbdev->config[0].interface[0].altsetting[usbdev->config[0].interface[0].act_altsetting].endpoint[0].bEndpointAddress;
            sx->rcv_data_size = usbdev->config[0].interface[0].altsetting[usbdev->config[0].interface[0].act_altsetting].endpoint[0].wMaxPacketSize;
            sx->usbdev        = usbdev;
            spin_lock_init(&sx->lock);
            spin_lock_init(&sx->ctrl_urb.lock);
            spin_lock_init(&sx->snd_urb.lock);
            spin_lock_init(&sx->rcv_urb.lock);
            init_waitqueue_head(&(sx->read_wait));
            FILL_CONTROL_URB_TO(&(sx->ctrl_urb),
                                  sx->usbdev,
                                  usb_sndctrlpipe(sx->usbdev, 0),
                                  (unsigned char *)&(sx->ctrl_setup),
                                  &(sx->ctrl_load_params),
                                  sizeof(ccdload),
                                  sx_usb_ctrl_complete,
                                  (void *)sx,
                                  5*HZ);
            FILL_BULK_URB_TO(&(sx->snd_urb),
                               sx->usbdev,
                               usb_sndbulkpipe(sx->usbdev, sx->snd_endpoint),
                               (unsigned char *)&(sx->ctrl_setup),
                               sizeof(devrequest) + sizeof(ccdloaddelay),
                               sx_usb_snd_complete,
                               (void *)sx,
                               5*HZ);
            FILL_BULK_URB_TO(&(sx->rcv_urb),
                               sx->usbdev,
                               usb_rcvbulkpipe(sx->usbdev, sx->rcv_endpoint),
                               NULL,
                               0,
                               sx_usb_rcv_complete,
                               (void *)sx,
                               5*HZ);
            /*
             * Check for camera model.
             */
            if (usb_control_msg(usbdev, usb_rcvctrlpipe(usbdev, 0), SX_USB_CAMERA_MODEL, USB_TYPE_VENDOR | USB_DIR_IN, 0, 0, setup_data, 2, 1*HZ) != 2)
            {
                printk(KERN_ERR "starlight-xpress: could not get camera model\n");
                return (NULL);
            }
            sx_model = model;
            sx_color = color;
            if (!force)
            {
                /*
                 * Override passed in model if camera returns valid #.
                 */
                printk(KERN_INFO "starlight-xpress: read 0x%02X model from camera\n", setup_data[0]);
                switch (setup_data[0] & 0x7F)
                {
                    case 0x05:
                        sx_color = setup_data[0] & 0x80;
                        sx_model = HX5;
                        break;
                    case 0x09:
                        sx_color = setup_data[0] & 0x80;
                        sx_model = HX9;
                        break;
                    case 0x45:
                        sx_color = setup_data[0] & 0x80;
                        sx_model = MX5;
                        break;
                    case 0x47:
                        sx_color = setup_data[0] & 0x80;
                        sx_model = MX7;
                        break;
                    case 0x49:
                        sx_color = setup_data[0] & 0x80;
                        sx_model = MX9;
                        break;
                }
            }
            if (force || (setup_data[0] == 0xFF && setup_data[1] == 0xFF))
            {
                /*
                 * Set camera model from passed in parameters.
                 */
                switch (sx_model)
                {
                    case HX5:
                        setup_data[0] = 0x05;
                        break;
                    case HX9:
                        setup_data[0] = 0x09;
                        break;
                    case MX5:
                        setup_data[0] = 0x45;
                        break;
                    case MX7:
                        setup_data[0] = 0x47;
                        break;
                    case MX9:
                        setup_data[0] = 0x49;
                        break;
                    default:
                        setup_data[0] = 0x45;
                }
                setup_data[0] |= (sx_color ? 0x80 : 0x00);
                printk(KERN_INFO "starlight-xpress: writing 0x%02X model to camera\n", setup_data[0]);
                if (usb_control_msg(usbdev, usb_sndctrlpipe(usbdev, 0), SX_USB_CAMERA_MODEL, USB_TYPE_VENDOR | USB_DIR_OUT, setup_data[0], 0, NULL, 0, 1*HZ) != 0)
                {
                    printk(KERN_ERR "starlight-xpress: could not download camera model\n");
                    return (NULL);
                }
            }
            /*
             * Verify CCD parameters.
             */
            if (usb_control_msg(usbdev, usb_rcvctrlpipe(usbdev, 0), SX_USB_GET_CCD, USB_TYPE_VENDOR | USB_DIR_IN, 0, 0, setup_data, 17, 1*HZ) != 17)
            {
                printk(KERN_ERR "starlight-xpress: could not upload CCD parameters\n");
                return (NULL);
            }
            /*
             * Setup CCD parameters.
             */
            strcpy(sx->id_string, "Starlight Xpress ");
            strcat(sx->id_string, sx_model < 0 ? "HX0" : "MX0");
            sx->id_string[strlen(sx->id_string) - 1] += sx_model < 0 ? -sx_model : sx_model;
            if (sx_color)
                strcat(sx->id_string, "C");
            strcat(sx->id_string, "/16");
            printk(KERN_INFO "starlight-xpress: camera model is %s\n", sx->id_string);
            sx->num          = 0;
            sx->hfront_porch = setup_data[0];
            sx->hback_porch  = setup_data[1];
            sx->vfront_porch = setup_data[4];
            sx->vback_porch  = setup_data[5];
            sx->width        = setup_data[2]  | (setup_data[3]  << 8);
            sx->height       = setup_data[6]  | (setup_data[7]  << 8);
            sx->pix_width    = setup_data[8]  | (setup_data[9]  << 8);
            sx->pix_height   = setup_data[10] | (setup_data[11] << 8);
            sx->pixel_size   = setup_data[14] >> 3;
            sx->ser_ports    = setup_data[15];
            sx->caps         = setup_data[16];
            sx->pixel_buf    = (unsigned char *)vmalloc(sx->width * sx->height * sx->pixel_size);
            sx->rcv_buf      = (unsigned char *)kmalloc(SX_BULK_READ_SIZE, GFP_KERNEL);
            sx->state_flags  = SX_STATE_CONNECTED;
            if (!sx->rcv_buf)
             {
                printk(KERN_INFO "starlight-xpress: unable to alloc rcv_buf\n");
                return (NULL);
            }
            if (!sx->pixel_buf)
            {
                printk(KERN_INFO "starlight-xpress: unable to alloc pixel_buf\n");
                return (NULL);
            }
            memset(&mini_device,  0, sizeof(mini_device));
            strcpy(mini_device.id_string, sx->id_string);
            mini_device.version      = CCD_VERSION;
            mini_device.width        = sx->width;
            mini_device.height       = sx->height;
            mini_device.pixel_width  = sx->pix_width;
            mini_device.pixel_height = sx->pix_height;
            mini_device.image_fields = (sx_model > 0) ? MX_IMAGE_FIELDS : HX_IMAGE_FIELDS;
            mini_device.image_depth  = sx->pixel_size * 8;
            mini_device.dac_bits     = sx->pixel_size * 8;
            mini_device.color_format = setup_data[12] | (setup_data[13] << 8);
                                       /*color ? (CCD_COLOR_MATRIX_2X2 | CCD_COLOR_MATRIX_ALT_ODD | (sx->vfront_porch & 1 ? (sx->hfront_porch & 1 ? 0x5B6 : 0xA79)
                                                                                                                          : (sx->hfront_porch & 1 ? 0x6B5 : 0x97A)))
                                               : CCD_COLOR_MONOCHROME;*/
            mini_device.flag_caps    = CCD_EXP_FLAGS_NOBIN_ACCUM | CCD_EXP_FLAGS_NOWIPE_FRAME | CCD_EXP_FLAGS_TDI | CCD_EXP_FLAGS_NOCLEAR_FRAME;
            mini_device.open         = sx_open;
            mini_device.control      = sx_control;
            mini_device.close        = sx_close;
            mini_device.read_row     = sx_read_row;
            mini_device.begin_read   = sx_begin_read;
            mini_device.end_read     = sx_end_read;
            mini_device.latch_frame  = sx_latch_frame;
            mini_device.new_frame    = sx_new_frame;
            if (ccd_register_device(&mini_device, (void *)sx) < 0)
            {
                printk(KERN_ERR "starlight-xpress: unable to register\n");
                return (NULL);
            }
            printk(KERN_INFO "starlight-xpress: camera has %d serial ports\n", sx->ser_ports);
            if (sx->caps & SX_USB_CAPS_STAR2K)
                printk(KERN_INFO "starlight-xpress: camera has integrated STAR 2000 port\n");
            if (sx->caps & SX_USB_CAPS_COMPRESS)
                printk(KERN_INFO "starlight-xpress: camera has compressed pixel support\n");
            if (sx->caps & SX_USB_CAPS_GUIDER)
            {
                struct sx_device_t *sxg = sx_new_struct();
                printk(KERN_INFO "starlight-xpress: camera has additional guide CCD support\n");
                if (sxg)
                {
                    /*
                     * Add auto-guider device (which is a simplified MX5).
                     */
                    strcpy(sxg->id_string, "Starlight Xpress Guider");
                    printk(KERN_INFO "starlight-xpress: camera model is %s\n", sxg->id_string);
                    /*
                     * Get CCD parameters for guider.
                     */
                    if (usb_control_msg(usbdev, usb_rcvctrlpipe(usbdev, 0), SX_USB_GET_CCD, USB_TYPE_VENDOR | USB_DIR_IN, 0, 1, setup_data, 17, 1*HZ) != 17)
                    {
                        printk(KERN_ERR "starlight-xpress: could not upload guider parameters\n");
                        return (NULL);
                    }
                    sxg->num           = 1;
                    sxg->hfront_porch  = setup_data[0];
                    sxg->hback_porch   = setup_data[1];
                    sxg->vfront_porch  = setup_data[4];
                    sxg->vback_porch   = setup_data[5];
                    sxg->width         = setup_data[2]  | (setup_data[3]  << 8);
                    sxg->height        = setup_data[6]  | (setup_data[7]  << 8);
                    sxg->pix_width     = setup_data[8]  | (setup_data[9]  << 8);
                    sxg->pix_height    = setup_data[10] | (setup_data[11] << 8);
                    sxg->pixel_size    = setup_data[14] >> 3;
                    sxg->snd_endpoint  = usbdev->config[0].interface[0].altsetting[usbdev->config[0].interface[0].act_altsetting].endpoint[1].bEndpointAddress;
                    sxg->snd_data_size = usbdev->config[0].interface[0].altsetting[usbdev->config[0].interface[0].act_altsetting].endpoint[1].wMaxPacketSize;
                    sxg->rcv_endpoint  = usbdev->config[0].interface[0].altsetting[usbdev->config[0].interface[0].act_altsetting].endpoint[0].bEndpointAddress;
                    sxg->rcv_data_size = usbdev->config[0].interface[0].altsetting[usbdev->config[0].interface[0].act_altsetting].endpoint[0].wMaxPacketSize;
                    sxg->usbdev        = usbdev;
                    sxg->pixel_buf     = (unsigned char *)vmalloc(sxg->width * sxg->height * sxg->pixel_size);
                    sxg->rcv_buf       = (unsigned char *)kmalloc(SX_BULK_READ_SIZE, GFP_KERNEL);
                    sxg->state_flags  = SX_STATE_CONNECTED;
                    spin_lock_init(&sxg->lock);
                    spin_lock_init(&sxg->ctrl_urb.lock);
                    spin_lock_init(&sxg->snd_urb.lock);
                    spin_lock_init(&sxg->rcv_urb.lock);
                    init_waitqueue_head(&(sxg->read_wait));
                    FILL_CONTROL_URB_TO(&(sxg->ctrl_urb),
                                          sxg->usbdev,
                                          usb_sndctrlpipe(sxg->usbdev, 0),
                                          (unsigned char *)&(sxg->ctrl_setup),
                                          &(sxg->ctrl_load_params),
                                          sizeof(ccdload),
                                          sx_usb_ctrl_complete,
                                          (void *)sxg,
                                          5*HZ);
                    FILL_BULK_URB_TO(&(sxg->snd_urb),
                                       sxg->usbdev,
                                       usb_sndbulkpipe(sxg->usbdev, sxg->snd_endpoint),
                                       (unsigned char *)&(sxg->ctrl_setup),
                                       sizeof(devrequest) + sizeof(ccdloaddelay),
                                       sx_usb_snd_complete,
                                       (void *)sxg,
                                       5*HZ);
                    FILL_BULK_URB_TO(&(sxg->rcv_urb),
                                       sxg->usbdev,
                                       usb_rcvbulkpipe(sxg->usbdev, sxg->rcv_endpoint),
                                       NULL,
                                       0,
                                       sx_usb_rcv_complete,
                                       (void *)sxg,
                                       5*HZ);
                    if (!sxg->rcv_buf)
                    {
                        printk(KERN_INFO "starlight-xpress: unable to alloc rcv_buf\n");
                        return (NULL);
                    }
                    if (!sxg->pixel_buf)
                    {
                        printk(KERN_INFO "starlight-xpress: unable to alloc pixel_buf\n");
                        return (NULL);
                    }
                    memset(&mini_device,  0, sizeof(mini_device));
                    strcpy(mini_device.id_string, sxg->id_string);
                    mini_device.version      = CCD_VERSION;
                    mini_device.width        = sxg->width;
                    mini_device.height       = sxg->height;
                    mini_device.pixel_width  = sxg->pix_width;
                    mini_device.pixel_height = sxg->pix_height;
                    mini_device.image_fields = 1;
                    mini_device.image_depth  = sxg->pixel_size * 8;
                    mini_device.dac_bits     = sxg->pixel_size * 8;
                    mini_device.color_format = setup_data[12] | (setup_data[13] << 8);
                    mini_device.flag_caps    = CCD_EXP_FLAGS_NOBIN_ACCUM | CCD_EXP_FLAGS_NOWIPE_FRAME | CCD_EXP_FLAGS_NOCLEAR_FRAME;
                    mini_device.open         = sx_open;
                    mini_device.control      = sx_control;
                    mini_device.close        = sx_close;
                    mini_device.read_row     = sx_read_row;
                    mini_device.begin_read   = sx_begin_read;
                    mini_device.end_read     = sx_end_read;
                    mini_device.latch_frame  = sx_latch_frame;
                    mini_device.new_frame    = sx_new_frame;
                    if (ccd_register_device(&mini_device, (void *)sxg) < 0)
                    {
                        printk(KERN_ERR "starlight-xpress: unable to register\n");
                        return (NULL);
                    }
                }
            }
            MOD_INC_USE_COUNT;
            return (sx);
        }
    }
    return (NULL);
}
static void sx_usb_disconnect(struct usb_device *usbdev, void *drv_context)
{
    unsigned long cpu_flags;
    int           i;

    printk(KERN_INFO "starlight-xpress: disconnect\n");
    if (drv_context == (void *)&ezusb_device)
    {
        if (ezusb_device.state_flags & EZUSB_STATE_QUEUED)
        {
            /*
             * It'll figure it out later.
             */
        }
        ezusb_device.state_flags &= ~(EZUSB_STATE_DOWNLOADING | EZUSB_STATE_QUEUED);
    }
    else
    {
        for (i = 0; i < NR_SX; i++)
        {
            if (sx_devices[i].in_use && sx_devices[i].usbdev == usbdev)
            {
                spin_lock_irqsave (&sx_devices[i].lock, cpu_flags);
                if (sx_devices[i].state_flags & SX_STATE_CONNECTED)
                    sx_devices[i].state_flags &= ~SX_STATE_CONNECTED;
                if (!(sx_devices[i].state_flags & SX_STATE_BUSY))
                {
                    ccd_unregister_device((void *)&sx_devices[i]);
                    if (sx_devices[i].rcv_buf)
                    {
                        kfree(sx_devices[i].rcv_buf);
                        sx_devices[i].rcv_buf = NULL;
                    }
                    if (sx_devices[i].pixel_buf)
                    {
                        vfree(sx_devices[i].pixel_buf);
                        sx_devices[i].pixel_buf = NULL;
                    }
                    sx_devices[i].usbdev = NULL;
                    sx_free_struct(&sx_devices[i]);
                }
                spin_unlock_irqrestore(&sx_devices[i].lock, cpu_flags);
                /*
                 * Wake any sleeping process.
                 */
                wake_up_interruptible(&sx_devices[i].read_wait);
            }
        }
    }
    MOD_DEC_USE_COUNT;
}

/***************************************************************************\
*                                                                           *
*                            Module init/cleanup                            *
*                                                                           *
\***************************************************************************/

/*
 * USB driver registration.
 */
static struct usb_driver sx_usb_driver =
{
    name:       "starlight-xpress",
    probe:      sx_usb_probe,
    disconnect: sx_usb_disconnect,
    fops:       NULL,
    minor:      0,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))
    id_table:   sx_usb_id_table
#endif
};
/*
 * TTY driver registration.
 */
static struct tty_driver sx_tty_driver =
{
    magic:          TTY_DRIVER_MAGIC,
    driver_name:    "starlight-xpress",
    name:           "ttysx",
    major:          TTY_MAJOR,
    minor_start:    128,
    num:            NR_SX * 4,
    type:           TTY_DRIVER_TYPE_SERIAL,
    subtype:        SERIAL_TYPE_NORMAL,
    flags:          TTY_DRIVER_REAL_RAW | TTY_DRIVER_NO_DEVFS,
    refcount:      &sx_tty_refcount,
    table:          sx_tty_table,
    termios:        sx_tty_termios,
    termios_locked: sx_tty_termios_locked,
    open:           sx_tty_open,
    close:          sx_tty_close,
    write:          sx_tty_write,
    write_room:     sx_tty_write_room,
    ioctl:          sx_tty_ioctl,
    chars_in_buffer:sx_tty_chars_in_buffer,
};
/*
 * Module initialization.
 */
int init_module(void)
{
    sx_devices = (struct sx_device_t *)kmalloc(sizeof(struct sx_device_t) * NR_SX, GFP_KERNEL);
    memset(sx_devices, 0, sizeof(struct sx_device_t) * NR_SX);
    /*
     * Convert poll from HZ into jiffies.
     */
     poll = poll ? HZ / poll : HZ;
     poll = max(poll, 2);
    /*
     * Register USB driver.
     */
    usb_register(&sx_usb_driver);
    /*
     * Register with TTY driver.
     */
    if (serial)
    {
        sx_tty_ser_opened                   = 0;
        sx_tty_driver.init_termios          = tty_std_termios;
        sx_tty_driver.init_termios.c_cflag  = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
        if (tty_register_driver(&sx_tty_driver))
        {
            printk(KERN_ERR "starlight-xpress: unable to register serial driver\n");
            serial = 0;
        }
    }
    return (0);
}
void cleanup_module(void)
{
    usb_deregister(&sx_usb_driver);
    if (serial && tty_unregister_driver(&sx_tty_driver))
        printk(KERN_ERR "starlight-xpress: failed to unregister serial driver\n");
    kfree(sx_devices);
}

