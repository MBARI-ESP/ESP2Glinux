/***************************************************************************\

    Copyright (c) 2004 Petr Kubanek

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
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))
#include <linux/slab.h>
#else
#include <linux/malloc.h>
#endif
#include <linux/mm.h>
#include <linux/parport.h>
#include <linux/parport_pc.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include "ccd.h"

/* Common device information */
struct flidevinfo_t
{
  long type;
  long fwrev;
  long hwrev;
  long devid;
  long serno;
  char *model;
  char *devnam;
};

struct point_t
{
  int x;			/* X coordinate */
  int y;			/* Y coordinate */
};

struct area_t
{
  struct point_t ul;		/* Upper-left */
  struct point_t lr;		/* Lower-right */
};

/* CCD Parameter list */
struct fliccdinfo_t
{
  short index;
  char *model;
  struct area_t array_area;
  struct area_t visible_area;
  double fillfactor;
  double pixelwidth;
  double pixelheight;
};

struct flicamdata_t
{
  long readto;
  long writeto;
  long dirto;

  /* Acquisistion parameters */
  struct area_t image_area;
  long vbin;
  long hbin;
  long vflushbin;
  long hflushbin;
  long exposure;
  long expdur;
  long expmul;
  long frametype;
  long flushes;
  long bitdepth;
  long exttrigger;
  long exttriggerpol;
//  double tempslope;
//  double tempintercept;

  long grabrowcount;
  long grabrowcounttot;
  long grabrowindex;
  long grabrowwidth;
  long grabrowbatchsize;
  long grabrowbufferindex;
  long flushcountbeforefirstrow;
  long flushcountafterlastrow;

  unsigned short *gbuf;
};

struct fli_device_t
{
  unsigned int state_flags;
  unsigned int frame_msec[2];
  struct pardevice *pdev;
  struct parport *pport;
  struct flidevinfo_t devinfo;
  struct fliccdinfo_t ccd;
  struct flicamdata_t data;
  int iface;
  int width, height;
  int hfront_porch, hback_porch;
  int vfront_porch, vback_porch;
  int field_xor_mask;
  int to;
  int dir;
  struct semaphore io_sem;
};

const struct fliccdinfo_t knowndev[] = {
  /* id model           array_area              visible_area */
  {1, "KAF-0260C0-2", {{0, 0}, {534, 520}}, {{12, 4}, {524, 516}}, 1.0, 20.0,
   20.0},
  {2, "KAF-0400C0-2", {{0, 0}, {796, 520}}, {{14, 4}, {782, 516}}, 1.0, 20.0,
   20.0},
  {3, "KAF-1000C0-2", {{0, 0}, {1042, 1032}}, {{8, 4}, {1032, 1028}}, 1.0,
   24.0, 24.0},
  {4, "KAF-1300C0-2", {{0, 0}, {1304, 1028}}, {{4, 2}, {1284, 1026}}, 1.0,
   20.0, 20.0},
  {5, "KAF-1400C0-2", {{0, 0}, {1348, 1037}}, {{14, 14}, {782, 526}}, 1.0,
   20.0, 20.0},
  {6, "KAF-1600C0-2", {{0, 0}, {1564, 1032}}, {{14, 4}, {1550, 1028}}, 1.0,
   20.0, 20.0},
  {7, "KAF-4200C0-2", {{0, 0}, {2060, 2048}}, {{25, 2}, {2057, 2046}}, 1.0,
   20.0, 20.0},
  {8, "SITe-502S", {{0, 0}, {527, 512}}, {{15, 0}, {527, 512}}, 1.0, 20.0,
   20.0},
  {9, "TK-1024", {{0, 0}, {1124, 1024}}, {{50, 0}, {1074, 1024}}, 1.0, 24.0,
   24.0},
  {10, "TK-512", {{0, 0}, {563, 512}}, {{51, 0}, {563, 512}}, 1.0, 24.0,
   24.0},
  {11, "SI-003A", {{0, 0}, {1056, 1024}}, {{16, 0}, {1040, 1024}}, 1.0, 24.0,
   24.0},
  {12, "KAF-6300", {{0, 0}, {3100, 2056}}, {{16, 4}, {3088, 2052}}, 1.0, 9.0,
   9.0},
  {13, "KAF-3200", {{0, 0}, {2267, 1510}}, {{46, 34}, {2230, 1506}}, 1.0, 6.8,
   6.8},
  {14, "SI424A", {{0, 0}, {2088, 2049}}, {{20, 0}, {2068, 2049}}, 1.0, 6.8,
   6.8},
  {15, "CCD47-10", {{0, 0}, {1072, 1027}}, {{8, 0}, {1064, 1027}}, 0.0, 0.0,
   0.0},
  {16, "CCD77", {{0, 0}, {527, 512}}, {{15, 0}, {527, 512}}, 0.0, 0.0, 0.0},
  {17, "CCD42-40", {{0, 0}, {2148, 2048}}, {{50, 0}, {2098, 2048}}, 1.0, 13.5,
   13.5},
  {18, "KAF-4300", {{0, 0}, {2102, 2092}}, {{8, 4}, {2092, 2088}}, 1.0, 24.0,
   24.0},
  {19, "KAF-16801", {{0, 0}, {4145, 4128}}, {{44, 29}, {4124, 4109}}, 1.0,
   9.0, 9.0},
  {0, "Unknown Model", {{0, 0}, {0, 0}}, {{0, 0}, {0, 0}}, 0.0, 0.0, 0.0}
};

