/***************************************************************************\

    Copyright (c) 2001, 2002, 2003 David Schmenk

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
#include <linux/sched.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include "ccd.h"
/*
 * State flag values.
 */
#define SX_STATE_READING        0x0002
/*
 * ioctl commands specific to SX parport driver.
 */
#define SX_CTL_CMD_SET_DELAY    1
/*
 * Minimum time (msec) before letting the driver time the exposure.
 */
#define SX_MIN_EXP_TIME             50
/*
 * Timeout to fallback to standard interface.
 */
#define SX_DAC_TIMEOUT      30
/*
 * Set up some defines for the CCD macro functions.
 */
#define DAT_OUT(io,v,d)                             \
            do {                                    \
                parport_write_data((io)->pport,v);  \
                if((d))udelay(d);                   \
            } while (0)
#define CTL_OUT(io,v,d)                             \
            do {                                    \
                parport_write_control((io)->pport,v);\
                if((d))udelay(d);                   \
            } while (0)
#define DAT_IN(io) parport_read_data((io)->pport)
#define STS_IN(io) parport_read_status((io)->pport)
#define CTL_IN(io) parport_read_control((io)->pport)


#define HX5                     -5
#define HX5_WIDTH               660
#define HX5_HEIGHT              494
#define HX5_PIX_WIDTH           (int)(7.4*256)
#define HX5_PIX_HEIGHT          (int)(7.4*256)
#define HX5_HFRONT_PORCH        18
#define HX5_HBACK_PORCH         20
#define HX5_VFRONT_PORCH        7
#define HX5_VBACK_PORCH         1
#define HX5_FIELD_XOR_MASK      (CCD_FIELD_EVEN<<4) // AND 0, XOR EVEN

#define HX9                     -9
#define HX9_WIDTH               1300
#define HX9_HEIGHT              1030
#define HX9_PIX_WIDTH           (int)(6.7*256)
#define HX9_PIX_HEIGHT          (int)(6.7*256)
#define HX9_HFRONT_PORCH        30
#define HX9_HBACK_PORCH         56
#define HX9_VFRONT_PORCH        2
#define HX9_VBACK_PORCH         1

#define HX9_FIELD_XOR_MASK  (CCD_FIELD_EVEN<<4) // AND 0, XOR EVEN

#define HX9_2                   -9
#define HX9_2_WIDTH             1392
#define HX9_2_HEIGHT            1040
#define HX9_2_PIX_WIDTH         (int)(6.7*256)
#define HX9_2_PIX_HEIGHT        (int)(6.7*256)
#define HX9_2_HFRONT_PORCH      22
#define HX9_2_HBACK_PORCH       40
#define HX9_2_VFRONT_PORCH      11
#define HX9_2_VBACK_PORCH       2
#define HX9_2_FIELD_XOR_MASK    (CCD_FIELD_EVEN<<4) // AND 0, XOR EVEN

#define HX_IMAGE_FIELDS         1

#if 0

#define MX5                     5
#define MX5_WIDTH               512
#define MX5_HEIGHT              291
#define MX5_PIX_WIDTH           (int)(9.8*256)
#define MX5_PIX_HEIGHT          (int)(12.6*256)
#define MX5_HFRONT_PORCH        18
#define MX5_HBACK_PORCH         30
#define MX5_VFRONT_PORCH        7
#define MX5_VBACK_PORCH         1
#define MX5_FIELD_XOR_MASK      CCD_FIELD_BOTH // AND BOTH, XOR 0

#define MX7                     7
#define MX7_WIDTH               768
#define MX7_HEIGHT              291
#define MX7_PIX_WIDTH           (int)(8.6*256)
#define MX7_PIX_HEIGHT          (int)(16.6*256)
#define MX7_HFRONT_PORCH        18
#define MX7_HBACK_PORCH         40
#define MX7_VFRONT_PORCH        6
#define MX7_VBACK_PORCH         1
#define MX7_FIELD_XOR_MASK      CCD_FIELD_BOTH // AND BOTH, XOR 0

#define MX9                     9
#define MX9_WIDTH               768
#define MX9_HEIGHT              291
#define MX9_PIX_WIDTH           (int)(11.6*256)
#define MX9_PIX_HEIGHT          (int)(22.4*256)
#define MX9_HFRONT_PORCH        18
#define MX9_HBACK_PORCH         40
#define MX9_VFRONT_PORCH        6
#define MX9_VBACK_PORCH         1
#define MX9_FIELD_XOR_MASK      CCD_FIELD_BOTH // AND BOTH, XOR 0

#else

