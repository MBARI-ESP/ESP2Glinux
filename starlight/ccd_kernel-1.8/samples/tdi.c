/***************************************************************************\

    TDI - Time Delay Integration

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

#define USE_VGA 1

#include "tdi_util.h"

#define SIDEREAL_RATE           0.00416667  // Degrees per second
#define RAD_TO_DEG              57.29577951
#define DEG_TO_RAD              0.01745329
#define DEFAULT_CCD_DEV         "/dev/ccda"
#define DEFAULT_FOCAL_LENGTH    100
#define DEFAULT_DECLINATION     0
#define DEFAULT_RA              0
#define DEFAULT_FILE_NAME       "tdi_out.fits"
#define DEFAULT_TIME_EXPOSURE   5

/***************************************************************************\
*                                                                           *
*                               Main Program                                *
*                                                                           *
\***************************************************************************/

int main(int argc, char **argv)
{
    int               xbin, ybin, i, time_exp, done, interactive;
    float             focal_length, dec, ra, deg_sec;
    unsigned long     row_exp, loadtime;
    char             *dev, *bias_file, *dark_file, *flat_file, *filename;
    struct timeval    now;
    struct ccd_dev    ccd;
    struct ccd_exp    exposure;
    struct ccd_image *bias, *dark, *flat;
#if USE_VGA
    int               y, old_vgamode;
    char             *scanline;
    vga_modeinfo     *vga;
#endif

    /*
     * Init structures.
     */
    memset(&ccd,      0, sizeof(struct ccd_dev));
    memset(&exposure, 0, sizeof(struct ccd_exp));
    /*
     * Set defaults.
     */
    interactive  = 0;
    dev          = DEFAULT_CCD_DEV;
    xbin         = 1;
    ybin         = 1;
    focal_length = DEFAULT_FOCAL_LENGTH;
    dec          = DEFAULT_DECLINATION;
    ra           = DEFAULT_RA;
    time_exp     = DEFAULT_TIME_EXPOSURE;
    filename     = DEFAULT_FILE_NAME;
    bias_file    = NULL;
    dark_file    = NULL;
    flat_file    = NULL;
    bias         = NULL;
    dark         = NULL;
    flat         = NULL;
    /*
     * Read options.
     */
    opterr = 0;
    while ((i = getopt(argc, argv, "c:x:y:l:d:r:t:b:s:f:o:i")) != EOF)
    {
        switch (i)
        {
            case 'c':
                dev = optarg;
                break;
            case 'x':
                xbin = atoi(optarg);
                break;
            case 'y':
                ybin = atoi(optarg);
                break;
            case 'l':
                focal_length = atof(optarg);
                break;
            case 'd':
                dec = atof(optarg);
                break;
            case 'r':
                ra = atof(optarg);
                break;
            case 't':
                time_exp = atoi(optarg);
                break;
            case 'b':
                bias_file = optarg;
                break;
            case 's':
                dark_file = optarg;
                break;
            case 'f':
                flat_file = optarg;
                break;
            case 'o':
                filename = optarg;
                break;
            case 'i':
                interactive = 1;
                break;
            case '?':
                fprintf(stderr, "Usage: %s\nOptions:\n\t[-c camera_device_name]\n\t[-x x_bin]\n\t[-y y_bin]\n\t[-l focal_length]\n\t[-d declination]\n\t[-r right_ascension]\n\t[-t exposure_time_minutes]\n\t[-b bias_frame_file]\n\t[-s scaled_dark_frame_file]\n\t[-f flat_frame_file]\n\t[-o output_filename]\n\t[-i (interactive)]\n", argv[0]);
                exit (1);
        }
    }
    /*
     * Init camera.
     */
    strcpy(ccd.filename, dev);
    ccd.fd = 0;
    if (!ccd_connect(&ccd))
    {
        fprintf(stderr, "Unable to open CCD device file: %s\n", dev);
        exit(1);
    }
    if (!(ccd.caps & CCD_EXP_FLAGS_TDI))
    {
        fprintf(stderr, "%s not capable of TDI mode.\n", ccd.camera);
        exit(1);
    }
    /*
     * Init exposure.
     */
    exposure.ccd      = &ccd;
    exposure.xoffset  =
    exposure.yoffset  = 0;
    exposure.width    = ccd.width;
    exposure.height   = ybin;
    exposure.xbin     = xbin;
    exposure.ybin     = ybin;
    exposure.dac_bits = ccd.dac_bits;
    exposure.flags    = 0;
    exposure.msec     = 1;
    /*
     * Init image.
     */
    exposure.image                   = ccd_image_new(filename);
    exposure.image->width            = exposure.width / xbin;
    exposure.image->height           = 1;
    exposure.image->depth            = ccd.depth;
    exposure.image->declination      = dec;
    exposure.image->right_ascension  = ra;
    exposure.image->pixmin           =
    exposure.image->pixmax           =
    exposure.image->datamin          = 0;
    exposure.image->datamax          = ~0UL >> (32 - exposure.dac_bits);
    exposure.image->pixel_width      = ccd.pixel_width  * exposure.xbin;
    exposure.image->pixel_height     = ccd.pixel_height * exposure.ybin;
    exposure.image->pixel_fov_width  = 2 * atan(exposure.image->pixel_width  / focal_length / 2000.0) * RAD_TO_DEG;
    exposure.image->pixel_fov_height = 2 * atan(exposure.image->pixel_height / focal_length / 2000.0) * RAD_TO_DEG;
    strcpy(exposure.image->camera, exposure.ccd->camera);
    /*
     * Load and validate calibration frames.
     */
    if (bias_file && !(bias = ccd_image_load(bias_file)))
    {
        fprintf(stderr, "Unable to open bias frame file: %s\n", bias_file);
        exit(1);
    }
    if (dark_file && !(dark = ccd_image_load(dark_file)))
    {
        fprintf(stderr, "Unable to open scaled dark frame file: %s\n", dark_file);
        exit(1);
    }
    if (flat_file && !(flat = ccd_image_load(flat_file)))
    {
        fprintf(stderr, "Unable to open flat frame file: %s\n", flat_file);
        exit(1);
    }
    if (bias && (bias->width != exposure.image->width || bias->depth != exposure.image->depth))
    {
        fprintf(stderr, "Bias frame size mismatch: %s\n", bias_file);
        exit(1);
    }
    if (dark && (dark->width != exposure.image->width || dark->depth != exposure.image->depth))
    {
        fprintf(stderr, "Scaled dark frame size mismatch: %s\n", dark_file);
        exit(1);
    }
    if (flat && (flat->width != exposure.image->width || flat->depth != exposure.image->depth))
    {
        fprintf(stderr, "Flat frame size mismatch: %s\n", flat_file);
        exit(1);
    }
    /*
     * Print out the internal values.
     */
    printf("Focal length (mm): %f\n", focal_length);
    printf("Declination: %f\n", dec);
    printf("Right Ascension: %f\n", ra);
    printf("Total exposure time (min) : %d\n", time_exp);
    printf("Output filename: %s\n", filename);
    printf("CCD camera: %s\n", ccd.camera);
    printf("CCD pixel height (um): %f\n", exposure.image->pixel_height);
    printf("CCD pixel width  (um): %f\n", exposure.image->pixel_width);
    printf("CCD pixel height (degree): %f\n", exposure.image->pixel_fov_height);
    printf("CCD pixel width  (degree): %f\n", exposure.image->pixel_fov_width);
    /*
     * Linear rate at given declination.  Keep it sane by limiting declination to < 89.5.
     */
    if (fabs(dec) > 89.5)
        dec = 89.5;
    deg_sec = SIDEREAL_RATE * cos(dec * DEG_TO_RAD);
    printf("Linear rate (degrees per second): %f\n", deg_sec);
    /*
     * Integration time per pixel row and entire frame.
     */
    row_exp                  = exposure.image->pixel_fov_height / deg_sec * 1000;
    exposure.image->exposure = row_exp * ccd.height;
    printf("Pixel(row) transit (msecs): %u\n", row_exp);
    printf("Effective exposure (msecs): %u\n", exposure.image->exposure);
#if USE_VGA
    if (interactive)
    {
        /*
         * Wait for user input before entering main loop.
         */
        printf("Press <ENTER> to continue...\n");
        getchar();
        /*
         * Init VGA mode.
         */
        if (vga_init() == 0)
        {
            int dac_shift = 0;
            old_vgamode = vga_getcurrentmode();
       //     if (vga_setmode(G1024x768x256))
                if (vga_setmode(G800x600x256))
                    vga_setmode(G640x480x256);
            if (vga_ext_set(VGA_EXT_AVAILABLE, VGA_AVAIL_FLAGS) & VGA_CLUT8)
                vga_ext_set(VGA_EXT_SET, VGA_CLUT8);
            else
                dac_shift = 2;
            for (i = 0; i < 256; i++)
                vga_setpalette(i, i >> dac_shift, i >> dac_shift, i >> dac_shift);
            vga = vga_getmodeinfo(vga_getcurrentmode());
        }
        else
        {
            fprintf(stderr, "Unable to initialize VGA.\n");
            exit(1);
        }
        scanline = malloc(exposure.image->width);
        y        = 0;
    }
#endif
    /*
     * Calc ending exposure time.
     */
    gettimeofday(&now, NULL);
    time_exp = now.tv_sec + time_exp * 60;
    /*
     * Clear ccd before starting TDI mode.
     */
    ccd_expose(&exposure);
    exposure.image->height = 0;
    /*
     * Use TDI mode for remaining exposures.
     */
    exposure.flags  = CCD_EXP_FLAGS_TDI;
    /*
     * Continue exposing and reading pixel rows to match sidereal rate.
     */
    done = 0;
    while (!done)
    {
        ccd_read_row(&exposure);
        ccd_image_calibrate_row(exposure.image, bias, dark, flat);
        ccd_image_write_fits_data_row(exposure.image);
        exposure.image->height++;
        gettimeofday(&now, NULL);
        loadtime = now.tv_sec  * 1000 + now.tv_usec  / 1000 - (exposure.start + exposure.msec);
        exposure.msec = loadtime > row_exp ? 0 : row_exp - loadtime;
        ccd_expose(&exposure);
        done = (now.tv_sec >= time_exp);
#if USE_VGA
        if (interactive)
        {
            /*
             * Display pixels on VGA in looping top to bottom fashion.
             */
            switch ((ccd.depth + 7) / 8)
            {
                case 1:
                    for (i = 0; i < exposure.image->width; i++)
                        scanline[i] = exposure.image->pixels[i];
                    break;
                case 4:
                    for (i = 0; i < exposure.image->width; i++)
                        scanline[i] = exposure.image->pixels[i*4 + 3];
                    break;
                case 2:
                default:
                    for (i = 0; i < exposure.image->width; i++)
                        scanline[i] = exposure.image->pixels[i*2 + 1];
                    break;
            }
            vga_drawscansegment(scanline, 0, y, min(exposure.image->width, vga->width));
            if (++y >= vga->height)
                y = 0;
            switch (vga_getkey())
            {
                case 'q':
                case 'Q':
                    done = 1;
                    break;
                case '+': // Increase row exposure by 5%
                case '=':
                    row_exp = row_exp * 1.05;
                    break;
                case '-': // Decrease row exposure by 5%
                case '_':
                    row_exp = row_exp * 0.95;
                    break;
            }
        }
#endif
    }
    /*
     * All done.
     */
    exposure.image->exposure = row_exp * (ccd.height / exposure.ybin);
    ccd_image_delete(exposure.image);
    ccd_image_delete(bias);
    ccd_image_delete(dark);
    ccd_image_delete(flat);
    ccd_release(&ccd);
#if USE_VGA
    if (interactive)
        vga_setmode(old_vgamode);
#endif
}