/* Define command and data word formats */
#define C_ADDRESS(addr,ext) (0x8000|(((addr)<<8)&0x0f00)|((ext)&0x00ff))
#define C_RESTCFG(gain,chnl,exttrig,res) (0x9000|(((gain)<<8)&0x0f00)|(((chnl)<<5)&0x00e0)|(((exttrig)<<4)&0x0010)|(((res)&0x000f)))
#define C_SHUTTER(open,dmult) (0xa000|((dmult)&0x07ff)|(((open)<<11)&0x0800))
#define C_SEND(x) (0xb000|((x)&0x0fff))
#define C_FLUSH(x) (0xc000|((x)&0x0fff))
#define C_VSKIP(x) (0xd000|((x)&0x0fff))
#define C_HSKIP(x) (0xe000|((x)&0x0fff))
#define C_TEMP(x) (0xf000|((x)&0x0fff))
#define D_XROWOFF(x) (0x0000|((x)&0x0fff))
#define D_XROWWID(x) (0x1000|((x)&0x0fff))
#define D_XFLBIN(x) (0x2000|((x)&0x0fff))
#define D_YFLBIN(x) (0x3000|((x)&0x0fff))
#define D_XBIN(x) (0x4000|((x)&0x0fff))
#define D_YBIN(x) (0x5000|((x)&0x0fff))
#define D_EXPDUR(x) (0x6000|((x)&0x0fff))
#define D_RESERVE(x) (0x7000|((x)&0x0fff))

/* Define extended parameter fields for querying camera */
#define EPARAM_ECHO (0x00)
#define EPARAM_CCDID (0x01)
#define EPARAM_FIRM (0x02)
#define EPARAM_SNHIGH (0x03)
#define EPARAM_SNLOW (0x04)
#define EPARAM_SIGGAIN (0x05)
#define EPARAM_DEVICE (0x06)

/* I/O Bit definitions */
#define FLICCD_IO_P0    (0x01)
#define FLICCD_IO_P1    (0x02)
#define FLICCD_IO_P2    (0x04)
#define FLICCD_IO_P3    (0x08)

#define FLI_FRAME_TYPE_NORMAL (0)
#define FLI_FRAME_TYPE_DARK (1)

#define FLI_MODE_8BIT (0)
#define FLI_MODE_16BIT (1)

#define C2 0x04			/* Bit 2 on the control port */
#define S5 0x20			/* Bit 5 on the status port */
#define S3 0x08			/* Bit 3 on the status port */
#define C5 0x20			/* Bit 5 on the control port */

#define DIR_FORWARD 0x01
#define DIR_REVERSE 0x02

/***************************************************************************\
*                                                                           *
*                          Helper functions                                 *
*                                                                           *
\***************************************************************************/

static long
ECPSetReverse (struct fli_device_t *dev)
{
  unsigned char byte;
  long timeout;

  if (dev->dir == DIR_REVERSE)
          return 0;

  byte = inb (ECONTROL (dev->pport));	/* Switch to PS/2 mode */
  byte &= ~0xe0;
  byte |= 0x20;
  outb (byte, ECONTROL (dev->pport));

  /* Set reverse mode */
  byte = inb (CONTROL (dev->pport));
  byte |= C5;			/* Program for input */
  outb (byte, CONTROL (dev->pport));
  byte &= ~C2;			/* Assert nReverseReq */

//      if(io->notecp > 0)
  byte |= 0x02;
  outb (byte, CONTROL (dev->pport));

  timeout = 0;
  while ((inb (STATUS (dev->pport)) & S5) > 0)	/* Wait for nAckReverse */
    {
      timeout++;
      if (timeout > dev->to)
	{
	  printk (KERN_INFO "ECP: Write timeout during reverse.\n");
	  return -EIO;
	}
    }

  byte = inb (ECONTROL (dev->pport));	/* Switch to ECP mode */
  byte &= ~0xe0;

//      if(io->notecp > 0)
  byte |= 0x20;
//      else
//              byte |= 0x60;
  outb (byte, ECONTROL (dev->pport));

  dev->dir = DIR_REVERSE;

  return 0;
}