#define MX5                     5
#define MX5_WIDTH               500
#define MX5_HEIGHT              290
#define MX5_PIX_WIDTH           (int)(9.8*256)
#define MX5_PIX_HEIGHT          (int)(12.6*256)
#define MX5_HFRONT_PORCH        23
#define MX5_HBACK_PORCH         30
#define MX5_VFRONT_PORCH        8
#define MX5_VBACK_PORCH         1
#define MX5_FIELD_XOR_MASK      CCD_FIELD_BOTH // AND BOTH, XOR 0

#define MX7                     7
#define MX7_WIDTH               752
#define MX7_HEIGHT              290
#define MX7_PIX_WIDTH           (int)(8.6*256)
#define MX7_PIX_HEIGHT          (int)(16.6*256)
#define MX7_HFRONT_PORCH        25
#define MX7_HBACK_PORCH         40
#define MX7_VFRONT_PORCH        7
#define MX7_VBACK_PORCH         1
#define MX7_FIELD_XOR_MASK      CCD_FIELD_BOTH // AND BOTH, XOR 0

#define MX9                     9
#define MX9_WIDTH               752
#define MX9_HEIGHT              290
#define MX9_PIX_WIDTH           (int)(11.6*256)
#define MX9_PIX_HEIGHT          (int)(22.4*256)
#define MX9_HFRONT_PORCH        25
#define MX9_HBACK_PORCH         40
#define MX9_VFRONT_PORCH        7
#define MX9_VBACK_PORCH         1
#define MX9_FIELD_XOR_MASK      CCD_FIELD_BOTH // AND BOTH, XOR 0

#endif

#define MX_IMAGE_FIELDS         2
#define SX_IMAGE_DEPTH          16
#define SX_DAC_BITS             16
#define SX_IFACE_STANDARD       0
#define SX_IFACE_FAST_MK1       1
#define SX_IFACE_FAST_MK2       2
struct sx_device_t
{
    unsigned int      state_flags;
    unsigned int      frame_msec[2];
    struct pardevice *pdev;
    struct parport   *pport;
    int               iface;
    int               width, height;
    int               hfront_porch, hback_porch;
    int               vfront_porch, vback_porch;
    int               field_xor_mask;
};
/*
 * Basic camera control macros.
 */
#define CCD_WIPE(io)                                                        \
        do {                                                                \
        DAT_OUT(io, 0xC5, 10);          /* Dump pixel charge            */  \
        DAT_OUT(io, 0x45, 0);                                               \
        } while (0)
#define CCD_LATCH_FRAME(io, flags)                                          \
        do {                                                                \
        CTL_OUT(io, 0x02, 3);           /* Amp off, VCLK = read mode    */  \
        if (flags & CCD_FIELD_EVEN) {   /* Latch even field             */  \
            DAT_OUT(io, 0x4D, 5);                                           \
            DAT_OUT(io, 0x45, 2);                                           \
        }                                                                   \
        if (flags & CCD_FIELD_ODD) {    /* Latch odd field              */  \
            DAT_OUT(io, 0x55, 5);                                           \
            DAT_OUT(io, 0x45, 2);                                           \
        }                                                                   \
        if (!(flags & CCD_EXP_FLAGS_TDI))                                   \
            DAT_OUT(io, 0x41, 0);       /* Trigger vert clock           */  \
        CTL_OUT(io, 0x06, 0);           /* Amp off, VCLK = clock mode   */  \
        } while (0)
#define CCD_LATCH_FRAME_FAST(io, flags)                                     \
        do {                                                                \
        DAT_OUT(io, 0x44, 0);                                               \
        DAT_OUT(io, 0x00, 0);           /* Amp off, VCLK = read mode    */  \
        DAT_OUT(io, 0x20, 0);                                               \
        DAT_OUT(io, 0x45, 0);                                               \
        if (flags & CCD_FIELD_EVEN) {   /* Latch even field             */  \
            DAT_OUT(io, 0x4D, 5);                                           \
            DAT_OUT(io, 0x45, 2);                                           \
        }                                                                   \
        if (flags & CCD_FIELD_ODD) {    /* Latch odd field              */  \
            DAT_OUT(io, 0x55, 5);                                           \
            DAT_OUT(io, 0x45, 2);                                           \
        }                                                                   \
        if (!(flags & CCD_EXP_FLAGS_TDI))                                   \
            DAT_OUT(io, 0x41, 0);       /* Trigger vert clock           */  \
        DAT_OUT(io, 0x44, 0);                                               \
        DAT_OUT(io, 0x02, 0);           /* Amp off, VCLK = clock mode   */  \
        DAT_OUT(io, 0x22, 0);                                               \
        DAT_OUT(io, 0x45, 0);                                               \
        } while (0)
#define CCD_AMP_ON(io)                                                      \
        CTL_OUT(io, 0x04, 0)            /* Amp on, VCLK = clock mode   */
