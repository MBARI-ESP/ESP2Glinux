/*
 * -*- linux-c -*-
 * Linux Kernel Module for the Audine Camera
 * Low level I/O-routines for the camera
 * Copyright (C) 2000 Peter Kirchgessner
 * http://www.kirchgessner.net, mailto:peter@kirchgessner.net
 *
 * Modified by F. Manenti <oss_astr_cav@arcanet.it> for the use in the
 * NOVA environment (nova.sourceforge.net)
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The sample interface routines for the module have been taken from the
 * Linux Kernel Module Programming Guide by Ori Pomerantz contained
 * in the Linux Documentation Project.
 *
 */

#include <asm/io.h>

/* Informations about the CCD-Chip */
typedef struct {
    char name[32];
    int lead_rows, trail_rows;	/* Leading
				   /trailing dummy rows */
    int lead_pixels, trail_pixels;	/* Leading/
					   trailing dummy pixels per row */
    int width, height;		/* Imageable area */
    int color;			/* Color flag */
} CCD_DEVICE_INFO;


/* Entry `1' of the array is used as default for the audine */
static CCD_DEVICE_INFO ccds[] = {
    {"KAF0400", 4, 4, 14, 14, 768, 512, 0},
    {"KAF0400C", 4, 4, 14, 14, 768, 512, 1},
    {"KAF0401", 4, 4, 14, 14, 768, 512, 0},
    {"KAF1600", 4, 4, 14, 14, 1536, 1024, 0},
    {"KAF1602", 4, 4, 14, 14, 1536, 1024, 0}
};
/* NOTE: The KAF chips have 4 colums at the beginning and 12 at the end
   of video line covered (lead_pixels and trail_pixels).
   But each line is transfered into the horizontal CCD shift register
   which has 796 elements. This has additionally 10 leading inactive pixels
   and two trailing inactice pixels. I just added these inactive pixels
   to the CCD_INFO-structure.
*/

enum ccd_type { KAF0400, KAF0400C, KAF0401, KAF1600, KAF1602 };
#define AUDINE_MAX_WIDTH  (1536)
#define AUDINE_MAX_HEIGHT (1024)

/* The functions have been taken from the samples given on the */
/* Audine page astroccd.com/terre/audine/English/program.htm  */

static void ccd_hreg_load(register unsigned short base);
static void ccd_pixels_read_fast(int n, register unsigned short base);
static void ccd_hreg_read_fast(const CCD_DEVICE_INFO * ccd,
                      register unsigned short base);
static void ccd_lines_read_fast(const CCD_DEVICE_INFO * ccd,
                      unsigned short base, int n);
static void ccd_clear(const CCD_DEVICE_INFO * ccd,
                      register unsigned short base);
#ifndef SIMUL
static void ccd_pixels_read(int npix, int binning, unsigned short *buf,
                      unsigned short port);
#endif
static void cntrl_register_setbit(int *CntrlReg, int value);
static void cntrl_register_clearbit(int *CntrlReg, int value);
static void cntrl_register_write(int CntrlReg, int Port);

/********************************************************************/
/* Transfer one line of the image area into the horizontal register */
/********************************************************************/
static void
ccd_hreg_load(register unsigned short base)
{
    outb(0xfb, base);		/* 11111011b */
    outb(0xfb, base);
    outb(0xfb, base);
    outb(0xfb, base);
    outb(0xfb, base);
    outb(0xfb, base);
    outb(0xfb, base);
    outb(0xfb, base);

    outb(0xfa, base);		/* 11111010b */
    outb(0xfa, base);
    outb(0xfa, base);
    outb(0xfa, base);
    outb(0xfa, base);
    outb(0xfa, base);
    outb(0xfa, base);
    outb(0xfa, base);

    outb(0xf9, base);		/* 11111001b */
    outb(0xf9, base);
    outb(0xf9, base);
    outb(0xf9, base);
    outb(0xf9, base);
    outb(0xf9, base);
    outb(0xf9, base);
    outb(0xf9, base);

    outb(0xfa, base);		/* 11111010b */
    outb(0xfa, base);
    outb(0xfa, base);
    outb(0xfa, base);
    outb(0xfa, base);
    outb(0xfa, base);
    outb(0xfa, base);
    outb(0xfa, base);

    outb(0xfb, base);		/* 11111011b */
    outb(0xfb, base);
    outb(0xfb, base);
    outb(0xfb, base);
    outb(0xfb, base);
    outb(0xfb, base);
    outb(0xfb, base);
    outb(0xfb, base);
}