long
ECPReadByte (struct fli_device_t *dev, unsigned char *byte,
	     unsigned long timeout)
{
  unsigned char pdata;

  if (ECPSetReverse (dev) != 0)
    {
      return -EIO;
    }

  if (1)			//io->notec > 0)
    {
      pdata = inb (CONTROL (dev->pport));
      while ((inb (STATUS (dev->pport)) & 0x40) > 0)
	{
	  if (timeout == 0)
	    {
	      printk (KERN_ERR "ECP: Timeout during readbyte 1.\n");
	      return -EIO;
	    }
	  timeout--;
	}
      *byte = inb (DATA (dev->pport));
      pdata &= ~0x02;
      outb (pdata, CONTROL (dev->pport));
      while ((inb (STATUS (dev->pport)) & 0x40) == 0)
	{
	  if (timeout == 0)
	    {
	      printk (KERN_ERR "ECP: Timeout during readbyte 2.\n");
	      return -EIO;
	    }
	  timeout--;
	}
      pdata |= 0x02;
      outb (pdata, CONTROL (dev->pport));
    }
  else
    {
      while (((pdata = inb (ECONTROL (dev->pport))) & 0x01) > 0)
	{
	  if (timeout == 0)
	    {
	      printk (KERN_ERR "ECP: Timeout during readbyte 3.\n");
	      return -EIO;
	    }
	  timeout--;
	}
      *byte = inb (CONFIGA (dev->pport));
    }
  return 0;
}

long
ECPRead (struct fli_device_t *dev, void *buffer, long length)
{
  int i = 0;
  int retval = 0;
  unsigned long to;

  if (length == 0)
    return 0;

  while (i < length)
    {
      to = dev->to;
      if ((retval =
	   ECPReadByte (dev, &((unsigned char *) buffer)[i], to)) != 0)
	{
	  printk (KERN_ERR "ECP: Error during read.\n");
	  break;
	}
      i++;
    }
  retval = i;

  return retval;
}

long
ECPReadWord (struct fli_device_t *dev, unsigned short *word,
	     unsigned long timeout)
{
  unsigned char byte;

  if (ECPReadByte (dev, &byte, timeout) != 0)
    {
      printk (KERN_ERR "ECP: Error during read (high byte).\n");
      return -EIO;
    }

  *word = (unsigned short) byte;
  if (ECPReadByte (dev, &byte, timeout) != 0)
    {
      printk (KERN_ERR "ECP: Error during read (low byte).\n");
      return -EIO;
    }
  *word += (byte << 8);
  return 0;
}


static long
ECPSetForward (struct fli_device_t *dev)
{
  unsigned char byte;
  long timeout;

  if (dev->dir == DIR_FORWARD)
          return 0;

  byte = inb (ECONTROL (dev->pport));	/* Switch to PS/2 mode */
  byte &= ~0xe0;
  byte |= 0x20;
  outb (byte, ECONTROL (dev->pport));

  /* Switch to forward mode */
  byte = inb (CONTROL (dev->pport));
  byte |= C2;			/* Deassert nReverseReq */
//      if(io->notecp > 0)
  byte &= ~0x03;
  outb (byte, CONTROL (dev->pport));

  timeout = 0;
  while ((inb (STATUS (dev->pport)) & S5) == 0)	/* Wait for nAckReverse */
    {
      timeout++;
      if (timeout > dev->to)
	{
	  printk (KERN_INFO "ECP: Error setting forward direction.\n");
	  return -EIO;
	}
    }

  byte &= ~C5;			/* Set for forward transfers */
  outb (byte, CONTROL (dev->pport));

  byte = inb (ECONTROL (dev->pport));	/* Switch back to ECP mode */
  byte &= ~0xe0;
//      if(io->notecp > 0)
  byte |= 0x20;
//      else
//              byte |= 0x60;
  outb (byte, ECONTROL (dev->pport));

  dev->dir = DIR_FORWARD;

  return 0;
}

long
ECPWriteByte (struct fli_device_t *dev, unsigned char byte,
	      unsigned long timeout)
{
  unsigned char pdata;

  if (ECPSetForward (dev) != 0)
    {
      printk (KERN_INFO "ECPSetForward\n");
      return -EIO;
    }

  if (1)  // io->notecp == TRUE
    {
	  outb (byte, DATA (dev->pport));
	  pdata = inb (CONTROL (dev->pport));
	  pdata |= 0x01;
	  outb (pdata, CONTROL (dev->pport));
	  while ((inb (STATUS (dev->pport)) & 0x80) > 0)
	    {
	      if (timeout == 0)
		{
		  printk (KERN_INFO "ECP: Write Timeout 1.\n");
		  return -EIO;
		}
	      timeout--;
	    }
	  pdata &= ~0x01;
	  outb (pdata, CONTROL (dev->pport));
	  while ((inb (STATUS (dev->pport)) & 0x80) == 0)
	    {
	      if (timeout == 0)
		{
		  printk (KERN_INFO "ECP: Write Timeout 2.\n");
		  return -EIO;
		}
	      timeout--;
	    }
     }
     else
     {
              outb(byte, CONFIGA(dev->pport));
              /* Check FIFO status */
              while((inb(ECONTROL (dev->pport)) & 0x01) == 0)
              {
                      if(timeout == 0)
                      {
                              printk(KERN_INFO "ECP: Write Timeout.\n");
                              return -EIO;
                      }
                      timeout--;
              }
      }
  return 0;
}

