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

#include "ui.h"
/*
 * Basic graphics routines.
 */
void gfx_hline(int x1, int x2, int y, int color, int flags)
{
    int x;
    
    if (flags & GFX_XOR_PIXEL)
    {
        for (x = x1; x < x2; x++)
        {
            vga_setcolor(color ^ vga_getpixel(x, y));
            vga_drawpixel(x, y);
        }
    }
    else
    {
        vga_setcolor(color);
        for (x = x1; x < x2; x++)
            vga_drawpixel(x, y);
    }
}
void gfx_vline(int y1, int y2, int x, int color, int flags)
{
    int y;
    
    if (flags & GFX_XOR_PIXEL)
    {
        for (y = y1; y < y2; y++)
        {
            vga_setcolor(color ^ vga_getpixel(x, y));
            vga_drawpixel(x, y);
        }
    }
    else
    {
        vga_setcolor(color);
        for (y = y1; y < y2; y++)
            vga_drawpixel(x, y);
    }
}
void gfx_rect(int x1, int y1, int x2, int y2, int color, int flags)
{
    int y;
    
    if (flags & GFX_FILL_RECT)
    {
        for (y = y1; y < y2; y++)
            gfx_hline(x1, x2, y, color, flags);
    }
    else
    {
        gfx_hline(x1, x2, y1, color, flags);
        gfx_hline(x1, x2, y2, color, flags);
        gfx_vline(y1, y2, x1, color, flags);
        gfx_vline(y1, y2, x2, color, flags);
    }
}
void gfx_frame(int x1, int y1, int x2, int y2, int width, int hicolor, int locolor, int flags)
{
    int x;

    x2--;
    y2--;
    for (x = 0; x < width; x++)
    {
        gfx_hline(x1 + x, x2 - x,     y1 + x, hicolor, flags);
        gfx_hline(x1 + x, x2 - x + 1, y2 - x, locolor, flags);
        gfx_vline(y1 + x, y2 - x,     x1 + x, hicolor, flags);
        gfx_vline(y1 + x, y2 - x,     x2 - x, locolor, flags);
    }
}
void gfx_putimage(unsigned char *image, int xpar, int xsrc, int ysrc, int width, int height, int pitch, int xdst, int ydst)
{
    int x, y;

    image += ysrc * pitch + xsrc;
    for (y = 0; y < height; y++)
    {
        if (xpar >= 0)
        {
            for (x = 0; x < width; x++)
                if (image[x] != xpar)
                {
                    vga_setcolor(image[x]);
                    vga_drawpixel(x, y);
                }
        }
        else
            vga_drawscansegment(image, xdst, ydst + y, width);
        image += pitch;
    }
}
void gfx_puttextimage(char *textimage, int base, char * text2color, int xpar, int width, int height, int xdst, int ydst)
{
    int x, y, color;
    int xlate[256];

    for (x = 0; text2color[x]; x++)
        xlate[(int)text2color[x]] = x + base;
    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
        {
            color = xlate[(int)*textimage++];
            if (color != xpar)
            {
                vga_setcolor(color);
                vga_drawpixel(x, y);
            }
        }
}
/*
 * User interface routines.
 */
#define MAX_KEY 127
static struct mouse_state mouse;
static int    mouse_avail, ui_quit = 1;
/*
 * Show/Hide mouse cursor.
 */
void ui_crosshair(int x, int y)
{
    gfx_hline(x - 5, x + 5, y, 0xFF, GFX_XOR_PIXEL);
    gfx_vline(y - 5, y + 5, x, 0xFF, GFX_XOR_PIXEL);
}
void ui_show_hide_mouse()
{
    ui_crosshair(mouse.x, mouse.y);
}
void ui_update_mouse(int x, int y, int buttons)
{
    mouse.x      = x;
    mouse.y      = y;
    mouse.button = buttons;
}
/*
 * Update frame load UI.
 */