/***********************************************************************/
/* Shift out n pixel from the horizontal register without digitization */
/***********************************************************************/
static void
ccd_pixels_read_fast(int n, register unsigned short base)
{
    while (n-- > 0) {
	outb(0xf7, base);	/* 11110111b */
	outb(0xff, base);	/* 11111111b */
	outb(0xfb, base);	/* 11111011b */
    }
}

/********************************************************************/
/* Shift out complete horizontal register without digitization.     */
/* Used also to clear horizontal register.                          */
/********************************************************************/
static void
ccd_hreg_read_fast(const CCD_DEVICE_INFO * ccd, register unsigned short base)
{
    int j;

    j = ccd->lead_pixels + ccd->width + ccd->trail_pixels;
    ccd_pixels_read_fast(j, base);
}

/********************************************************************/
/* Shift out n lines of image area and clear horizontal register    */
/********************************************************************/
static void
ccd_lines_read_fast(const CCD_DEVICE_INFO * ccd, unsigned short base, int n)
{
    int k;

    while (n > 0) {
	k = (n > 4) ? 4 : n;	/* To increase performance
				   load 4 lines into hreg */
	while (k--) {
	    ccd_hreg_load(base);
	    n--;
	}
	ccd_hreg_read_fast(ccd, base);
    }
}


/********************************************************************/
/* Clear image area                                                 */
/********************************************************************/

static void
ccd_clear(const CCD_DEVICE_INFO * ccd, register unsigned short base)
{
    ccd_lines_read_fast(ccd, base,
			ccd->trail_rows + ccd->height + ccd->lead_rows);
}



/********************************************************************/
/* Read pixels from horizontal register and digitize                */
/********************************************************************/
#ifndef SIMUL
static void
ccd_pixels_read(int npix, int binning, unsigned short *buf,
		unsigned short port)
{
    unsigned short P, P2;
    int a1, a2, a3, a4;
    int b;

    P = port;
    P2 = P + 1;

    while (npix-- > 0) {
	outb(247, P);		/* Reset */
	outb(255, P);		/* palier de reference  */
	outb(255, P);
	outb(255, P);
	outb(255, P);
	outb(255, P);
	outb(239, P);		/* clamp */

	for (b = 0; b < binning; b++) {
	    outb(255, P);
	    outb(251, P);	/* palier video */
	}
	outb(251, P);
	outb(251, P);
	outb(251, P);
	outb(251, P);
	outb(219, P);		/* start convert */
	outb(219, P);
	outb(219, P);
	outb(219, P);
	outb(219, P);
	outb(219, P);

	/* Read digitized values */
	a1 = (inb(P2)) >> 4;
	outb(91, P);
	a2 = (inb(P2)) >> 4;
	outb(155, P);
	a3 = (inb(P2)) >> 4;
	outb(27, P);
	a4 = (inb(P2)) >> 4;
	b = (a1 + (a2 << 4) + (a3 << 8) + (a4 << 12)) ^ 0x8888;
	if (b > 32767)
	    b = 32767;
	*(buf++) = (unsigned short) b;
    }
}
#endif

static void
cntrl_register_setbit(int *CntrlReg, int value)
{
    *CntrlReg |= value;
}

static void
cntrl_register_clearbit(int *CntrlReg, int value)
{
    *CntrlReg &= ((~value) & 0xff);
}

static void
cntrl_register_write(int CntrlReg, int Port)
{
    /* The samples from the Audine site say to switch on amplifier */
    /* one must write the value 1 to the control register. */
    /* But it seems that we have to write 0. So we invert bits 1 and 3. */
    CntrlReg ^= 5;

    if (Port)
	outb(CntrlReg, Port + 2);
}
