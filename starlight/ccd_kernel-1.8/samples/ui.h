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

#include <vga.h>
#include <vgakeyboard.h>
#include <vgamouse.h>
#define MAX_KEY 127
/*
 * Save top to CLUT indices for the UI.
 */
#define GFX_FILL_RECT   2
#define GFX_XOR_PIXEL   1
#define GFX_COPY_PIXEL  0
#define UI_MAX_PIXEL    253
#define UI_LO_COLOR     254
#define UI_HI_COLOR     255
#define UI_NO_EVENT     0
#define UI_KEY_EVENT    1
#define UI_MOUSE_EVENT  2
/*
 * Placements of various UI components.
 */
#define FRAME_SCREEN_LEFT   0
#define FRAME_SCREEN_RIGHT  639
#define FRAME_SCREEN_TOP    0
#define FRAME_SCREEN_BOTTOM 479
#define FRAME_SCREEN_WIDTH  (FRAME_SCREEN_RIGHT - FRAME_SCREEN_LEFT)
#define FRAME_SCREEN_HEIGHT (FRAME_SCREEN_BOTTOM - FRAME_SCREEN_TOP)
/*
 * CCD image.
 */
#define FRAME_IMAGE_BORDER  2
#define FRAME_IMAGE_WIDTH   512
#define FRAME_IMAGE_HEIGHT  384
#define FRAME_IMAGE_LEFT    (FRAME_SCREEN_LEFT + FRAME_IMAGE_BORDER)
#define FRAME_IMAGE_TOP     (FRAME_SCREEN_TOP  + FRAME_IMAGE_BORDER)
#define FRAME_IMAGE_RIGHT   (FRAME_IMAGE_LEFT  + FRAME_IMAGE_WIDTH)
#define FRAME_IMAGE_BOTTOM  (FRAME_IMAGE_TOP   + FRAME_IMAGE_HEIGHT)
/*
 * CCD image histogram.
 */
#define FRAME_HIST_BORDER   2
#define FRAME_HIST_WIDTH    256
#define FRAME_HIST_TOP      (FRAME_IMAGE_BOTTOM  + FRAME_IMAGE_BORDER + FRAME_HIST_BORDER)
#define FRAME_HIST_LEFT     (FRAME_SCREEN_LEFT   + FRAME_HIST_BORDER)
#define FRAME_HIST_BOTTOM   (FRAME_SCREEN_BOTTOM - FRAME_HIST_BORDER)
#define FRAME_HIST_RIGHT    (FRAME_HIST_LEFT     + FRAME_HIST_WIDTH)
#define FRAME_HIST_HEIGHT   (FRAME_HIST_BOTTOM   - FRAME_HIST_TOP)
/*
 * CCD image loading feedback.
 */
#define FRAME_LOAD_BORDER   2
#define FRAME_LOAD_HEIGHT   FRAME_IMAGE_HEIGHT
#define FRAME_LOAD_WIDTH    6
#define FRAME_LOAD_LEFT     (FRAME_IMAGE_RIGHT + FRAME_IMAGE_BORDER + FRAME_LOAD_BORDER)
#define FRAME_LOAD_RIGHT    (FRAME_LOAD_LEFT   + FRAME_LOAD_WIDTH)
#define FRAME_LOAD_TOP      (FRAME_SCREEN_TOP  + FRAME_LOAD_BORDER)
#define FRAME_LOAD_BOTTOM   (FRAME_IMAGE_TOP   + FRAME_IMAGE_HEIGHT)
/*
 * Exposure meter.
 */
