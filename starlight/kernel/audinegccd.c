/***************************************************************************\

    Copyright (c) 2001 David Schmenk
    Audine interface copyright (c) 2001 Peter Kirchgessner

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
#include <linux/ioport.h>
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

/* I don't like including C-source. But I need the source */
/* for two kernel drivers using the same low level IOs */
#include "audine_io.c"

/*
 * v1.00, 27-Nov-2001, pk, created
 */
static char *version = "v1.00";

static int port = 0x378;   /* default port */
static int type = KAF0401; /* default CCD type */

/* #define DEBUG */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))
MODULE_AUTHOR("Peter Kirchgessner");
MODULE_DESCRIPTION("Audine parallel port astronomy camera driver");
MODULE_LICENSE("GPL");
MODULE_PARM_DESC(type, "Audine camera model");
MODULE_PARM_DESC(port, "Audine camera printer port #");
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
MODULE_PARM(port, "i");
MODULE_PARM(type, "i");
#else
module_param(port, int, 0);
module_param(type, int, 0);
#endif
#endif

/* This mini-driver can handle only one Audine camera */
static struct audine_device
{
  CCD_DEVICE_INFO *ccd;
  int Port;
  int got_Port;
  unsigned int CntrlReg;
  unsigned int msec;
  unsigned int xbin, ybin;
  unsigned int xoffset, yoffset;
  unsigned int width, height;

  /* Quite a lot of memory. Maybe some time we allocate it dynamically */
  unsigned short pixel[AUDINE_MAX_HEIGHT][AUDINE_MAX_WIDTH];
} audine_camera;

/*
 * Audine read row (from memory)
 */
static int
audine_read_row (void *vp,
                 unsigned int offset, unsigned int row,
                 unsigned int width, unsigned int xbin, unsigned int ybin,
                 unsigned int dac_bits, unsigned int flags, unsigned char *buf)
{
  struct audine_device *audine = (struct audine_device *)vp;
  int k;
  unsigned short *udst = (unsigned short *)buf;
  unsigned short *usrc;

#ifdef DEBUG
  /* printk ("audine_read_row: row=%d, offset=%d, width=%d\n",
   * row, offset, width);
   */
#endif

  /* Get real image data */
  usrc = &(audine->pixel[row][offset]);
  k = width/xbin;
  while (k-- > 0)
    *(udst++) = *(usrc++);

  /*
   * Return # of bytes read.
   */
  return ((width / xbin) * sizeof (unsigned short));
}

/*
 * Begin reading frame (nothing to do. Image already read)
 */
static void
audine_begin_read (void *vp, unsigned int offset, unsigned int flags)
{
#ifdef DEBUG
  printk ("audine_begin_read: offset=%d\n", offset);
#endif
}

/*
 * Complete reading a frame (nothing to to)
 */
static void
audine_end_read (void *vp, unsigned int flags)
{
#ifdef DEBUG
  printk ("audine_end_read:\n");
#endif
}

/*
 * Clear frame and start integrating (save image values)
 */
static void
audine_new_frame (void *vp,
                  unsigned int xoffset, unsigned int yoffset,
                  unsigned int width, unsigned int height,
                  unsigned int xbin, unsigned int ybin,
                  unsigned int dac_bits,
                  unsigned int msec, unsigned int flags)
{
  struct audine_device *audine = (struct audine_device *)vp;
  int k;

#ifdef DEBUG
  printk ("audine_new_frame: msec=%d, xbin=%d, ybin=%d\n", msec, xbin, ybin);
  printk ("                  xoffset=%d, yoffset=%d\n", xoffset, yoffset);
  printk ("                  width=%d, height=%d\n", width, height);
#endif

  /* We must save the values, because we have to read the complete */
  /* image within audine_latch_frame(). Timing is very sensitive. */
  audine->msec = msec;
  audine->xbin = xbin;
  audine->ybin = ybin;
  audine->xoffset = xoffset;
  audine->yoffset = yoffset;
  audine->width = width;
  audine->height = height;

  /* Turn off amplifier (it should be off already) */
  cntrl_register_clearbit (&(audine->CntrlReg), 1);
  cntrl_register_write (audine->CntrlReg, audine->Port);

  /* If msec is 0, it is not a real request to read an image. */
  /* We only prepare for a new image if msec is not 0. */
  if (msec != 0)
    {
      for (k = 0; k < 3; k++)
        ccd_clear (audine->ccd, audine->Port);
    }
}

/*
 * Latch a frame and prepare for reading (in fact read image)
 */
static void
audine_latch_frame(void *vp, unsigned int flags)
{
  struct audine_device *audine = (struct audine_device *)vp;
  CCD_DEVICE_INFO *ccd = audine->ccd;
  int Port = audine->Port;
  int skip, row, npix, k;
  unsigned short *ubuf;

#ifdef DEBUG
  printk ("audine_latch_frame:\n");
#endif

  if (audine->msec == 0)
    {
#ifdef DEBUG
      printk ("audine_latch_frame: Nothing to do\n");
#endif
      return;  /* Not real work ? */
    }

  /* Start reading the complete image. Because of timing problems, */
  /* we can not read in each row when requested by the mini-driver. */

  /* Turn on amplifier */
  cntrl_register_setbit (&(audine->CntrlReg), 1);
  cntrl_register_write (audine->CntrlReg, Port);

  /* Skip leading rows + y-offset */
  skip = ccd->lead_rows + audine->yoffset;
  ccd_lines_read_fast (ccd, Port, skip);

  /* Read in all rows */
  for (row = audine->yoffset; row < audine->yoffset + audine->height;
       row += audine->ybin)
    {
      /* Clear horizontal register */
      ccd_hreg_read_fast (ccd, Port);

      /* Due to binning load horizontal register */
      for (k = 0; k < audine->ybin; k++)
        ccd_hreg_load (Port);

      /* Skip leading pixels */
      skip = ccd->lead_pixels + audine->xoffset;
      if (skip > 0) ccd_pixels_read_fast (skip, Port);

      /* Get real image data */
      npix = audine->width/audine->xbin;
      ubuf = &(audine->pixel[row][audine->xoffset]);
      ccd_pixels_read (npix, audine->xbin, ubuf, Port);

      /* Skip trailing pixels */
      skip =   (ccd->width + ccd->trail_pixels)
             - (audine->xoffset + audine->width);
      if (skip > 0) ccd_pixels_read_fast (skip, Port);
    }

  /* Skip trailing rows */
  skip = (ccd->height + ccd->trail_rows) - (audine->yoffset - audine->height);
  ccd_lines_read_fast (ccd, Port, skip);

  /* Turn off amplifier */
  cntrl_register_clearbit (&(audine->CntrlReg), 1);
  cntrl_register_write (audine->CntrlReg, Port);
}