static void ui_init_load()
{
}
void ui_draw_load()
{
    ui_show_hide_mouse();
    gfx_frame(FRAME_LOAD_LEFT - FRAME_LOAD_BORDER, FRAME_LOAD_TOP - FRAME_LOAD_BORDER, FRAME_LOAD_RIGHT + FRAME_LOAD_BORDER, FRAME_LOAD_BOTTOM + FRAME_LOAD_BORDER, FRAME_LOAD_BORDER, UI_LO_COLOR, UI_LO_COLOR, 0);
    gfx_rect(FRAME_LOAD_LEFT, FRAME_LOAD_TOP, FRAME_LOAD_RIGHT, FRAME_LOAD_BOTTOM, 0, GFX_FILL_RECT);
    ui_show_hide_mouse();
}
void ui_update_load(int scanline)
{
    ui_show_hide_mouse();
    if (scanline > 0)
        gfx_hline(FRAME_LOAD_LEFT, FRAME_LOAD_RIGHT, scanline + FRAME_LOAD_TOP, UI_HI_COLOR, GFX_COPY_PIXEL);
    else
        gfx_rect(FRAME_LOAD_LEFT, FRAME_LOAD_TOP, FRAME_LOAD_RIGHT, FRAME_LOAD_BOTTOM, 0, GFX_FILL_RECT);
    ui_show_hide_mouse();
}
/*
 * CCD image and histogram.
 */
#define IMAGE_BACKGROUND  10

static unsigned char ccd_image[FRAME_IMAGE_WIDTH * FRAME_IMAGE_HEIGHT];
static int           histogram[256];
static void ui_init_ccd_image()
{
    int j;

    for (j = 0; j < 256; j++)
        histogram[j] = 0;
    for (j = 0; j < FRAME_IMAGE_WIDTH * FRAME_IMAGE_HEIGHT; j++)
        ccd_image[j] = 0;
}
void ui_draw_ccd_image()
{
    int j, max, scaled;

    ui_show_hide_mouse();
    /*
     * Draw frames.
     */
    gfx_frame(FRAME_HIST_LEFT - FRAME_HIST_BORDER, FRAME_HIST_TOP - FRAME_HIST_BORDER, FRAME_HIST_RIGHT + FRAME_HIST_BORDER, FRAME_HIST_BOTTOM + FRAME_HIST_BORDER, FRAME_HIST_BORDER, UI_LO_COLOR, UI_LO_COLOR, 0);
    gfx_frame(FRAME_IMAGE_LEFT - FRAME_IMAGE_BORDER, FRAME_IMAGE_TOP - FRAME_IMAGE_BORDER, FRAME_IMAGE_RIGHT + FRAME_IMAGE_BORDER, FRAME_IMAGE_BOTTOM + FRAME_IMAGE_BORDER, FRAME_IMAGE_BORDER, UI_LO_COLOR, UI_LO_COLOR, 0);
    /*
     * Find max value for scaling.
     */
    max = 0;
    for (j = 0; j < 256; j++)
        max = histogram[j] > max ? histogram[j] : max;
    if (max == 0)
        max = 1;
    /*
     * Draw histogram.
     */
    for (j = 0; j < 256; j++)
    {
        scaled = histogram[j] * (FRAME_HIST_HEIGHT) / max;
        gfx_vline(FRAME_HIST_BOTTOM - scaled, FRAME_HIST_BOTTOM, j + FRAME_HIST_LEFT, j > UI_MAX_PIXEL ? UI_MAX_PIXEL : j, GFX_COPY_PIXEL);
        gfx_vline(FRAME_HIST_TOP, FRAME_HIST_BOTTOM - scaled,    j + FRAME_HIST_LEFT, UI_LO_COLOR , GFX_COPY_PIXEL);
    }
    /*
     * Draw image.
     */
    gfx_putimage(ccd_image, -1, 0, 0, FRAME_IMAGE_WIDTH, FRAME_IMAGE_HEIGHT, FRAME_IMAGE_WIDTH, FRAME_IMAGE_LEFT, FRAME_IMAGE_TOP);
    ui_show_hide_mouse();
}
void ui_update_ccd_image(unsigned char *image, unsigned int xoffset, unsigned int yoffset, unsigned int width, unsigned height, int contrast_stretch)
{
    int             i, j, stretch, background, pix;
    unsigned short *ccd_src = (unsigned short *)image;
    unsigned char  *ccd_dst = ccd_image + yoffset * FRAME_IMAGE_WIDTH + xoffset;
    
    /*
     * Clear image and histogram.
     */
    for (j = 0; j < 256; j++)
        histogram[j] = 0;
    for (j = 0; j < FRAME_IMAGE_WIDTH * FRAME_IMAGE_HEIGHT; j++)
        ccd_image[j] = 0;
    /*
     * Get histogram of CCD image.
     */
    for (j = 0; j < height * width; j++)
        histogram[ccd_src[j] >> 8]++;
    /*
     * Convert CCD image to 8BPP.
     */
    if (contrast_stretch)
    {
        background = stretch = i = 0;
        for (j = 0; j < 256; j++)
        {
            /*
             * Accumulate 10% of the image and call that the background.
             */
            i += histogram[j];
            if (background == 0 && i > FRAME_IMAGE_WIDTH * FRAME_IMAGE_HEIGHT / IMAGE_BACKGROUND)
                background = j;
            if (histogram[j])
                stretch = j;
        }
        background--;
        if (background < 0)
            background = 0;
        if (background > 127)
            background = 127;
        stretch = stretch - background + 1;
        if (stretch < 1)
            stretch = 65536;
        else
            stretch = 65536 / stretch;
        background <<= 8;
    }
    else
    {
        background = 0;
        stretch    = 256;
    }
    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width; i++)
        {
            if ((pix = (*ccd_src++ - background)) < 0)
                pix = 0;
            pix = (pix * stretch) >> 16;
            ccd_dst[i] = pix > UI_MAX_PIXEL ? UI_MAX_PIXEL : pix;
        }
        ccd_dst += FRAME_IMAGE_WIDTH;
    }
    ui_draw_ccd_image();
}
/*
 * Convert msec to graduated scale. 100 msec ... 1 hour -> 0 ... 300
 */