#define CCD_AMP_ON_FAST(io)                                                 \
        do {                                                                \
        DAT_OUT(io, 0x44, 0);                                               \
        DAT_OUT(io, 0x06, 0);           /* Amp on, VCLK = clock mode   */   \
        DAT_OUT(io, 0x26, 0);                                               \
        DAT_OUT(io, 0x45, 0);                                               \
        } while (0)
#define CCD_AMP_OFF(io)                                                     \
        CTL_OUT(io, 0x06, 0)            /* Amp off, VCLK = clock mode  */
#define CCD_AMP_OFF_FAST(io)                                                \
        do {                                                                \
        DAT_OUT(io, 0x44, 0);                                               \
        DAT_OUT(io, 0x02, 0);           /* Amp off, VCLK = clock mode   */  \
        DAT_OUT(io, 0x22, 0);                                               \
        DAT_OUT(io, 0x45, 0);                                               \
        } while (0)
#define CCD_VCLK(io)                                                        \
        do {                                                                \
        DAT_OUT(io, 0x41, 0);           /* Trigger vert clock           */  \
        DAT_OUT(io, 0x45, 2);                                               \
        } while (0)
#define CCD_HCLK(io)                                                        \
        do {                                                                \
        DAT_OUT(io, 0x05, 0);           /* Trigger horiz clock          */  \
        DAT_OUT(io, 0x45, 0);                                               \
        } while (0)
#define CCD_RESET_OUTPUT(io)                                                \
        DAT_OUT(io, 0x65, 0)            /* Reset ouput gate             */
#define CCD_LATCH_PIXEL(io)                                                 \
        do {                                                                \
        DAT_OUT(io, 0x47, 0);           /* A/D convert                  */  \
        DAT_OUT(io, 0x45, 2);                                               \
        } while (0)
#define CCD_LOAD_PIXEL(io, bits, pixel)  /* Do after HCLK2               */ \
        do {                                                                \
        int _ii;                                                            \
        DAT_OUT(io, 0x44, 0);                                               \
        DAT_OUT(io, 0x45, 0);                                               \
        pixel = 0;                                                          \
        for (_ii = 0; _ii < bits; _ii++) {                                  \
            DAT_OUT(io, 0x44, 0);                                           \
            /*STS_IN(io);*/             /* Delay for long cable         */  \
            pixel += pixel;             /* Shift up by one              */  \
            pixel |= STS_IN(io) & 0x20;                                     \
            DAT_OUT(io, 0x45, 0);                                           \
        }                                                                   \
        pixel >>= 5;                                                        \
        if (bits < 16) pixel <<= (16 - bits);                               \
        } while (0)
#define CCD_LATCH_PIXEL_FAST(io,to)     /* Trigger download to F/I       */ \
        do {                                                                \
        (to)=SX_DAC_TIMEOUT;                                                \
        DAT_OUT(io, 0x47, 0);                                               \
        while(!(STS_IN(io)&0x80)&&--(to));                                  \
        DAT_OUT(io, 0x46, 0);                                               \
        } while (0)
#define CCD_LOAD_PIXEL_FAST(io, bits, pixel) /* Load pixel nibble at a time*/\
        do {                                                                \
        DAT_OUT(io, 0x76, 0);                                               \
        DAT_OUT(io, 0x56, 0);                                               \
        pixel = (STS_IN(io)&0x78) << 9; /* Read most sig nibble         */  \
        if (bits > 4) {                                                     \
        DAT_OUT(io, 0x66, 0);                                               \
        DAT_OUT(io, 0x46, 0);                                               \
        pixel |= (STS_IN(io)&0x78) << 5;/* Read 3rd nibble              */  \
        if (bits > 8) {                                                     \
        DAT_OUT(io, 0xB6, 0);                                               \
        DAT_OUT(io, 0x96, 0);                                               \
        pixel |= (STS_IN(io)&0x78) << 1;/* Read 2nd nibble              */  \
        if (bits > 12) {                                                    \
        DAT_OUT(io, 0xA6, 0);                                               \
        DAT_OUT(io, 0x86, 0);                                               \
        pixel |= (STS_IN(io)&0x78) >> 3;/* Read least sig nibble        */  \
        } } }                                                               \
        } while (0)

/***************************************************************************\
*                                                                           *
*                          Basic SX camera functions                        *
*                                                                           *
\***************************************************************************/

static int sx_wait_idle(struct sx_device_t *sx, int timeout)
{
    timeout = timeout / 2 + 1;
    while ((sx->state_flags & SX_STATE_READING)
        && timeout--)
    {
           current->state = TASK_INTERRUPTIBLE;
           schedule_timeout(2);
    }
    return (sx->state_flags & SX_STATE_READING);
}
/*
 * Clear horizontal shift registers.
 */