size_t
ECPWrite (struct fli_device_t * dev, void *buffer, size_t length)
{
  int i = 0;
  long retval = 0;
  unsigned long to;

  if (length == 0)
    return 0;

  while (i < length)
    {
      to = dev->to;
      if ((retval =
	   ECPWriteByte (dev, ((unsigned char *) buffer)[i], to)) != 0)
	{
	  printk (KERN_INFO "ECP: Error during write %li.\n", retval);
	  break;
	}
      i++;
    }
  retval = i;
  return retval;
}

long
ECPWriteWord (struct fli_device_t *dev, unsigned short word,
	      unsigned long timeout)
{
  if (ECPWriteByte (dev, (unsigned char) (word & 0x00ff), timeout) < 0)
    {
      printk (KERN_INFO "ECP: Write timeout on low byte.\n");
      return -EIO;
    }
  if (ECPWriteByte (dev, (unsigned char) (word >> 8), timeout) < 0)
    {
      printk (KERN_INFO "ECP: Write timeout on high byte.\n");
      return -EIO;
    }
  return 0;
}


int
fli_parportio (struct fli_device_t * vp, void *buf, size_t * wlen,
	       size_t * rlen)
{
  size_t orig_wlen = *wlen;
  size_t orig_rlen = *rlen;
  if (down_interruptible (&vp->io_sem))
    return -EINTR;
  if (*wlen > 0)
    {
      *wlen = ECPWrite (vp, buf, *wlen);
      if (*wlen != orig_wlen)
	{
	  printk (KERN_INFO "Invalid wlen %li %li\n", orig_wlen, *wlen);
	}
    }
  if (*rlen > 0)
    {
      *rlen = ECPRead (vp, buf, *rlen);
      if (*rlen != orig_rlen)
	{
	  printk (KERN_INFO "Invalid rlen %li %li\n", orig_rlen, *rlen);
	}
    }
  up (&vp->io_sem);
  return 0;
}

long fli_camera_parport_flush_rows (struct fli_device_t *dev, long rows, long repeat)
{
  struct flicamdata_t *cam;
  double dTm;
  size_t rlen, wlen;
  unsigned short buf;

  if (rows < 0)
    return -EINVAL;

  if (rows == 0)
    return 0;

  cam = &dev->data;

  dTm = ((25e-6) / (cam->hflushbin / 2)) * dev->ccd.array_area.lr.x + 1e-3;
  dTm = dTm * rows;
  dTm = dTm / 1e-6;
  cam->readto = (long)dTm;
  cam->writeto = (long)dTm;

  while (repeat>0)
  {
    long retval;

    rlen = 2; wlen = 2;
    buf = htons((unsigned short) C_FLUSH(rows));
    retval = fli_parportio (dev, &buf, &wlen, &rlen);
    if (retval != 0)
    {
      cam->readto = 1000;
      cam->writeto = 1000;
      return retval;
    }
    repeat--;
  }

  return 0;
}

long fli_set_exposure_time(struct fli_device_t *dev, unsigned msec)
{
  struct flicamdata_t *cam;

  cam = &dev->data;

  cam->exposure = msec;

  if (msec <= 15000) /* Less than thirty seconds..., 8.192e-3 sec */
  {
    cam->expdur = 1;
    cam->expmul = (long) (((double) msec) / 8.192);
  }
  else if (msec <= 2000000) /* Less than one hour */
  {
    cam->expdur = (long) (1.0 / 8.192e-3);
    cam->expmul = (long) (msec / 1000);
  }
  else
  {
    cam->expdur = (long) (10.0 / 8.192e-3);
    cam->expmul = (long) (msec / 10000);
  }

  return 0;
}

/*
 * Does windowing, binning, and DAC precision truncation.
 */