static int msec2scale(int msec)
{
    int scale;

    if (msec < 100)
        scale = msec / 2;
    else if (msec < 1000)
        scale = msec / 20 + 50;
    else if (msec < 60000)
        scale = msec / 600 + 100;
    else if (msec < 3600000)
        scale = msec /  36000 + 200;
    else
        scale = 300;
    return (scale);
}
/*
 * Convert scale to msec.  0 ... 300 -> 100 msec ... 1 hour
 */
static int scale2msec(int scale)
{
    int msec;

    if (scale < 100)
        msec = scale * 10;
    else if (scale < 200)
        msec = (scale - 100) * 600;
    else if (scale < 300)
        msec = (scale - 200) *  36000;
    else
        msec = 3600000;
    return (msec);
}
/*
 * Exposure meter.
 */
#define EXPO_VERT_SCALE(s) ((s) * (FRAME_EXPO_BOTTOM - FRAME_EXPO_TOP) / 300)
unsigned int exposure, elapsed;
static void ui_init_expo()
{
    exposure = 100;
    elapsed  = 0;
}
void ui_draw_expo()
{
    int scale_exp, scale_elp;

    /*
     * Convert time values into logarithm scale.
     */
    scale_exp = FRAME_EXPO_BOTTOM - EXPO_VERT_SCALE(msec2scale(exposure));
    scale_elp = FRAME_EXPO_BOTTOM - EXPO_VERT_SCALE(msec2scale(elapsed));
    ui_show_hide_mouse();
    gfx_frame(FRAME_EXPO_LEFT - FRAME_EXPO_BORDER, FRAME_EXPO_TOP - FRAME_EXPO_BORDER, FRAME_EXPO_RIGHT + FRAME_EXPO_BORDER, FRAME_EXPO_BOTTOM + FRAME_EXPO_BORDER, FRAME_EXPO_BORDER, UI_LO_COLOR, UI_LO_COLOR, 0);
    gfx_rect(FRAME_EXPO_LEFT + FRAME_EXPO_WIDTH / 2, FRAME_EXPO_TOP - 1, FRAME_EXPO_RIGHT - 1, FRAME_EXPO_BOTTOM - 1, 0, GFX_FILL_RECT);
    gfx_rect(FRAME_EXPO_LEFT, FRAME_EXPO_TOP, FRAME_EXPO_RIGHT - FRAME_EXPO_WIDTH / 2, scale_exp, 0, GFX_FILL_RECT);
    gfx_rect(FRAME_EXPO_LEFT, scale_exp, FRAME_EXPO_RIGHT - FRAME_EXPO_WIDTH / 2, scale_elp, UI_LO_COLOR, GFX_FILL_RECT);
    gfx_rect(FRAME_EXPO_LEFT, scale_elp, FRAME_EXPO_RIGHT - FRAME_EXPO_WIDTH / 2, FRAME_EXPO_BOTTOM, UI_HI_COLOR, GFX_FILL_RECT);
    gfx_vline(FRAME_EXPO_TOP, FRAME_EXPO_BOTTOM, FRAME_EXPO_RIGHT, UI_HI_COLOR, GFX_COPY_PIXEL);
    gfx_hline(FRAME_EXPO_RIGHT - FRAME_EXPO_WIDTH/2, FRAME_EXPO_RIGHT, FRAME_EXPO_TOP, UI_HI_COLOR, GFX_COPY_PIXEL);
    gfx_hline(FRAME_EXPO_RIGHT - FRAME_EXPO_WIDTH/2, FRAME_EXPO_RIGHT, FRAME_EXPO_BOTTOM, UI_HI_COLOR, GFX_COPY_PIXEL);
    gfx_hline(FRAME_EXPO_RIGHT - FRAME_EXPO_WIDTH/2, FRAME_EXPO_RIGHT, FRAME_EXPO_BOTTOM - EXPO_VERT_SCALE(100), UI_HI_COLOR, GFX_COPY_PIXEL);
    gfx_hline(FRAME_EXPO_RIGHT - FRAME_EXPO_WIDTH/2, FRAME_EXPO_RIGHT, FRAME_EXPO_BOTTOM - EXPO_VERT_SCALE(50), UI_HI_COLOR, GFX_COPY_PIXEL);
    gfx_hline(FRAME_EXPO_RIGHT - FRAME_EXPO_WIDTH/2, FRAME_EXPO_RIGHT, FRAME_EXPO_BOTTOM - EXPO_VERT_SCALE(200), UI_HI_COLOR, GFX_COPY_PIXEL);
    gfx_hline(FRAME_EXPO_RIGHT - FRAME_EXPO_WIDTH/2, FRAME_EXPO_RIGHT, FRAME_EXPO_BOTTOM - EXPO_VERT_SCALE(300), UI_HI_COLOR, GFX_COPY_PIXEL);
    gfx_hline(FRAME_EXPO_RIGHT - FRAME_EXPO_WIDTH/4, FRAME_EXPO_RIGHT, FRAME_EXPO_BOTTOM - EXPO_VERT_SCALE(25), UI_HI_COLOR, GFX_COPY_PIXEL);
    gfx_hline(FRAME_EXPO_RIGHT - FRAME_EXPO_WIDTH/4, FRAME_EXPO_RIGHT, FRAME_EXPO_BOTTOM - EXPO_VERT_SCALE(75), UI_HI_COLOR, GFX_COPY_PIXEL);
    gfx_hline(FRAME_EXPO_RIGHT - FRAME_EXPO_WIDTH/4, FRAME_EXPO_RIGHT, FRAME_EXPO_BOTTOM - EXPO_VERT_SCALE(150), UI_HI_COLOR, GFX_COPY_PIXEL);
    gfx_hline(FRAME_EXPO_RIGHT - FRAME_EXPO_WIDTH/4, FRAME_EXPO_RIGHT, FRAME_EXPO_BOTTOM - EXPO_VERT_SCALE(250), UI_HI_COLOR, GFX_COPY_PIXEL);
    gfx_hline(FRAME_EXPO_RIGHT - FRAME_EXPO_WIDTH/8, FRAME_EXPO_RIGHT, FRAME_EXPO_BOTTOM - EXPO_VERT_SCALE(125), UI_HI_COLOR, GFX_COPY_PIXEL);
    gfx_hline(FRAME_EXPO_RIGHT - FRAME_EXPO_WIDTH/8, FRAME_EXPO_RIGHT, FRAME_EXPO_BOTTOM - EXPO_VERT_SCALE(175), UI_HI_COLOR, GFX_COPY_PIXEL);
    gfx_hline(FRAME_EXPO_RIGHT - FRAME_EXPO_WIDTH/8, FRAME_EXPO_RIGHT, FRAME_EXPO_BOTTOM - EXPO_VERT_SCALE(225), UI_HI_COLOR, GFX_COPY_PIXEL);
    gfx_hline(FRAME_EXPO_RIGHT - FRAME_EXPO_WIDTH/8, FRAME_EXPO_RIGHT, FRAME_EXPO_BOTTOM - EXPO_VERT_SCALE(275), UI_HI_COLOR, GFX_COPY_PIXEL);
    ui_show_hide_mouse();
}
void ui_update_expo(int exposure_time, int remaining_time)
{
    int elapsed_time = remaining_time > 0 ? exposure_time - remaining_time : 0;
    if (elapsed_time < 0)
        elapsed_time = 0;
    if (msec2scale(exposure_time) != msec2scale(exposure) || msec2scale(elapsed_time) != msec2scale(elapsed))
    {
        int scale_exp, scale_elp;

        /*
         * Convert time values into logarithm scale.
         */
        exposure = exposure_time;
        elapsed  = elapsed_time;
        scale_exp = FRAME_EXPO_BOTTOM - EXPO_VERT_SCALE(msec2scale(exposure));
        scale_elp = FRAME_EXPO_BOTTOM - EXPO_VERT_SCALE(msec2scale(elapsed));
        ui_show_hide_mouse();
        gfx_rect(FRAME_EXPO_LEFT, FRAME_EXPO_TOP, FRAME_EXPO_RIGHT - FRAME_EXPO_WIDTH / 2, scale_exp, 0, GFX_FILL_RECT);
        gfx_rect(FRAME_EXPO_LEFT, scale_exp, FRAME_EXPO_RIGHT - FRAME_EXPO_WIDTH / 2, scale_elp, UI_LO_COLOR, GFX_FILL_RECT);
        gfx_rect(FRAME_EXPO_LEFT, scale_elp, FRAME_EXPO_RIGHT - FRAME_EXPO_WIDTH / 2, FRAME_EXPO_BOTTOM, UI_HI_COLOR, GFX_FILL_RECT);
        ui_show_hide_mouse();
    }
}
/*
 * Guide star image and slew arrows.
 */
