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

/***************************************************************************\
*                                                                           *
*                       USB basic control macros                            *
*                                                                           *
\***************************************************************************/
#ifndef _SX_USB_H_
#define _SX_USB_H_
/*
 * Vendor and product IDs.
 */
#define EZUSB_VENDOR_ID     0x0547
#define EZUSB_PRODUCT_ID    0x2131
#define EZUSB2_VENDOR_ID    0x04B4
#define EZUSB2_PRODUCT_ID   0x8613
#define ECHO2_VENDOR_ID     0x1278
#define ECHO2_PRODUCT_ID    0x0100
#define ECHO3_VENDOR_ID     0x1278
#define ECHO3_PRODUCT_ID    0x0200
/*
 * Set and reset 8051 requests.
 */
#define EZUSB_CPUCS_REG     0x7F92
#define EZUSB2_CPUCS_REG    0xE600
#define CPUCS_RESET         0x01
#define CPUCS_RUN           0x00
/*
 * Address in 8051 external memory for debug info and CCD parameters.
 * Must use low address alias for EZ-USB
 */
#define SX_EZUSB_DEBUG_BUF      0x1C80
#define SX_EZUSB2_DEBUG_BUF     0xE1F0
/*
 * EZ-USB code download requests.
 */
#define EZUSB_FIRMWARE_LOAD 0xA0
/*
 * EZ-USB download code for each camera. Converted from .HEX file.
 */
struct sx_ezusb_download_record
{
    int           addr;
    int           len;
    unsigned char data[16];
};
struct sx_ezusb_download_record sx_ezusb_code[] = 
{
    #include "sx_ezusb_code.h"
};
struct sx_ezusb_download_record sx_ezusb2_code[] = 
{
    #include "sx_ezusb2_code.h"
};
#endif /* _SX_USB_H_ */