static int
fli_read_row (void *vp, unsigned int offset, unsigned int row,
	      unsigned int width, unsigned int xbin, unsigned int ybin,
	      unsigned int dac_bits, unsigned int flags, unsigned char *buff)
{
  struct fli_device_t *dev = (struct fli_device_t*) vp;
  struct flicamdata_t *cam;
  int r;
  size_t ret;
  double dTm;
  size_t rlen, wlen;
  unsigned short buf;

  cam = &dev->data;

  if (cam->flushcountbeforefirstrow > 0)
  {
    if ((r = fli_camera_parport_flush_rows(dev,
					   cam->flushcountbeforefirstrow, 1)))
      return r;

    cam->flushcountbeforefirstrow = 0;
  }

  dTm = (25.0e-6) * dev->ccd.array_area.lr.x + 1e-3;
  dTm = dTm / 1e-6;
  cam->readto = (long)dTm;
  cam->writeto = (long)dTm;

  rlen = 0; wlen = 2;
  buf = htons((unsigned short) C_SEND(cam->grabrowwidth));
  fli_parportio (dev, &buf, &wlen, &rlen);

  if (cam->bitdepth == FLI_MODE_8BIT)
  {
    unsigned char *cbuf;
    int x;

    if ((cbuf = kmalloc (cam->grabrowwidth, GFP_KERNEL)) == NULL)
    {
      printk (KERN_ERR "Failed memory allocation during row grab.\n");
      return -ENOMEM;
    }

    ret = cam->grabrowwidth; wlen = 0;
    r = fli_parportio (dev, cbuf, &wlen, &ret);
    if (r != 0)
    {
      printk (KERN_WARNING "Couldn't grab entire row, got %li of %li bytes.\n",
	    ret, cam->grabrowwidth);
    }
    for (x = 0; x < (int)width; x++)
    {
      ((char *)buff)[x] = (((cbuf[x]) + 128) & 0x00ff);
    }
    kfree(cbuf);
  }
  else
  {
    unsigned short *sbuf;
    int x;

    if ((sbuf = kmalloc (cam->grabrowwidth * sizeof(unsigned short), GFP_KERNEL)) == NULL)
    {
      printk (KERN_ERR "Failed memory allocation during row grab.\n");
      return -ENOMEM;
    }

    ret = cam->grabrowwidth * sizeof(unsigned short); wlen = 0;
    r = fli_parportio (dev, sbuf, &wlen, &ret);
    if (r != 0)
    {
      printk (KERN_WARNING "Couldn't grab entire row, got %li of %li bytes.\n",
	    ret, cam->grabrowwidth);
    }
    for (x = 0; x < (int)width; x++)
    {
      if (dev->devinfo.hwrev == 0x01) /* IMG camera */
      {
				((unsigned short *)buff)[x] = ntohs(sbuf[x]);
      }
      else
      {
				((unsigned short *)buff)[x] = ntohs(sbuf[x]);
      }
    }
    kfree (sbuf);
  }

  rlen = 2; wlen = 0;
  fli_parportio (dev, &buf, &wlen, &rlen);
  if (ntohs(buf) != C_SEND(width))
  {
    printk (KERN_WARNING "Width: %d, requested %li.\n",
	  width, cam->grabrowwidth * sizeof(unsigned short));
    printk (KERN_WARNING "Got 0x%04x instead of 0x%04x.\n", ntohs(buf), C_SEND(width));
    printk (KERN_WARNING "Didn't get command echo at end of row.\n");
  }

  if (cam->grabrowcount > 0)
  {
    cam->grabrowcount--;
    if (cam->grabrowcount == 0)
    {
      if ((r = fli_camera_parport_flush_rows(dev,
					     cam->flushcountafterlastrow, 1)))
			return r;

      cam->flushcountafterlastrow = 0;
      cam->grabrowbatchsize = 1;
    }
  }

  cam->readto = 1000;
  cam->writeto = 1000;

  return ret;
}

/*
 * Begin reading frame. Clock out rows until window Y offset reached unless in TDI mode.
 */
static void
fli_begin_read (void *vp, unsigned int offset, unsigned int flags)
{
  return;
}

/*
 * Complete reading a frame (turn off amplifier).
 */
static void
fli_end_read (void *vp, unsigned int flags)
{
  return;
}

/*
 * Latch a frame and prepare for reading.
 */
static void
fli_latch_frame (void *vp, unsigned int flags)
{
  return;
}


/*
 * Clear frame and start integrating.
 */