/***************************************************************************\
*                                                                           *
*                   Open, close and control functions                       *
*                                                                           *
\***************************************************************************/

static void
audine_release_port (struct audine_device *audine)
{
  if (audine->got_Port)
    {
#ifdef DEBUG
      printk ("audine_release_port: release region 0x%03x\n", audine->Port);
#endif
      release_region ((unsigned long)audine->Port, (unsigned long)1);
      audine->got_Port = 0;
    }
}

static int
audine_request_port (struct audine_device *audine)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0))
  int err;
#endif

  audine_release_port (audine);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))
#ifdef DEBUG
  printk ("audine_request_port: request region 0x%03x\n", audine->Port);
#endif
  request_region ((unsigned long)audine->Port, 1, "audinegccd"); /* Can't fail */
#else
  err = check_region((unsigned long)audine->Port, (unsigned long)1);
  if (err)
    {
      printk ("audine_request_port: port 0x%03x not available (%d)\n",
              audine->Port, err);
      return err;
    }
#ifdef DEBUG
  printk ("audine_request_port: request region 0x%03x\n", audine->Port);
#endif
  request_region ((unsigned long)audine->Port, 1, "audinegccd");
#endif
  audine->got_Port = 1;

  return 0;
}

/*
 * Prepare to use CCD device.
 */
static int
audine_open (void *vp)
{
  struct audine_device *audine = (struct audine_device *)vp;
  int err;

#ifdef DEBUG
  printk ("audine_open:\n");
#endif

  err = audine_request_port (audine);
  if (err) return err;

  /* Turn off amplifier */
  cntrl_register_clearbit (&(audine->CntrlReg), 1);
  cntrl_register_write (audine->CntrlReg, audine->Port);

#ifdef DEBUG
  printk ("audine_open: increment use count\n");
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
  MOD_INC_USE_COUNT;
#endif
  return (0);
}

/*
 * Control CCD device.
 * This is device specific, stuff like temperature control, etc.
 */
static int
audine_control (void *vp, unsigned short cmd, unsigned long param)
{
  struct audine_device *audine = (struct audine_device *)vp;

  if (cmd == 0xDEAD)
    {
      audine_release_port (audine);
#ifdef DEBUG
      printk ("audine_control (0xDEAD): dec. use count\n");
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
      MOD_DEC_USE_COUNT;
#endif
    }
  return (0);
}

/*
 * Release CCD device.
 */
static int
audine_close (void *vp)
{
  struct audine_device *audine = (struct audine_device *)vp;

  audine_release_port (audine);

#ifdef DEBUG
  printk ("audine_close: decrement use count\n");
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
  MOD_DEC_USE_COUNT;
#endif
  return (0);
}

/***************************************************************************\
*                                                                           *
*                          Module initialization                            *
*                                                                           *
\***************************************************************************/

int
init_module (void)
{
  struct ccd_mini mini_device;

  /* Get a valid CCD-type */
  if ((type < 0) || (type > sizeof (ccds) / sizeof (ccds[0])))
    type = 2;

  audine_camera.ccd = &(ccds[type]);
  audine_camera.Port = port;
  audine_camera.got_Port = 0;
  audine_camera.CntrlReg = 1;
  audine_camera.msec = 0;
  audine_camera.xbin = audine_camera.ybin = 1;
  audine_camera.xoffset = audine_camera.yoffset = 0;

  strcpy(mini_device.id_string, "Audine Camera");
  mini_device.version      = CCD_VERSION;
  mini_device.width        = audine_camera.ccd->width;
  mini_device.height       = audine_camera.ccd->height;
  mini_device.pixel_width  = 0x100;
  mini_device.pixel_height = 0x100;
  mini_device.image_fields = 1;
  mini_device.image_depth  = 16;
  mini_device.dac_bits     = 16;
  mini_device.color_format = CCD_COLOR_MONOCHROME;
  mini_device.open         = audine_open;
  mini_device.control      = audine_control;
  mini_device.close        = audine_close;
  mini_device.read_row     = audine_read_row;
  mini_device.begin_read   = audine_begin_read;
  mini_device.end_read     = audine_end_read;
  mini_device.latch_frame  = audine_latch_frame;
  mini_device.new_frame    = audine_new_frame;

  if (ccd_register_device(&mini_device, (void *)&audine_camera) < 0)
    {
      printk("audinegccd init_module: unable to register\n");
      return (-EIO);
    }
  else
    {
      printk ("Registered audinegccd %s\n", version);
    }
  return (0);
}

void
cleanup_module (void)
{
}