static int arrows[4] = {UI_LO_COLOR, UI_LO_COLOR, UI_LO_COLOR, UI_LO_COLOR};
static void ui_init_arrows()
{
}
void ui_draw_arrows()
{
    int i, min_size;
    
    min_size = (FRAME_ARROW_HEIGHT < FRAME_ARROW_WIDTH) ? FRAME_ARROW_HEIGHT / 3 : FRAME_ARROW_WIDTH / 3;
    ui_show_hide_mouse();
    /*
     * Up & down arrows.
     */
    for (i = 0; i < min_size; i++)
    {
        gfx_hline(FRAME_ARROW_LEFT + FRAME_ARROW_WIDTH/2 - i, FRAME_ARROW_LEFT + FRAME_ARROW_WIDTH/2 + i, FRAME_ARROW_TOP    + 1 + i, arrows[ARROW_UP], 0);
        gfx_hline(FRAME_ARROW_LEFT + FRAME_ARROW_WIDTH/2 - i, FRAME_ARROW_LEFT + FRAME_ARROW_WIDTH/2 + i, FRAME_ARROW_BOTTOM - 1 - i, arrows[ARROW_DOWN], 0);
    }
    /*
     * Left & right arrows.
     */
    for (i = 0; i < min_size; i++)
    {
        gfx_vline(FRAME_ARROW_TOP + FRAME_ARROW_HEIGHT/2 - i, FRAME_ARROW_TOP + FRAME_ARROW_HEIGHT/2 + i, FRAME_ARROW_LEFT  + 1 + i, arrows[ARROW_LEFT], 0);
        gfx_vline(FRAME_ARROW_TOP + FRAME_ARROW_HEIGHT/2 - i, FRAME_ARROW_TOP + FRAME_ARROW_HEIGHT/2 + i, FRAME_ARROW_RIGHT - 1 - i, arrows[ARROW_RIGHT], 0);
    }
    ui_show_hide_mouse();
}
void ui_update_arrows(int dir, int hilite)
{
    if (dir > ARROW_DOWN || dir < ARROW_LEFT)
        return;
    arrows[dir] = hilite ? UI_HI_COLOR : UI_LO_COLOR;
    ui_draw_arrows();
}
static unsigned char guide_image[FRAME_GUIDE_WIDTH * FRAME_GUIDE_HEIGHT];
static void ui_init_guide_image()
{
    int j;

    for (j = 0; j < FRAME_GUIDE_WIDTH * FRAME_GUIDE_HEIGHT; j++)
        guide_image[j] = 0;
    for (j = 0; j < FRAME_GUIDE_HEIGHT; j++)
        guide_image[j * FRAME_GUIDE_WIDTH + FRAME_GUIDE_WIDTH / 2] = UI_HI_COLOR;
    for (j = 0; j < FRAME_GUIDE_WIDTH; j++)
        guide_image[FRAME_GUIDE_HEIGHT / 2 * FRAME_GUIDE_WIDTH + j] = UI_HI_COLOR;
}
void ui_draw_guide_image()
{
    ui_show_hide_mouse();
    gfx_frame(FRAME_GUIDE_LEFT - FRAME_GUIDE_BORDER, FRAME_GUIDE_TOP - FRAME_GUIDE_BORDER, FRAME_GUIDE_RIGHT + FRAME_GUIDE_BORDER, FRAME_GUIDE_BOTTOM + FRAME_GUIDE_BORDER, FRAME_GUIDE_BORDER, UI_LO_COLOR, UI_LO_COLOR, 0);
    gfx_putimage(guide_image, -1, 0, 0, FRAME_GUIDE_WIDTH, FRAME_GUIDE_HEIGHT, FRAME_GUIDE_WIDTH, FRAME_GUIDE_LEFT, FRAME_GUIDE_TOP);
    ui_show_hide_mouse();
}
void ui_update_guide_image(unsigned char *image, unsigned int width, unsigned height)
{
    int             i, j, stretch, background, pix, guide_hist[256];
    unsigned short *guide_src = (unsigned short *)image;
    unsigned char  *guide_dst = guide_image + (FRAME_GUIDE_HEIGHT - height) / 2 * FRAME_GUIDE_WIDTH + (FRAME_GUIDE_WIDTH - width) /2;
    
    /*
     * Clear image histogram.
     */
    for (j = 0; j < 256; j++)
        guide_hist[j] = 0;
    for (j = 0; j < FRAME_GUIDE_WIDTH * FRAME_GUIDE_HEIGHT; j++)
        guide_image[j] = 0;
    /*
     * Get histogram of CCD image.
     */
    for (j = 0; j < height * width; j++)
        guide_hist[guide_src[j] >> 8]++;
    /*
     * Convert CCD image to 8BPP.
     */
    background = stretch = i = 0;
    for (j = 0; j < 256; j++)
    {
        /*
         * Accumulate 50% of the image and call that the background.
         */
        i += guide_hist[j];
        if (background == 0 && i > FRAME_GUIDE_WIDTH * FRAME_GUIDE_HEIGHT / 2)
            background = j;
        if (guide_hist[j])
            stretch = j;
    }
    if (background < 0)
        background = 0;
    if (background > 127)
        background = 127;
    stretch = stretch - background + 1;
    if (stretch < 1)
        stretch = 65536;
    else
        stretch = 65536 / stretch;
    background <<= 8;
    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width; i++)
        {
            if ((pix = (*guide_src++ - background)) < 0)
                pix = 0;
            pix = (pix * stretch) >> 16;
            guide_dst[i] = pix > UI_MAX_PIXEL ? UI_MAX_PIXEL : pix;
        }
        guide_dst += FRAME_GUIDE_WIDTH;
    }
    /*
     * Draw reticule cross hair.
     */
    for (j = 0; j < FRAME_GUIDE_HEIGHT; j++)
        guide_image[j * FRAME_GUIDE_WIDTH + FRAME_GUIDE_WIDTH / 2] = UI_HI_COLOR;
    for (i = 0; i < FRAME_GUIDE_WIDTH; i++)
        guide_image[FRAME_GUIDE_HEIGHT / 2 * FRAME_GUIDE_WIDTH + i] = UI_HI_COLOR;
    ui_draw_guide_image();
}
/*
 * Draw all UI components.
 */