static void
fli_new_frame (void *vp, unsigned int xoffset, unsigned int yoffset,
	       unsigned int width, unsigned int height, unsigned int xbin,
	       unsigned int ybin, unsigned int dac_bits, unsigned int msec,
	       unsigned int flags)
{
  struct fli_device_t *dev = (struct fli_device_t*) vp;
  struct flicamdata_t *cam;
  size_t rlen, wlen;
  unsigned short buf;

  cam = &dev->data;

  fli_set_exposure_time (dev, msec);

  printk (KERN_DEBUG "Setting X Row Offset.\n");
  rlen = 2; wlen = 2;
  buf = htons((unsigned short) D_XROWOFF(cam->image_area.ul.x));
  fli_parportio (dev, &buf, &wlen, &rlen);

  printk (KERN_DEBUG "Setting X Row Width to %d.\n", dev->ccd.array_area.lr.x - dev->ccd.array_area.ul.x);
  buf = htons((unsigned short) D_XROWWID(dev->ccd.array_area.lr.x - dev->ccd.array_area.ul.x));
  fli_parportio (dev, &buf, &wlen, &rlen);

  printk (KERN_DEBUG "Setting X Flush Bin.\n");
  buf = htons((unsigned short) D_XFLBIN(cam->hflushbin));
  fli_parportio (dev, &buf, &wlen, &rlen);

  printk (KERN_DEBUG "Setting Y Flush Bin.\n");
  buf = htons((unsigned short) D_YFLBIN(cam->vflushbin));
  fli_parportio (dev, &buf, &wlen, &rlen);

  printk (KERN_DEBUG "Setting X Bin.\n");
  buf = htons((unsigned short) D_XBIN(cam->hbin));
  fli_parportio (dev, &buf, &wlen, &rlen);

  printk (KERN_DEBUG "Setting Y Bin.\n");
  buf = htons((unsigned short) D_YBIN(cam->vbin));
  fli_parportio (dev, &buf, &wlen, &rlen);

  printk (KERN_DEBUG "Setting Exposure Duration.\n");
  buf = htons((unsigned short) D_EXPDUR(cam->expdur));
  fli_parportio (dev, &buf, &wlen, &rlen);

  if (cam->bitdepth == FLI_MODE_8BIT)
  {
    printk (KERN_INFO "Eight Bit.\n");
    buf = htons((unsigned short)((cam->exttrigger > 0) ?
				 C_RESTCFG(0,0,1,7) : C_RESTCFG(0,0,0,7)));
  }
  else
  {
    printk (KERN_INFO "Sixteen Bit.\n");
    buf = htons((unsigned short)((cam->exttrigger > 0) ?
				 C_RESTCFG(0,0,1,15) :
				 C_RESTCFG(0,0,0,15)));
  }
  fli_parportio (dev, &buf, &wlen, &rlen);

  if (cam->flushes > 0)
  {
    int r;

    printk (KERN_DEBUG "Flushing array.\n");
    if ((r = fli_camera_parport_flush_rows(dev,
					   dev->ccd.array_area.lr.y - dev->ccd.array_area.ul.y,
					   cam->flushes)))
      return;
  }

  printk (KERN_INFO "Exposing.\n");
  buf = htons((unsigned short) C_SHUTTER((cam->frametype == FLI_FRAME_TYPE_DARK)?0:1,
			cam->expmul));
  fli_parportio (dev, &buf, &wlen, &rlen);

  cam->grabrowwidth = cam->image_area.lr.x - cam->image_area.ul.x;
  cam->flushcountbeforefirstrow = cam->image_area.ul.y;
  cam->flushcountafterlastrow =
    (dev->ccd.array_area.lr.y - dev->ccd.array_area.ul.y) -
    ((cam->image_area.lr.y - cam->image_area.ul.y) * cam->vbin) -
    cam->image_area.ul.y;

  if (cam->flushcountafterlastrow < 0)
    cam->flushcountafterlastrow = 0;

  cam->grabrowcount = cam->image_area.lr.y - cam->image_area.ul.y;

  return;
}

/***************************************************************************\
*                                                                           *
*                   Open, close and control functions                       *
*                                                                           *
\***************************************************************************/

/*
 * Prepare to use CCD device.
 */