#define FRAME_EXPO_BORDER   2
#define FRAME_EXPO_HEIGHT   FRAME_IMAGE_HEIGHT
#define FRAME_EXPO_RIGHT    (FRAME_SCREEN_RIGHT - FRAME_EXPO_BORDER)
#define FRAME_EXPO_LEFT     (FRAME_LOAD_RIGHT   + FRAME_LOAD_BORDER + FRAME_EXPO_BORDER)
#define FRAME_EXPO_WIDTH    (FRAME_EXPO_RIGHT   - FRAME_EXPO_LEFT)
#define FRAME_EXPO_TOP      (FRAME_SCREEN_TOP   + FRAME_EXPO_BORDER)
#define FRAME_EXPO_BOTTOM   (FRAME_EXPO_TOP     + FRAME_EXPO_HEIGHT)
/*
 * Guide star image.
 */
#define FRAME_GUIDE_BORDER  2
#define FRAME_GUIDE_TOP     (FRAME_IMAGE_BOTTOM + FRAME_IMAGE_BORDER + FRAME_GUIDE_BORDER)
#define FRAME_GUIDE_BOTTOM  (FRAME_SCREEN_BOTTOM - FRAME_GUIDE_BORDER)
#define FRAME_GUIDE_HEIGHT  (FRAME_GUIDE_BOTTOM  - FRAME_GUIDE_TOP)
#define FRAME_GUIDE_WIDTH   FRAME_GUIDE_HEIGHT
#define FRAME_GUIDE_LEFT    (FRAME_HIST_RIGHT    + FRAME_HIST_BORDER +  FRAME_GUIDE_BORDER)
#define FRAME_GUIDE_RIGHT   (FRAME_GUIDE_LEFT    + FRAME_GUIDE_WIDTH)
/*
 * Arrow panel.
 */
#define FRAME_ARROW_BORDER  2
#define FRAME_ARROW_TOP     (FRAME_IMAGE_BOTTOM  + FRAME_IMAGE_BORDER + FRAME_ARROW_BORDER)
#define FRAME_ARROW_BOTTOM  (FRAME_SCREEN_BOTTOM - FRAME_ARROW_BORDER)
#define FRAME_ARROW_HEIGHT  (FRAME_ARROW_BOTTOM - FRAME_ARROW_TOP)
#define FRAME_ARROW_LEFT    (FRAME_GUIDE_RIGHT   + FRAME_GUIDE_BORDER +  FRAME_ARROW_BORDER)
#define FRAME_ARROW_RIGHT   (FRAME_SCREEN_RIGHT  - FRAME_ARROW_BORDER)
#define FRAME_ARROW_WIDTH   (FRAME_ARROW_RIGHT  - FRAME_ARROW_LEFT)
#define ARROW_LEFT  0
#define ARROW_RIGHT 1
#define ARROW_UP    2
#define ARROW_DOWN  3
/*
 * Mouse state passed from ui_event.
 */
struct mouse_state
{
    int x, y, button;
};

void gfx_hline(int x1, int x2, int y, int color, int flags);
void gfx_vline(int y1, int y2, int x, int color, int flags);
void gfx_rect(int x1, int y1, int x2, int y2, int color, int flags);
void gfx_frame(int x1, int y1, int x2, int y2, int width, int hicolor, int locolor, int flags);
void gfx_putimage(unsigned char *image, int xpar, int xsrc, int ysrc, int width, int height, int pitch, int xdst, int ydst);
void gfx_puttextimage(char *textimage, int base, char * text2color, int xpar, int width, int height, int xdst, int ydst);
int ui_init();
void ui_exit();
int ui_event(unsigned *param);
void ui_show_hide_mouse();
void ui_update_mouse(int x, int y, int buttons);
void ui_draw_load();
void ui_update_load(int scanline);
void ui_draw_ccd_image();
void ui_update_ccd_image(unsigned char *image, unsigned int xoffset, unsigned int yoffset, unsigned int width, unsigned height, int contrast_stretch);
void ui_draw_expo();
void ui_update_expo(int exposure_time, int current_time);
void ui_draw_arrows();
void ui_update_arrows(int arrow, int hilite);
void ui_draw_guide_image();
void ui_update_guide_image(unsigned char *image, unsigned int width, unsigned height);