static void sx_clear_row(struct sx_device_t *sx)
{
    unsigned int i, timeout;

    switch (sx->iface)
    {
        case SX_IFACE_FAST_MK1:
        case SX_IFACE_FAST_MK2:
            for (i = 0; i < sx->hfront_porch + sx->width + sx->hback_porch; i++)
            {
                CCD_HCLK(sx);
                if (!(i & 0x1F))
                    CCD_LATCH_PIXEL_FAST(sx, timeout);
            }
            CCD_LATCH_PIXEL_FAST(sx, timeout);
            break;
        case SX_IFACE_STANDARD:
        default:
            for (i = 0; i < sx->hfront_porch + sx->width + sx->hback_porch; i++)
            {
                CCD_HCLK(sx);
                CCD_RESET_OUTPUT(sx);
            }
            break;
    }
}
/*
 * Clock out frame row by row.
 */
static void sx_clear_frame(struct sx_device_t *sx)
{
    unsigned int j;

    switch (sx->iface)
    {
        case SX_IFACE_FAST_MK1:
        case SX_IFACE_FAST_MK2:
            CCD_AMP_ON_FAST(sx);
            break;
        case SX_IFACE_STANDARD:
        default:
            CCD_AMP_ON(sx);
            break;
    }
    for (j = 0; j < sx->vfront_porch + sx->height + sx->vback_porch; j++)
    {
        CCD_VCLK(sx); /* Double clock the vertical registers to ensure clean frame */
        CCD_VCLK(sx);
        if (!(j & 0x1F))
            sx_clear_row(sx);
    }
    sx_clear_row(sx);
    switch (sx->iface)
    {
        case SX_IFACE_FAST_MK1:
        case SX_IFACE_FAST_MK2:
            CCD_AMP_OFF_FAST(sx);
            break;
        case SX_IFACE_STANDARD:
        default:
            CCD_AMP_OFF(sx);
            break;
    }
}
/*
 * Does windowing, binning, and DAC precision truncation.
 */
