/***************************************************************************\
    
    Copyright (c) 2002 David Schmenk
    
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
#include <asm/uaccess.h>
#include "ccd.h"
/*
 * State flag values.
 */
#define CB_STATE_READING        0x0002
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
#include "cookbook.h"
struct cb_device_t
{
    unsigned int      state_flags;
    struct pardevice *pdev;
    struct parport   *pport;
    int               width, height;
    int               hfront_porch, hback_porch;
    int               vfront_porch, vback_porch;
};

/***************************************************************************\
*                                                                           *
*                          Basic CB camera functions                        *
*                                                                           *
\***************************************************************************/

static int cb_wait_idle(struct cb_device_t *cb, int timeout)
{
    timeout = timeout / 2 + 1;
    while ((cb->state_flags & CB_STATE_READING)
        && timeout--)
    {
           current->state = TASK_INTERRUPTIBLE;
           schedule_timeout(2);
    }
    return (cb->state_flags & CB_STATE_READING);
}
/*
 * Clear horizontal shift registers.
 */
static void cb_clear_row(struct cb_device_t *cb)
{                                                   
    unsigned int i;

    for (i = 0; i < cb->hfront_porch + cb->width + cb->hback_porch; i++)
    {
        CB_CLEAR_PIXEL(cb);
    }
}
/*
 * Clock out frame row by row.
 */
static void cb_clear_frame(struct cb_device_t *cb)
{
    unsigned int j;

    for (j = 0; j < (cb->vfront_porch + cb->height + cb->vback_porch) * 2; j++)
    {
        CB_WIPE(cb);
        if (!(j & 0x0F))
            cb_clear_row(cb);
    }
}
/*
 * Does windowing, binning, and DAC precision truncation.
 */
static int cb_read_row(void *vp, unsigned int offset, unsigned int row, unsigned int width, unsigned int xbin, unsigned int ybin, unsigned int dac_bits, unsigned int flags, unsigned char *buf)
{
    unsigned int        pixel, pixel_max, i, j;
    unsigned long       psw;
    struct cb_device_t *cb       = (struct cb_device_t *)vp;
    unsigned short     *pixel_buf = (unsigned short *)buf;

    cb->state_flags |= CB_STATE_READING;
    /*
     * Parallel port interface version for reading pixels.
     */
    if ((j = (ybin - 1)))
    {
        /*
         * 'Binning' accumulates multiple rows worth of pixel
         * charges in the horizontal shift registers.
         */
        while (j--)
            CB_VCLK(cb);
    }
    CB_VCLK(cb);
    /*
     * Skip offset pixels.
     */
    for (i = 0; i < offset + cb->hfront_porch; i++)
    {
        CB_CLEAR_PIXEL(cb);
    }
    /*
     * Load width / bin pixels.
     */
    local_irq_save(psw);
    if (xbin > 1)
    {
        pixel_max = (1 << dac_bits) - 1;
        for (i = xbin - 1; i < width; i += xbin)
        {
            *pixel_buf = 0;
            local_irq_disable();
            CB_RESET_OUTPUT(cb);
            for (j = 1; j < xbin; j++)
            {
                /*
                 * 'Binning' accumulates multiple pixels in
                 * the output register. This is done unless the
                 * no-accumulate flag is set.
                 */
                CB_HCLK(cb);
            }
            /*
             * Read out final pixel.
             */
            CB_HCLK(cb);
            CB_LATCH_PIXEL(cb);
            CB_LOAD_PIXEL(cb, dac_bits, pixel);
            local_irq_restore(psw);
            *pixel_buf = pixel + *pixel_buf > pixel_max ? pixel_max : pixel + *pixel_buf;
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
            CB_RESET_OUTPUT(cb);
            CB_HCLK(cb);
            CB_LATCH_PIXEL(cb);
            CB_LOAD_PIXEL(cb, dac_bits, pixel);
            local_irq_restore(psw);
            *pixel_buf++ = (unsigned short)pixel;
        }
    }
    /*
     * Clear out rest of horizontal shift register.
     */
    while (i++ < cb->width + cb->hback_porch)
    {
        CB_HCLK(cb);
        CB_RESET_OUTPUT(cb);
    }
    cb->state_flags &= ~CB_STATE_READING;
    /*
     * Return # of bytes read.
     */
    return ((width / xbin) * sizeof(unsigned short));
}
/*
 * Begin reading frame. Clock out rows until window Y offset reached unless in TDI mode.
 */
