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

/***************************************************************************\
*                                                                           *
*                    Cookbook basic control macros                          *
*                                                                           *
\***************************************************************************/
#ifndef _CB_PARPORT_H_
#define _CB_PARPORT_H_

/*
 * The different cookbook models
 */
#define CB211                   0
#define CB211_WIDTH             192
#define CB211_HEIGHT            165
#define CB211_PIX_WIDTH         (int)(17*256)
#define CB211_PIX_HEIGHT        (int)(20*256)
#define CB211_HFRONT_PORCH      0
#define CB211_HBACK_PORCH       0
#define CB211_VFRONT_PORCH      0
#define CB211_VBACK_PORCH       0

#define CB245                   1
#define CB245_WIDTH             378
#define CB245_HEIGHT            242
#define CB245_PIX_WIDTH         (int)(17*256)
#define CB245_PIX_HEIGHT        (int)(20*256)
#define CB245_HFRONT_PORCH      0
#define CB245_HBACK_PORCH       0
#define CB245_VFRONT_PORCH      3
#define CB245_VBACK_PORCH       3

#define CB_WIPE(io)                                                         \
        do {                                                                \
        DAT_OUT(io, 0x38, 0);                                               \
        DAT_OUT(io, 0x78, 0);                                               \
        DAT_OUT(io, 0x77, 0);                                               \
        DAT_OUT(io, 0x37, 0);                                               \
        } while (0)

#define CB_LATCH_FRAME(io, flags)                                           \
        do {                                                                \
        DAT_OUT(io, 0x38, 0);                                               \
        DAT_OUT(io, 0x78, 0);                                               \
        DAT_OUT(io, 0xF7, 0);                                               \
        DAT_OUT(io, 0xB7, 0);                                               \
        } while (0)

#define CB_BEGIN_FRAME(io)                                                  \
        do {                                                                \
        } while (0)


#define CB_END_FRAME(io)                                                    \
        do {                                                                \
        } while (0)


#define CB_VCLK(io)                                                         \
        do {                                                                \
        DAT_OUT(io, 0x38, 0);                                               \
        DAT_OUT(io, 0x30, 0);                                               \
        DAT_OUT(io, 0x38, 0);                                               \
        DAT_OUT(io, 0x30, 0);                                               \
        DAT_OUT(io, 0x38, 0);                                               \
        DAT_OUT(io, 0x37, 0);                                               \
        DAT_OUT(io, 0x70, 0);                                               \
        DAT_OUT(io, 0x30, 0);                                               \
        } while (0)

#define CB_CLEAR_PIXEL(io)                                                  \
        do {                                                                \
        DAT_OUT(io, 0x31, 0);                                               \
        DAT_OUT(io, 0x32, 0);                                               \
        DAT_OUT(io, 0x34, 0);                                               \
        } while (0)

#define CB_HCLK(io)                                                         \
        do {                                                                \
        } while (0)

#define CB_RESET_OUTPUT(io)                                                 \
        do {                                                                \
        udelay(1);                                                          \
        CTL_OUT(io, 0x0C, 2);                                               \
        CTL_OUT(io, 0x0D, 0);                                               \
        } while (0)


#define CB_LATCH_PIXEL(io)                                                  \
        do {                                                                \
        } while (0)

#define CB_LOAD_PIXEL(io, bits, pixel)  /* Load pixel nibble at a time  */  \
        do {                                                                \
        DAT_OUT(io, 0x10, 0);                                               \
        DAT_OUT(io, 0x11, 0);                                               \
        pixel = ((STS_IN(io)&0xF0)^0x80)<<4;/* Read most sig nibble     */  \
        DAT_OUT(io, 0x20, 0);                                               \
        DAT_OUT(io, 0x22, 0);                                               \
        pixel |= ((STS_IN(io)&0xF0)^0x80);/* Read 2nd nibble            */  \
        DAT_OUT(io, 0x30, 0);                                               \
        DAT_OUT(io, 0x34, 0);                                               \
        pixel |= ((STS_IN(io)&0xF0)^0x80)>>4;/* Read least sig nibble   */  \
        } while (0)
#endif /* _CB_PARPORT_H_ */