static int sx_read_row(void *vp, unsigned int offset, unsigned int row, unsigned int width, unsigned int xbin, unsigned int ybin, unsigned int dac_bits, unsigned int flags, unsigned char *buf)
{
    unsigned int        pixel, i, j, timeout;
    unsigned long       psw;
    struct sx_device_t *sx       = (struct sx_device_t *)vp;
    unsigned short     *pixel_buf = (unsigned short *)buf;

    sx->state_flags |= SX_STATE_READING;
    switch (sx->iface)
    {
        case SX_IFACE_FAST_MK1:
        case SX_IFACE_FAST_MK2:
            /*
             * Fast parallel port interface version for reading pixels.
             */
            if ((j = (ybin - 1)))
            {
                /*
                 * 'Binning' accumulates multiple rows worth of pixel
                 * charges in the horizontal shift registers.
                 */
                while (j--)
                    CCD_VCLK(sx);
                if (flags & CCD_EXP_FLAGS_NOBIN_ACCUM) /* Force no accumulation */
                    sx_clear_row(sx);
            }
            CCD_VCLK(sx);
            /*
             * Skip offset pixels.
             */
            for (i = 0; i <= offset + sx->hfront_porch; i++)
            {
                CCD_HCLK(sx);
                if (i & 0x01)
                    CCD_LATCH_PIXEL_FAST(sx, timeout);
            }
            CCD_LATCH_PIXEL_FAST(sx, timeout);
            /*
             * Load width / bin pixels.
             */
            local_irq_save(psw);
            if (xbin > 1)
            {
                for (i = xbin - 1; i < width; i += xbin)
                {
                    *pixel_buf = 0;
                    local_irq_disable();
                    for (j = 1; j < xbin; j++)
                    {
                        /*
                         * 'Binning' accumulates multiple pixels in
                         * the output register. This is done unless the
                         * no-accumulate flag is set.
                         */
                        CCD_HCLK(sx);
                    }
                    if (flags & CCD_EXP_FLAGS_NOBIN_ACCUM)
                        CCD_LATCH_PIXEL_FAST(sx, timeout);
                    /*
                     * Read out final pixel.
                     */
                    CCD_HCLK(sx);
                    CCD_LATCH_PIXEL_FAST(sx, timeout);
                    CCD_LOAD_PIXEL_FAST(sx, dac_bits, pixel);
                    local_irq_restore(psw);
                    *pixel_buf = pixel + *pixel_buf > 0xFFFF ? 0xFFFF : pixel + *pixel_buf;
                    pixel_buf++;
                }
            }
            else
            {
                for (i = 0; i < width; i++)
                {
                    local_irq_disable();
                    /*
                     * Read out pixel.
                     */
                    CCD_HCLK(sx);
                    CCD_LATCH_PIXEL_FAST(sx, timeout);
                    CCD_LOAD_PIXEL_FAST(sx, dac_bits, pixel);
                    local_irq_restore(psw);
                    *pixel_buf++ = (unsigned short)pixel;
                }
            }
            /*
             * Clear out rest of horizontal shift register.
             */
            while (i++ < sx->width + sx->hback_porch)
            {
                CCD_HCLK(sx);
                if (!(i & 0x03))
                    CCD_LATCH_PIXEL_FAST(sx, timeout);
            }
            CCD_LATCH_PIXEL_FAST(sx, timeout);
            break;
        case SX_IFACE_STANDARD:
        default:
            /*
             * Regular parallel port interface version for reading pixels.
             */
            if ((j = (ybin - 1)))
            {
                /*
                 * 'Binning' accumulates multiple rows worth of pixel
                 * charges in the horizontal shift registers.
                 */
                while (j--)
                    CCD_VCLK(sx);
                if (flags & CCD_EXP_FLAGS_NOBIN_ACCUM) /* Force no accumulation */
                    sx_clear_row(sx);
            }
            CCD_VCLK(sx);
            /*
             * Skip offset pixels.
             */
            for (i = 0; i < offset + sx->hfront_porch; i++)
            {
                CCD_HCLK(sx);
                CCD_RESET_OUTPUT(sx);
            }
            /*
             * Load width / bin pixels.
             */
            local_irq_save(psw);
            if (xbin > 1)
            {
                for (i = xbin - 1; i < width; i += xbin)
                {
                    *pixel_buf = 0;
                    local_irq_disable();
                    CCD_RESET_OUTPUT(sx);
                    for (j = 1; j < xbin; j++)
                    {
                        /*
                         * 'Binning' accumulates multiple pixels in
                         * the output register. This is done unless the
                         * no-accumulate flag is set.
                         */
                        CCD_HCLK(sx);
                    }
                    if (flags & CCD_EXP_FLAGS_NOBIN_ACCUM)
                        CCD_RESET_OUTPUT(sx);
                    /*
                     * Read out final pixel.
                     */
                    CCD_HCLK(sx);
                    CCD_LATCH_PIXEL(sx);
                    CCD_LOAD_PIXEL(sx, dac_bits, pixel);
                    local_irq_restore(psw);
                    *pixel_buf = pixel + *pixel_buf > 0xFFFF ? 0xFFFF : pixel + *pixel_buf;
                    pixel_buf++;
                }
            }
            else
            {
                for (i = 0; i < width; i++)
                {
                    local_irq_disable();
                    /*
                     * Read out pixel.
                     */
                    CCD_RESET_OUTPUT(sx);
                    CCD_HCLK(sx);
                    CCD_LATCH_PIXEL(sx);
                    CCD_LOAD_PIXEL(sx, dac_bits, pixel);
                    local_irq_restore(psw);
                    *pixel_buf++ = (unsigned short)pixel;
                }
            }
            /*
             * Clear out rest of horizontal shift register.
             */
            while (i++ < sx->width + sx->hback_porch)
            {
                CCD_HCLK(sx);
                CCD_RESET_OUTPUT(sx);
            }
            break;
    }
    sx->state_flags &= ~SX_STATE_READING;
    /*
     * Return # of bytes read.
     */
    return ((width / xbin) * sizeof(unsigned short));
}
/*
 * Begin reading frame. Clock out rows until window Y offset reached unless in TDI mode.
 */
static void sx_begin_read(void *vp, unsigned int offset, unsigned int flags)
{
    unsigned int        i;
    struct sx_device_t *sx = (struct sx_device_t *)vp;

    switch (sx->iface)
    {
        case SX_IFACE_FAST_MK1:
        case SX_IFACE_FAST_MK2:
            CCD_AMP_ON_FAST(sx);
            break;
        case SX_IFACE_STANDARD:
        default:
            CCD_AMP_ON(sx);
            break;
    }
    if (!(flags & CCD_EXP_FLAGS_TDI))
    {
        /*
         * Clear out front porch really well.
         * The MX7 really holds a charge there.
         */
        for (i = 1; i < sx->vfront_porch; i++)
        {
            CCD_VCLK(sx);
            sx_clear_row(sx);
        }
        /*
         * Skip offset rows.
         */
        while (offset--)
        {
            CCD_VCLK(sx);
            if (!(offset & 0x0F))
                sx_clear_row(sx);
        }
    }
    /*
     * Clear row a bunch of times.
     */
    for (i = 0; i < 10; i++)
        sx_clear_row(sx);
}
/*
 * Complete reading a frame (turn off amplifier).
 */
static void sx_end_read(void *vp, unsigned int flags)
{
    struct sx_device_t *sx = (struct sx_device_t *)vp;

    switch (sx->iface)
    {
        case SX_IFACE_FAST_MK1:
        case SX_IFACE_FAST_MK2:
            CCD_AMP_OFF_FAST(sx);
            break;
        case SX_IFACE_STANDARD:
        default:
            CCD_AMP_OFF(sx);
            break;
    }
}
/*
 * Latch a frame and prepare for reading.
 */