static void cb_begin_read(void *vp, unsigned int offset, unsigned int flags)
{
    struct cb_device_t *cb = (struct cb_device_t *)vp;
    
    if (!(flags & CCD_EXP_FLAGS_TDI))
    {
        /*
         * Skip offset rows.
         */
        offset += cb->vfront_porch;
        while (offset--)
        {
            CB_VCLK(cb);
            if (!(offset & 0x0F))
                cb_clear_row(cb);
        }
    }
    /*
     * Clear row a bunch of times.
     */
    offset = (flags & CCD_EXP_FLAGS_TDI) ? 10 : 2;
    while (offset--)
        cb_clear_row(cb);
}
/*
 * Complete reading a frame (turn off amplifier).
 */
static void cb_end_read(void *vp, unsigned int flags)
{
    struct cb_device_t *cb = (struct cb_device_t *)vp;

    cb = cb; // Turn off compiler warning.
}
/*
 * Clear frame and start integrating.
 */
static void cb_new_frame(void *vp, unsigned int xoffset, unsigned int yoffset, unsigned int width, unsigned int height, unsigned int xbin, unsigned int ybin, unsigned int dac_bits, unsigned int msec, unsigned int flags)
{
    struct cb_device_t *cb = (struct cb_device_t *)vp;

    if (!(flags & CCD_TDI))
    {
        if (!(flags & (CCD_NOWIPE_FRAME | CCD_NOCLEAR_FRAME)))
        {
            cb_clear_frame(cb);
        }
    }
}
/*
 * Latch a frame by shifting into covered half of chip.
 */