void ui_draw()
{
    ui_show_hide_mouse();
    ui_draw_load();
    ui_draw_ccd_image();
    ui_draw_expo();
    ui_draw_guide_image();
    ui_draw_arrows();
}
/*
 * Init and exit UI.
 */
static vga_modeinfo *vga;
static int           old_mode;
int ui_init(int *fd_keyboard, int *fd_mouse)
{
    int dac_shift, i, mouse_type, force_red;
    
    if (vga_init() == 0)
    {
        old_mode = vga_getcurrentmode();
        vga_setmode(G640x480x256);
        /*vga_setlinearaddressing();*/
        if (vga_ext_set(VGA_EXT_AVAILABLE, VGA_AVAIL_FLAGS) & VGA_CLUT8)
        {
            vga_ext_set(VGA_EXT_SET, VGA_CLUT8);
            dac_shift = 0;
        }
        else
            dac_shift = 2;
        force_red = 0xFF;
        for (i = 0; i < 255; i++)
            vga_setpalette(i,
                           i >> dac_shift,
                           (i & force_red) >> dac_shift,
                           (i & force_red) >> dac_shift);
        vga_setpalette(UI_LO_COLOR, 127 >> dac_shift, 0, 0); /* Save dim    red for UI */
        vga_setpalette(UI_HI_COLOR, 255 >> dac_shift, 0, 0); /* Save bright red for UI */
        vga = vga_getmodeinfo(vga_getcurrentmode());
    }
    else
        return (-1);
    /*
     * Set up keyboard.
     */
    if ((*fd_keyboard = keyboard_init_return_fd()) < 0)
        return (-1);
    keyboard_translatekeys(TRANSLATE_CURSORKEYS | TRANSLATE_DIAGONAL | TRANSLATE_KEYPADENTER);
    /*
     * Set up mouse.
     */
    vga_setmousesupport(1);
    mouse_type = MOUSE_PS2;
    if ((*fd_mouse = mouse_init_return_fd("/dev/mouse", mouse_type, MOUSE_DEFAULTSAMPLERATE)) < 0)
    {
        mouse_avail =  0;
        *fd_mouse   = -1;
    }
    else
    {
        mouse_avail = 1;
        mouse_setposition(vga->width  / 2, vga->height / 2);
        mouse_setxrange(0, vga->width - 1);
        mouse_setyrange(0, vga->height - 1);
        mouse_setscale(30);
    }
    mouse.x = FRAME_SCREEN_WIDTH  / 2;
    mouse.y = FRAME_SCREEN_HEIGHT / 2;
    ui_quit = 0;
    /*
     * Init UI components.
     */
    ui_init_load();
    ui_init_ccd_image();
    ui_init_expo();
    ui_init_guide_image();
    ui_init_arrows();
    /*
     * Draw UI.
     */
    ui_draw();
    return (0);
}
void ui_exit()
{
    keyboard_close();
    if (mouse_avail)
        mouse_close();
    vga_setmode(old_mode);
    ui_quit = 1;
}

