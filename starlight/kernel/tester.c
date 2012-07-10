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

#define TEST_WIDTH          256
#define TEST_HEIGHT         256
#define TEST_IMAGE_DEPTH    8

struct test_device
{
    int x_star, y_star;
    int dx_star, dy_star;
    int x_radius, y_radius;
} test_camera;

unsigned char test_pixels[TEST_HEIGHT][TEST_WIDTH];
/*
 * Test read row.
 */
static int test_read_row(void *vp, unsigned int offset, unsigned int row, unsigned int width, unsigned int xbin, unsigned int ybin, unsigned int dac_bits, unsigned int flags, unsigned char *buf)
{
    int i;
    
    /*
     * Load width / bin pixels.
     */
    for (i = 0; i < width; i += xbin)
        *buf++ = test_pixels[row][offset + i];
    /*
     * Return # of bytes read.
     */
    return (width / xbin);
}
/*
 * Begin reading frame. Clock out rows until window Y offset reached.
 */
static void test_begin_read(void *vp, unsigned int offset, unsigned int flags)
{
}
/*
 * Complete reading a frame (turn off amplifier).
 */
static void test_end_read(void *vp, unsigned int flags)
{
}
/*
 * Clear frame and start integrating.
 */
static void test_new_frame(void *vp, unsigned int xoffset, unsigned int yoffset, unsigned int width, unsigned int height, unsigned int xbin, unsigned int ybin, unsigned int dac_bits, unsigned int msec, unsigned int flags)
{
    int i, j;
    for (j = 0; j < TEST_HEIGHT; j++)
        for (i = 0; i < TEST_WIDTH; i++)
            test_pixels[j][i] = 0;
}
/*
 * Latch a frame and prepare for reading.
 */
static void test_latch_frame(void *vp, unsigned int flags)
{
    int                 i, j;
    struct test_device *test = (struct test_device *)vp;
    
    for (i = 1; i < 4; i++)
    {
        for (j = -i; j <= i; j++)
        {
            test_pixels[test->y_star + j][test->x_star - i] = 0x7F;
            test_pixels[test->y_star + j][test->x_star + i] = 0x7F;
            test_pixels[test->y_star + i][test->x_star + j] = 0x7F;
            test_pixels[test->y_star - i][test->x_star + j] = 0x7F;
        }
    }
    test_pixels[test->y_star][test->x_star] = 0xFF;
    test->x_star += test->dx_star;
    test->y_star += test->dy_star;
    if (test->x_star < TEST_WIDTH / 2 - 4 || test->x_star > TEST_WIDTH / 2 + 4)
        test->dx_star = -test->dx_star;
    if (test->y_star < TEST_HEIGHT / 2 - 4 || test->y_star > TEST_HEIGHT / 2 + 4)
        test->dy_star = -test->dy_star;
}

/***************************************************************************\
*                                                                           *
*                   Open, close and control functions                       *
*                                                                           *
\***************************************************************************/

/*
 * Prepare to use CCD device.
 */
static int test_open(void *vp)
{
    struct test_device *test = (struct test_device *)vp;
    
    test->x_star = TEST_WIDTH  / 2;
    test->y_star = TEST_HEIGHT / 2;
    test->dx_star = 1;
    test->dy_star = 1;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
    MOD_INC_USE_COUNT;
#endif
    return (0);
}
/*
 * Control CCD device. This is device specific, stuff like temperature control, etc.
 */
static int test_control(void *vp, unsigned short cmd, unsigned long param)
{
    if (cmd == 0xDEAD)
    {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
        MOD_DEC_USE_COUNT;
#endif
    }
    return (0);
}
/*
 * Release CCD device.
 */
static int test_close(void *vp)
{
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

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))
MODULE_AUTHOR("David Schmenk, dschmenk@earthlink.net");
MODULE_DESCRIPTION("Testing astronomy camera driver");
MODULE_LICENSE("GPL");
#endif
int init_module(void)
{
    struct ccd_mini mini_device;

    strcpy(mini_device.id_string, "Test Camera");
    mini_device.version      = CCD_VERSION;
    mini_device.width        = TEST_WIDTH;
    mini_device.height       = TEST_HEIGHT;
    mini_device.pixel_width  = 0x100;
    mini_device.pixel_height = 0x100;
    mini_device.image_fields = 1;
    mini_device.image_depth  = TEST_IMAGE_DEPTH; 
    mini_device.dac_bits     = TEST_IMAGE_DEPTH;
    mini_device.color_format = CCD_COLOR_MONOCHROME;
    mini_device.open         = test_open;
    mini_device.control      = test_control;
    mini_device.close        = test_close;
    mini_device.read_row     = test_read_row;
    mini_device.begin_read   = test_begin_read;
    mini_device.end_read     = test_end_read;
    mini_device.latch_frame  = test_latch_frame;
    mini_device.new_frame    = test_new_frame;
    if (ccd_register_device(&mini_device, (void *)&test_camera) < 0)
    {
        printk("test: unable to register\n");
        return (-EIO);
    }
    return (0);
}

void cleanup_module(void)
{
    ccd_unregister_device((void *)&test_camera);
}