static void cb_latch_frame(void *vp, unsigned int flags)
{
    struct cb_device_t *cb = (struct cb_device_t *)vp;
    unsigned int        j;

    for (j = 0; j < cb->vfront_porch + cb->height + cb->vback_porch; j++)
    {
        CB_VCLK(cb);
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
static int cb_open(void *vp)
{
    struct cb_device_t *cb = (struct cb_device_t *)vp;
    
    parport_claim_or_block(cb->pdev);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
    MOD_INC_USE_COUNT;
#endif
    return (0);
}
/*
 * Control CCD device. This is device specific, stuff like temperature control, etc.
 */
static int cb_control(void *vp, unsigned short cmd, unsigned long param)
{
    struct cb_device_t *cb = (struct cb_device_t *)vp;
    
    switch (cmd)
    {
        case CCD_CTRL_CMD_RESET:
            if (cb->state_flags & CB_STATE_READING)
                printk(KERN_INFO "starlight-xpress: reading flag set during reset\n");
            if (cb_wait_idle(cb, 2*HZ))
                printk(KERN_ERR "starlight-xpress: reading flag never unset during reset!\n");
            CB_BEGIN_FRAME(cb);
            CB_WIPE(cb);
            CB_END_FRAME(cb);
            cb_clear_frame(cb);
            break;
        /*
         * Major hack while debugging.
         */
        case CCD_CTRL_CMD_DEC_MOD:
            parport_release(cb->pdev);
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
static int cb_close(void *vp)
{
    struct cb_device_t *cb = (struct cb_device_t *)vp;
    
    if (cb->state_flags & CB_STATE_READING)
        printk(KERN_INFO "starlight-xpress: reading flag set during close\n");
    if (cb_wait_idle(cb, 2*HZ))
        printk(KERN_ERR "starlight-xpress: reading flag never unset during close!\n");
    parport_release(cb->pdev);
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

#define NR_CB 2
static struct cb_device_t cb_devices[NR_CB];
/*
 * Optional parameters.
 */
static int parport[NR_CB] = { [0 ... NR_CB-1] = -1 };
static int model[NR_CB] = { [0 ... NR_CB-1] = CB245  };
/*
 * Per camera dimensions.
 */
static int cb_hfront_porch[]   = {CB211_HFRONT_PORCH,   CB245_HFRONT_PORCH};
static int cb_hback_porch[]    = {CB211_HBACK_PORCH,    CB245_HBACK_PORCH};
static int cb_vfront_porch[]   = {CB211_VFRONT_PORCH,   CB245_VFRONT_PORCH};
static int cb_vback_porch[]    = {CB211_VBACK_PORCH,    CB245_VBACK_PORCH};
static int cb_width[]          = {CB211_WIDTH,          CB245_WIDTH};
static int cb_height[]         = {CB211_HEIGHT,         CB245_HEIGHT};
static int cb_pix_width[]      = {CB211_PIX_WIDTH,      CB245_PIX_WIDTH};
static int cb_pix_height[]     = {CB211_PIX_HEIGHT,     CB245_PIX_HEIGHT};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))
MODULE_AUTHOR("David Schmenk, dschmenk@earthlink.net");
MODULE_DESCRIPTION("Cookbook parallel port astronomy camera driver");
MODULE_LICENSE("GPL");
MODULE_PARM_DESC(parport,"Cookbook camera printer port #");
MODULE_PARM_DESC(model,  "Cookbook camera model");
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
MODULE_PARM(parport, "1-" __MODULE_STRING(NR_CB) "i");
MODULE_PARM(model,   "1-" __MODULE_STRING(NR_CB) "i");
#else
module_param_array(parport, int, NULL, 0);
module_param_array(model, int, NULL, 0);
#endif

unsigned int count = 0;

static void cookbook_attach (struct parport *port)
{
    int j;
    struct ccd_mini mini_device;
    for (j = 0; j < NR_CB; j++)
    if (port->number == parport[j])
    {
        j                                = model[count] ? CB245 : CB211;
        cb_devices[count].state_flags    = 0;
        cb_devices[count].pdev           = parport_register_device(port, "cb", NULL, NULL, NULL, 0, NULL);
        cb_devices[count].pport          = port;
        cb_devices[count].hfront_porch   = cb_hfront_porch[j];
        cb_devices[count].hback_porch    = cb_hback_porch[j];
        cb_devices[count].vfront_porch   = cb_vfront_porch[j];
        cb_devices[count].vback_porch    = cb_vback_porch[j];
        cb_devices[count].width          = cb_width[j];
        cb_devices[count].height         = cb_height[j];
        strcpy(mini_device.id_string,    "CookBook ");
        strcat(mini_device.id_string, model[count] ? "211" : "245");
        mini_device.version            = CCD_VERSION;
        mini_device.width              = cb_width[j];
        mini_device.height             = cb_height[j];
        mini_device.pixel_width        = cb_pix_width[j];
        mini_device.pixel_height       = cb_pix_height[j];
        mini_device.image_fields       = 1;
        mini_device.image_depth        = 16; 
        mini_device.dac_bits           = 12;
        mini_device.color_format       = CCD_COLOR_MONOCHROME;
        mini_device.flag_caps          = CCD_EXP_FLAGS_NOBIN_ACCUM | CCD_EXP_FLAGS_NOWIPE_FRAME | CCD_EXP_FLAGS_TDI | CCD_EXP_FLAGS_NOCLEAR_FRAME;
        mini_device.open               = cb_open;
        mini_device.control            = cb_control;
        mini_device.close              = cb_close;
        mini_device.read_row           = cb_read_row;
        mini_device.begin_read         = cb_begin_read;
        mini_device.end_read           = cb_end_read;
        mini_device.latch_frame        = cb_latch_frame;
        mini_device.new_frame          = cb_new_frame;
        if (ccd_register_device(&mini_device, (void *)&cb_devices[count]) < 0)
        {
            printk(KERN_ERR "cookbook ccd: unable to register\n");
            return;
        }
        printk(KERN_INFO "cookbook %d on port %d.\n", model[count] ? 245 : 211, port->number);
        count++;
        break;
    }
}

static void cookbook_detach (struct parport *port)
{
    printk(KERN_INFO "starlight-xpress ccd: detaching parport %i\n", port->number);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
static struct list_head cookbook_driver_head = LIST_HEAD_INIT(cookbook_driver_head);
#endif

static struct parport_driver cookbook_driver;

int init_module(void)
{
    unsigned int    i;

    for (i = 0; i < NR_CB; i++)
        cb_devices[i].pdev = NULL;
 
    cookbook_driver.name = "cookbook";
    cookbook_driver.attach = cookbook_attach;
    cookbook_driver.detach = cookbook_detach;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,3))
    cookbook_driver.next = NULL;
#else
    cookbook_driver.list = cookbook_driver_head;
#endif

    /*
     * Default to parport0 if no options given.
     */
    if (parport[0] == -1)
        parport[0] = 0;

    // that will call attach for every founded parport
    if (parport_register_driver (&cookbook_driver))
        return -EIO;
    
    if (count == 0)
    {
        printk(KERN_INFO "cookbook ccd: no devices found\n");
        return (-ENODEV);
    }
    return (0);
}

void cleanup_module(void)
{
    int i;

    for (i = 0; i < NR_CB; i++)
    {
        if (cb_devices[i].pdev)
        {
            parport_unregister_device(cb_devices[i].pdev);
            ccd_unregister_device((void *)&cb_devices[i]);
        }
    }
}