static int
fli_open (void *vp)
{
  struct fli_device_t *fli = (struct fli_device_t *) vp;
  unsigned short buf;
  size_t rlen, wlen;
  int id;

  parport_claim_or_block (fli->pdev);

  outb (0x00, CONTROL (fli->pport));
  outb (0x24, CONTROL (fli->pport));	/* Select "ECP" mode */

  ECPSetForward (fli);

  rlen = 2;
  wlen = 2;
  buf = htons (C_ADDRESS (1, EPARAM_ECHO));
  fli_parportio (fli, &buf, &wlen, &rlen);
  if (buf != htons (C_ADDRESS (1, EPARAM_ECHO)))
    {
      printk (KERN_INFO "Echo back from camera failed.\n");
      return -ENODEV;
    }
  rlen = 2;
  wlen = 2;
  buf = htons (C_ADDRESS (1, EPARAM_DEVICE));
  fli_parportio (fli, &buf, &wlen, &rlen);
  fli->devinfo.hwrev = ntohs (buf) & 0x00ff;
  printk (KERN_INFO "Revision: %li\n", fli->devinfo.hwrev);

  rlen = 2;
  wlen = 2;
  buf = htons (C_ADDRESS (1, EPARAM_CCDID));
  fli_parportio (fli, &buf, &wlen, &rlen);
  fli->devinfo.devid = ntohs (buf) & 0x00ff;
  printk (KERN_INFO "Devid: %li\n", fli->devinfo.devid);

  for (id = 0; knowndev[id].index != 0; id++)
    if (knowndev[id].index == fli->devinfo.devid)
      break;

  if (knowndev[id].index == 0)
    return -ENODEV;

  fli->ccd.array_area.ul.x = knowndev[id].array_area.ul.x;
  fli->ccd.array_area.ul.y = knowndev[id].array_area.ul.y;
  fli->ccd.array_area.lr.x = knowndev[id].array_area.lr.x;
  fli->ccd.array_area.lr.y = knowndev[id].array_area.lr.y;
  fli->ccd.visible_area.ul.x = knowndev[id].visible_area.ul.x;
  fli->ccd.visible_area.ul.y = knowndev[id].visible_area.ul.y;
  fli->ccd.visible_area.lr.x = knowndev[id].visible_area.lr.x;
  fli->ccd.visible_area.lr.y = knowndev[id].visible_area.lr.y;
  fli->ccd.pixelwidth = knowndev[id].pixelwidth;
  fli->ccd.pixelheight = knowndev[id].pixelheight;

  if ((fli->devinfo.model =
       (char *) kmalloc (strlen (knowndev[id].model) + 1,
			 GFP_KERNEL)) == NULL)
    return -ENOMEM;
  strcpy (fli->devinfo.model, knowndev[id].model);

  printk (KERN_DEBUG "Name: %s\n", fli->devinfo.model);
  printk (KERN_DEBUG "Array: (%d,%d),(%d,%d)\n",
	  fli->ccd.array_area.ul.x,
	  fli->ccd.array_area.ul.y,
	  fli->ccd.array_area.lr.x, fli->ccd.array_area.lr.y);
  printk (KERN_DEBUG "Visible: (%d,%d),(%d,%d)\n",
	  fli->ccd.visible_area.ul.x,
	  fli->ccd.visible_area.ul.y,
	  fli->ccd.visible_area.lr.x, fli->ccd.visible_area.lr.y);

  rlen = 2;
  wlen = 2;
  buf = htons (C_ADDRESS (1, EPARAM_SNHIGH));
  fli_parportio (fli, &buf, &wlen, &rlen);
  fli->devinfo.serno = (ntohs (buf) & 0x00ff) << 8;

  rlen = 2;
  wlen = 2;
  buf = htons (C_ADDRESS (1, EPARAM_SNLOW));
  fli_parportio (fli, &buf, &wlen, &rlen);
  fli->devinfo.serno |= (ntohs (buf) & 0x00ff);

  rlen = 2;
  wlen = 2;
  buf = htons (C_ADDRESS (1, EPARAM_FIRM));
  fli_parportio (fli, &buf, &wlen, &rlen);
  fli->devinfo.fwrev = (ntohs (buf) & 0x00ff);

  /* Initialize all varaibles to something */
/*  switch (fli->devinfo.hwrev)
    {
    case 0x01:
      fli->data.tempslope = (100.0 / 201.1);
      fli->data.tempintercept = (-61.613);
      break;

    case 0x02:
      fli->data.tempslope = (70.0 / 215.75);
      fli->data.tempintercept = (-52.5681);
      break;

    default:
      printk (KERN_ERR "Could not set temperature parameters.\n");
      break;
    }*/

  fli->data.vflushbin = 4;
  fli->data.hflushbin = 4;
  fli->data.vbin = 1;
  fli->data.hbin = 1;
  fli->data.image_area.ul.x = fli->ccd.visible_area.ul.x;
  fli->data.image_area.ul.y = fli->ccd.visible_area.ul.y;
  fli->data.image_area.lr.x = fli->ccd.visible_area.lr.x;
  fli->data.image_area.lr.y = fli->ccd.visible_area.lr.y;
  fli->data.exposure = 100;
  fli->data.frametype = FLI_FRAME_TYPE_NORMAL;
  fli->data.flushes = 0;
  fli->data.bitdepth = FLI_MODE_16BIT;
  fli->data.exttrigger = 0;

  fli->data.grabrowwidth =
    (fli->data.image_area.lr.x - fli->data.image_area.ul.x) / fli->data.hbin;
  fli->data.grabrowcount = 1;
  fli->data.grabrowcounttot = fli->data.grabrowcount;
  fli->data.grabrowindex = 0;
  fli->data.grabrowbatchsize = 1;
  fli->data.grabrowbufferindex = fli->data.grabrowcount;
  fli->data.flushcountbeforefirstrow = 0;
  fli->data.flushcountafterlastrow = 0;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
  MOD_INC_USE_COUNT;
#endif
  return (0);
}

/*
 * Control CCD device. This is device specific, stuff like temperature control, etc.
 */
static int
fli_control (void *vp, unsigned short cmd, unsigned long param)
{
  struct fli_device_t *fli = (struct fli_device_t *) vp;

  switch (cmd)
    {
    case CCD_CTRL_CMD_RESET:
      break;
      /*
       * Major hack while debugging.
       */
    case CCD_CTRL_CMD_DEC_MOD:
      parport_release (fli->pdev);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
      MOD_DEC_USE_COUNT;
#endif
      break;
    }
  return (0);
}