static void sx_latch_frame(void *vp, unsigned int flags)
{
    struct sx_device_t *sx    = (struct sx_device_t *)vp;
    unsigned int        field = (flags & sx->field_xor_mask) ^ (sx->field_xor_mask >> 4);

    if (sx->frame_msec[flags & CCD_FIELD_ODD] >= SX_MIN_EXP_TIME)
    {
        if (!(flags & (CCD_TDI | CCD_NOCLEAR_FRAME)) && (sx->frame_msec[flags & CCD_FIELD_ODD] > 2000))
            sx_clear_frame(sx);
        switch (sx->iface)
        {
            case SX_IFACE_FAST_MK1:
            case SX_IFACE_FAST_MK2:
                CCD_LATCH_FRAME_FAST(sx, field);
                break;
            case SX_IFACE_STANDARD:
            default:
                CCD_LATCH_FRAME(sx, field);
                break;
        }
    }
}
/*
 * Clear frame and start integrating.
 */
static void sx_new_frame(void *vp, unsigned int xoffset, unsigned int yoffset, unsigned int width, unsigned int height, unsigned int xbin, unsigned int ybin, unsigned int dac_bits, unsigned int msec, unsigned int flags)
{
    struct sx_device_t *sx    = (struct sx_device_t *)vp;
    unsigned int        field = (flags & sx->field_xor_mask) ^ (sx->field_xor_mask >> 4);

    if (!(flags & CCD_TDI))
    {
        if (!(flags & (CCD_NOWIPE_FRAME | CCD_NOCLEAR_FRAME)))
        {
            switch (sx->iface)
            {
                case SX_IFACE_FAST_MK1:
                case SX_IFACE_FAST_MK2:
                    CCD_LATCH_FRAME_FAST(sx, CCD_FIELD_BOTH);
                    break;
                case SX_IFACE_STANDARD:
                default:
                    CCD_LATCH_FRAME(sx, CCD_FIELD_BOTH);
                    break;
            }
            sx_clear_frame(sx);
            CCD_WIPE(sx);
        }
        else if (width == sx->width && height == sx->height)
        {
            switch (sx->iface)
            {
                case SX_IFACE_FAST_MK1:
                case SX_IFACE_FAST_MK2:
                    CCD_LATCH_FRAME_FAST(sx, field);
                    break;
                case SX_IFACE_STANDARD:
                default:
                    CCD_LATCH_FRAME(sx, field);
                    break;
            }
            sx_clear_frame(sx);
        }
    }
    sx->frame_msec[flags & CCD_FIELD_ODD] = msec;
    if (msec < SX_MIN_EXP_TIME)
    {
        unsigned int field = (flags & sx->field_xor_mask) ^ (sx->field_xor_mask >> 4);
        while (msec--)
            udelay(1000);
        switch (sx->iface)
        {
            case SX_IFACE_FAST_MK1:
            case SX_IFACE_FAST_MK2:
                CCD_LATCH_FRAME_FAST(sx, field);
                break;
            case SX_IFACE_STANDARD:
            default:
                CCD_LATCH_FRAME(sx, field);
                break;
        }
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
static int sx_open(void *vp)
{
    unsigned int       timeout;
    struct sx_device_t *sx = (struct sx_device_t *)vp;

    parport_claim_or_block(sx->pdev);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
    MOD_INC_USE_COUNT;
#endif
    /*
     * Auto-detect fast interface.
     */
    sx->iface = SX_IFACE_FAST_MK1;
    CCD_LATCH_PIXEL_FAST(sx, timeout);
    /*
     * Don't believe an interface that is too slow or too quick.
     */
    if (timeout == 0 || timeout == SX_DAC_TIMEOUT)
        sx->iface = SX_IFACE_STANDARD;
    return (0);
}
/*
 * Control CCD device. This is device specific, stuff like temperature control, etc.
 */
static int sx_control(void *vp, unsigned short cmd, unsigned long param)
{
    struct sx_device_t *sx = (struct sx_device_t *)vp;

    switch (cmd)
    {
        case CCD_CTRL_CMD_RESET:
            if (sx->state_flags & SX_STATE_READING)
                printk(KERN_INFO "starlight-xpress: reading flag set during reset\n");
            if (sx_wait_idle(sx, 2*HZ))
                printk(KERN_ERR "starlight-xpress: reading flag never unset during reset!\n");
            switch (sx->iface)
            {
                case SX_IFACE_FAST_MK1:
                case SX_IFACE_FAST_MK2:
                    CCD_WIPE(sx);
                    CCD_LATCH_FRAME_FAST(sx, CCD_FIELD_BOTH);
                    CCD_AMP_OFF(sx);
                    break;
                case SX_IFACE_STANDARD:
                default:
                    CCD_WIPE(sx);
                    CCD_LATCH_FRAME(sx, CCD_FIELD_BOTH);
                    CCD_AMP_OFF(sx);
                    break;
            }
            sx_clear_frame(sx);
            break;
        /*
         * Major hack while debugging.
         */
        case CCD_CTRL_CMD_DEC_MOD:
            parport_release(sx->pdev);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
            MOD_DEC_USE_COUNT;
#endif
            break;
    }
    return (0);
}
/*
 * Release CCD device.
 */
static int sx_close(void *vp)
{
    struct sx_device_t *sx = (struct sx_device_t *)vp;

    if (sx->state_flags & SX_STATE_READING)
        printk(KERN_INFO "starlight-xpress: reading flag set during close\n");
    if (sx_wait_idle(sx, 2*HZ))
        printk(KERN_ERR "starlight-xpress: reading flag never unset during close!\n");
    parport_release(sx->pdev);
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

#define NR_SX 2
static struct sx_device_t sx_devices[NR_SX];
/*
 * Optional parameters.
 */
static int parport[NR_SX] = { [0 ... NR_SX-1] = -1 };
static int model[NR_SX] = { [0 ... NR_SX-1] = MX5  };
static int dac[NR_SX] = { [0 ... NR_SX-1] = SX_DAC_BITS };
static int color[NR_SX] = { 0, };
/*
 * Per camera dimensions.
 */
static int sx_hfront_porch[]   = {HX5_HFRONT_PORCH,   HX9_HFRONT_PORCH,   MX5_HFRONT_PORCH,   MX7_HFRONT_PORCH,   MX9_HFRONT_PORCH};
static int sx_hback_porch[]    = {HX5_HBACK_PORCH,    HX9_HBACK_PORCH,    MX5_HBACK_PORCH,    MX7_HBACK_PORCH,    MX9_HBACK_PORCH};
static int sx_vfront_porch[]   = {HX5_VFRONT_PORCH,   HX9_VFRONT_PORCH,   MX5_VFRONT_PORCH,   MX7_VFRONT_PORCH,   MX9_VFRONT_PORCH};
static int sx_vback_porch[]    = {HX5_VBACK_PORCH,    HX9_VBACK_PORCH,    MX5_VBACK_PORCH,    MX7_VBACK_PORCH,    MX9_VBACK_PORCH};
static int sx_width[]          = {HX5_WIDTH,          HX9_WIDTH,          MX5_WIDTH,          MX7_WIDTH,          MX9_WIDTH};
static int sx_height[]         = {HX5_HEIGHT,         HX9_HEIGHT,         MX5_HEIGHT,         MX7_HEIGHT,         MX9_HEIGHT};
static int sx_pix_width[]      = {HX5_PIX_WIDTH,      HX9_PIX_WIDTH,      MX5_PIX_WIDTH,      MX7_PIX_WIDTH,      MX9_PIX_WIDTH};
static int sx_pix_height[]     = {HX5_PIX_HEIGHT,     HX9_PIX_HEIGHT,     MX5_PIX_HEIGHT,     MX7_PIX_HEIGHT,     MX9_PIX_HEIGHT};
static int sx_field_xor_mask[] = {HX5_FIELD_XOR_MASK, HX9_FIELD_XOR_MASK, MX5_FIELD_XOR_MASK, MX7_FIELD_XOR_MASK, MX9_FIELD_XOR_MASK};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))
MODULE_AUTHOR("David Schmenk, dschmenk@earthlink.net");
MODULE_DESCRIPTION("Starlight Xpress parallel port astronomy camera driver");
MODULE_LICENSE("GPL");
MODULE_PARM_DESC(model,  "SX camera model");
MODULE_PARM_DESC(parport,"SX camera printer port #");
MODULE_PARM_DESC(dac,    "SX camera ADC size in bits");
MODULE_PARM_DESC(color,  "SX camera one-shot color flag");
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
module_param_array(parport, int, NULL, 0);
module_param_array(model, int, NULL, 0);
module_param_array(dac, int, NULL, 0);
module_param_array(color, int, NULL, 0);
#else
MODULE_PARM(parport, "1-" __MODULE_STRING(NR_SX) "i");
MODULE_PARM(model,   "1-" __MODULE_STRING(NR_SX) "i");
MODULE_PARM(dac,     "1-" __MODULE_STRING(NR_SX) "i");
MODULE_PARM(color,   "1-" __MODULE_STRING(NR_SX) "i");
#endif

static unsigned int count = 0;

static void sx_attach (struct parport *port)
{
    int j;
    struct ccd_mini mini_device;
    for (j = 0; j < NR_SX; j++)
    if (port->number == parport[j])
    {
        switch (model[count])
        {
            case HX5:
                j = 0;
                break;
            case HX9:
                j = 1;
                break;
            case MX5:
                j = 2;
                break;
            case MX7:
                j = 3;
                break;
            case MX9:
                j = 4;
                break;
            default:
                printk(KERN_INFO "starlight-xpress: invalid model specified (%i)\n", model[count]);
                return;
        }
        sx_devices[count].state_flags    = 0;
        sx_devices[count].pdev           = parport_register_device(port, "sx", NULL, NULL, NULL, 0, NULL);
        sx_devices[count].pport          = port;
        sx_devices[count].hfront_porch   = sx_hfront_porch[j];
        sx_devices[count].hback_porch    = sx_hback_porch[j];
        sx_devices[count].vfront_porch   = sx_vfront_porch[j];
        sx_devices[count].vback_porch    = sx_vback_porch[j];
        sx_devices[count].width          = sx_width[j];
        sx_devices[count].height         = sx_height[j];
        sx_devices[count].field_xor_mask = sx_field_xor_mask[j];
        strcpy(mini_device.id_string,    "SX ");
        strcat(mini_device.id_string, model[count] < 0 ? "HX0" : "MX0");
        mini_device.id_string[strlen(mini_device.id_string) - 1] += model[count] < 0 ? -model[count] : model[count];
        if (color[count])
            strcat(mini_device.id_string, "C");
        strcat(mini_device.id_string, dac[count] == 16 ? "/16" : "/12");
        strcat(mini_device.id_string, " (LPT:1)");
        mini_device.id_string[strlen(mini_device.id_string) - 2] += port->number;
        mini_device.version            = CCD_VERSION;
        mini_device.width              = sx_width[j];
        mini_device.height             = sx_height[j];
        mini_device.pixel_width        = sx_pix_width[j];
        mini_device.pixel_height       = sx_pix_height[j];
        mini_device.image_fields       = MX_IMAGE_FIELDS;
        mini_device.image_depth        = SX_IMAGE_DEPTH;
        mini_device.dac_bits           = dac[count];
        mini_device.color_format       = color[count] ? (CCD_COLOR_MATRIX_2X2 | CCD_COLOR_MATRIX_ALT_ODD | (sx_vfront_porch[j] & 1 ? (sx_hfront_porch[j] & 1 ? 0x5B6 : 0xA79) : (sx_hfront_porch[j] & 1 ? 0x6B5 : 0x97A))) : CCD_COLOR_MONOCHROME;
        mini_device.flag_caps          = CCD_EXP_FLAGS_NOBIN_ACCUM | CCD_EXP_FLAGS_NOWIPE_FRAME | CCD_EXP_FLAGS_TDI | CCD_EXP_FLAGS_NOCLEAR_FRAME;
        mini_device.open               = sx_open;
        mini_device.control            = sx_control;
        mini_device.close              = sx_close;
        mini_device.read_row           = sx_read_row;
        mini_device.begin_read         = sx_begin_read;
        mini_device.end_read           = sx_end_read;
        mini_device.latch_frame        = sx_latch_frame;
        mini_device.new_frame          = sx_new_frame;
        if (ccd_register_device(&mini_device, (void *)&sx_devices[count]) < 0)
        {
            printk(KERN_ERR "starlight-xpress ccd: unable to register on parport %i\n", port->number);
            return;
        }
        printk(KERN_INFO "starlight-xpress %d on port %d.\n", model[count], port->number);
        count++;
        break;
    }
}

static void sx_detach (struct parport *port)
{
    printk(KERN_INFO "starlight-xpress ccd: detaching parport %i\n", port->number);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
static struct list_head sx_driver_head = LIST_HEAD_INIT(sx_driver_head);
#endif

static struct parport_driver sx_driver;

int init_module(void)
{
    int i;
    for (i = 0; i < NR_SX; i++)
        sx_devices[i].pdev = NULL;

    sx_driver.name = "sx";
    sx_driver.attach = sx_attach;
    sx_driver.detach = sx_detach;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,3))
    sx_driver.next = NULL;
#else
    sx_driver.list = sx_driver_head;
#endif

    /*
     * Default to parport0 if no options given.
     */
    if (parport[0] == -1)
        parport[0] = 0;

    // that will call attach for every founded parport
    if (parport_register_driver (&sx_driver))
        return -EIO;
    
    if (count == 0)
    {
        printk(KERN_INFO "starlight-xpress ccd: no devices found\n");
        return (-ENODEV);
    }
    return (0);
}

void cleanup_module(void)
{
    int i;

    for (i = 0; i < NR_SX; i++)
    {
        if (sx_devices[i].pdev)
        {
            parport_unregister_device(sx_devices[i].pdev);
            ccd_unregister_device((void *)&sx_devices[i]);
        }
    }
}


