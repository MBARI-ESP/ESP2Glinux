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

/***************************************************************************\
*                                                                           *
*                               CCD driver defines                          *
*                                                                           *
\***************************************************************************/

#ifndef _CCD_H_
#define _CCD_H_

#include "ccd_msg.h"

#define CCD_VERSION                 ((VERSION_MAJOR << 16) | VERSION_MINOR)
#define CCD_FIELD_ODD               1
#define CCD_FIELD_EVEN              2
#define CCD_FIELD_BOTH              (CCD_FIELD_EVEN|CCD_FIELD_ODD)
#define CCD_FIELD_MASK              CCD_FIELD_BOTH
#define CCD_NOBIN_ACCUM             CCD_EXP_FLAGS_NOBIN_ACCUM
#define CCD_NOWIPE_FRAME            CCD_EXP_FLAGS_NOWIPE_FRAME
#define CCD_NOOPEN_SHUTTER          CCD_EXP_FLAGS_NOOPEN_SHUTTER
#define CCD_TDI                     CCD_EXP_FLAGS_TDI
#define CCD_NOCLEAR_FRAME           CCD_EXP_FLAGS_NOCLEAR_FRAME
/*
 * CCD mini-driver object.
 */
struct ccd_mini
{
    unsigned int version;
    char         id_string[CCD_CCD_NAME_LEN + 1];
    unsigned int width;
    unsigned int height;
    unsigned int pixel_width;
    unsigned int pixel_height;
    unsigned int image_fields;
    unsigned int image_depth;
    unsigned int dac_bits;
    unsigned int color_format;
    unsigned int flag_caps;
    /*
     * Function pointers for basic camera ops.
     */
    int          (*open)(void *);   
    int          (*control)(void *, unsigned short, unsigned long);   
    int          (*close)(void *);   
    int          (*read_row)(void *, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned char *);
    void         (*begin_read)(void *, unsigned int, unsigned int);
    void         (*end_read)(void *, unsigned int);
    void         (*latch_frame)(void *, unsigned int);
    void         (*new_frame)(void *, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);
    void         (*temp_control)(void *, unsigned short *, int *);
};
int ccd_register_device(struct ccd_mini *, void *);
int ccd_unregister_device(void *);

#endif /* _CCD_H_ */