void
fli_temp_control (void *vp, unsigned short *fan, int *temp)
{
/*  size_t rlen, wlen;

  unsigned short buf;

  struct fli_device_t *fli = (struct fli_device_t *) vp;

  printk (KERN_INFO "(fli_temp_control) setting to: %i\n", *temp);
  rlen = wlen = 2;
  buf = *temp - fli->data.tempintercept / fli->data.tempslope;
  buf = htons (C_TEMP(buf));
  fli_parportio (fli, &buf, &wlen, &rlen);
  if ((ntohs(buf) & 0xf000) != C_TEMP(0))
     printk (KERN_ERR "(settemperature) echo back from camera failed.");
  rlen = wlen = 2;
  buf = htons (C_TEMP(0x0800)); // special temp from FLI driver
  fli_parportio (fli, &buf, &wlen, &rlen);
  if ((ntohs(buf) & 0xf000) != C_TEMP(0))
    {
       printk (KERN_ERR "(settemperature) echo back from camera failed.");
       *temp = 0;
    }
  else
    *temp = fli->data.tempslope * (double)(ntohs(buf) & 0x00ff) +
       fli->data.tempintercept; */
}
/*
 * Release CCD device.
 */
static int
fli_close (void *vp)
{
  struct fli_device_t *fli = (struct fli_device_t *) vp;

  kfree (fli->devinfo.model);

  parport_release (fli->pdev);
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

#define NR_FLI 2
static struct fli_device_t fli_devices[NR_FLI];
/*
 * Optional parameters.
 */
static int parport[NR_FLI] = {[0 ... NR_FLI - 1] = -1 };

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))
MODULE_AUTHOR ("Petr Kubanek <petr@lascaux.asu.cas.cz>");
MODULE_DESCRIPTION ("FLI-Cam parallel port astronomy camera driver");
MODULE_LICENSE ("GPL");
MODULE_PARM_DESC (parport, "FLI-Cam camera printer port #");
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
MODULE_PARM (parport, "1-" __MODULE_STRING (NR_FLI) "i");
#else
module_param_array (parport, int, NULL, 0);
#endif

unsigned int count = 0;

static void fli_attach (struct parport *port)
{
  unsigned int i;
  struct ccd_mini mini_device;
  for (i = 0; i < NR_FLI; i++)
  {
  if (port->number == parport[i])
    {
      fli_devices[count].state_flags = 0;
      fli_devices[count].pdev =
	parport_register_device (port, "fli", NULL, NULL, NULL, 0,
				 NULL);
      fli_devices[count].pport = port;
      fli_devices[count].to = 1000000;
      fli_devices[count].dir = 0;
      sema_init (&fli_devices[count].io_sem, 1);
      strcpy (mini_device.id_string, "Finger Lakes Instrumentation ");
      strcat (mini_device.id_string, "FLI");
      mini_device.version = CCD_VERSION;
      mini_device.width = 1024;
      mini_device.height = 1074;
      mini_device.pixel_width = 1;
      mini_device.pixel_height = 1;
      mini_device.image_fields = 1;
      mini_device.image_depth = 16;
      mini_device.dac_bits = 16;
      mini_device.color_format = CCD_COLOR_MONOCHROME;
      mini_device.flag_caps =
	CCD_EXP_FLAGS_NOBIN_ACCUM | CCD_EXP_FLAGS_NOWIPE_FRAME |
	CCD_EXP_FLAGS_TDI | CCD_EXP_FLAGS_NOCLEAR_FRAME;
      mini_device.open = fli_open;
      mini_device.control = fli_control;
      mini_device.close = fli_close;
      mini_device.read_row = fli_read_row;
      mini_device.begin_read = fli_begin_read;
      mini_device.end_read = fli_end_read;
      mini_device.latch_frame = fli_latch_frame;
      mini_device.new_frame = fli_new_frame;
//      mini_device.temp_control = fli_temp_control;
      if (ccd_register_device
	  (&mini_device, (void *) &fli_devices[count]) < 0)
	{
	  printk (KERN_ERR "flicam ccd: unable to register\n");
	  return;
	}
      printk (KERN_INFO "flicam on port %d.\n", port->number);
      count++;
      break;
    }
  }
}

static void fli_detach (struct parport *port)
{
    printk(KERN_INFO "flicam ccd: detaching parport %i\n", port->number);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
static struct list_head fli_driver_head = LIST_HEAD_INIT(fli_driver_head);
#endif

static struct parport_driver fli_driver;

int
init_module (void)
{
  unsigned int i;

  for (i = 0; i < NR_FLI; i++)
    fli_devices[i].pdev = NULL;

  fli_driver.name = "fli";
  fli_driver.attach = fli_attach;
  fli_driver.detach = fli_detach;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,3))
  fli_driver.next = NULL;
#else
  fli_driver.list = fli_driver_head;
#endif

  /*
   * Default to parport0 if no options given.
   */
  if (parport[0] == -1)
    parport[0] = 0;

  // that will call attach for every founded parport
  if (parport_register_driver (&fli_driver))
    return -EIO;

  if (count == 0)
    {
      printk (KERN_INFO "flicam ccd: no devices found\n");
      return (-ENODEV);
    }
  return (0);
}

void
cleanup_module (void)
{
  int i;

  for (i = 0; i < NR_FLI; i++)
    {
      if (fli_devices[i].pdev)
	{
	  parport_unregister_device (fli_devices[i].pdev);
	  ccd_unregister_device ((void *) &fli_devices[i]);
	}
    }
}
