/***************************************************************************\
    
    TDI - Time Delay Integration Calibration Program
    
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

#include "tdi_util.h"
#define	DEFAULT_CCD_DEVICE	"/dev/ccda"
#define BIAS_ROW_COUNT  10
#define DARK_ROW_COUNT  10
#define FLAT_ROW_COUNT  10

void row_ave(struct ccd_exp *exposure, struct ccd_image *ave, int row_count)
{
    int            i, j;
    unsigned int   loadtime, row_exp;
    float         *row_sum, inv_row_count;
    struct timeval now;

    /*
     * Set average image vector fields.
     */
    ave->width        = exposure->image->width;
    ave->height       = exposure->image->height;
    ave->depth        = exposure->image->depth;
    ave->datamin      = exposure->image->datamin;
    ave->datamax      = exposure->image->datamax;
    ave->pixel_width  = exposure->image->pixel_width;
    ave->pixel_height = exposure->image->pixel_height;
    ave->pixels       = malloc(ave->width * ((ave->depth + 7) / 8));
    /*
     * Clear row sum.
     */
    row_sum = malloc(exposure->width * sizeof(float));
    for (i = 0; i < exposure->width; i++)
        row_sum[i] = 0.0;
    /*
     * Clear ccd before starting TDI mode.
     */
    row_exp         = exposure->msec;
    exposure->flags = 0;
    ccd_expose(exposure);
    exposure->flags = CCD_EXP_FLAGS_TDI;
    /*
     * Read out full frame before calibration rows.
     */
    for (j = 0; j < exposure->ccd->height; j++)
    {
        ccd_read_row(exposure);
        if (!(j & 0x1F))
        {
            printf(".");
            fflush(stdout);
        }
        gettimeofday(&now, NULL);
        loadtime = now.tv_sec  * 1000 + now.tv_usec  / 1000 - (exposure->start + exposure->msec);
        exposure->msec = loadtime > row_exp ? 0 : row_exp - loadtime;
        ccd_expose(exposure);
    }
    for (j = 0; j < row_count; j++)
    {
        ccd_read_row(exposure);
        printf("*");
        fflush(stdout);

#define PIXEL_LOOP(pixel_type)                                              \
        for (i = 0; i < exposure->image->width; i++)                        \
            row_sum[i] += ((pixel_type *)exposure->image->pixels)[i];

        PIXEL_SIZE_CASE((exposure->image->depth + 7) / 8);
#undef PIXEL_LOOP

        gettimeofday(&now, NULL);
        loadtime = now.tv_sec  * 1000 + now.tv_usec  / 1000 - (exposure->start + exposure->msec);
        exposure->msec = loadtime > row_exp ? 0 : row_exp - loadtime;
        ccd_expose(exposure);
    }
    ccd_read_row(exposure);
    inv_row_count = 1.0 / row_count;

#define PIXEL_LOOP(pixel_type)                                              \
    for (i = 0; i < ave->width; i++)                                        \
        ((pixel_type *)ave->pixels)[i] = row_sum[i] * inv_row_count;

    PIXEL_SIZE_CASE((ave->depth + 7) / 8);
#undef PIXEL_LOOP

    free(row_sum);
    /*
     * Save effective exposure time for calibrated vector.
     */
    ave->exposure = row_exp * (exposure->ccd->height / exposure->ybin);
}
/***************************************************************************\
*                                                                           *
*                               Main Program                                *
*                                                                           *
\***************************************************************************/

int main(int argc, char **argv)
{
    int               xbin, ybin, i;
    char             *dev, *bias_file, *dark_file, *flat_file;
    struct ccd_dev    ccd;
    struct ccd_exp    exposure;
    struct ccd_image *bias, *dark, *flat;

    /*
     * Init structures.
     */
    memset(&ccd,      0, sizeof(struct ccd_dev));
    memset(&exposure, 0, sizeof(struct ccd_exp));
    /*
     * Set defaults.
     */
    dev       = DEFAULT_CCD_DEVICE;
    xbin      = 1;
    ybin      = 1;
    bias_file = NULL;
    dark_file = NULL;
    flat_file = NULL;
    bias      = NULL;
    dark      = NULL;
    flat      = NULL;
    /*
     * Read options.
     */
    opterr = 0;
    while ((i = getopt(argc, argv, "c:x:y:b:s:f:")) != EOF)
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
            case 'b':
                bias_file = optarg;
                break;
            case 's':
                dark_file = optarg;
                break;
            case 'f':
                flat_file = optarg;
                break;
            case '?':
                fprintf(stderr, "Usage: %s\nOptions:\n\t[-c camera_device_name]\n\t[-x x_bin]\n\t[-y y_bin]\n\t[-b bias_frame_file]\n\t[-s scaled_dark_frame_file]\n\t[-f flat_frame_file]\n", argv[0]);
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
    exposure.msec     = 0;
    /*
     * Init image.
     */
    exposure.image               = ccd_image_new(NULL);
    exposure.image->width        = exposure.width  / exposure.xbin;
    exposure.image->height       = exposure.height / exposure.ybin;
    exposure.image->depth        = ccd.depth;
    exposure.image->pixmin       =
    exposure.image->pixmax       =
    exposure.image->datamin      = 0;
    exposure.image->datamax      = ~0UL >> (32 - exposure.dac_bits);
    exposure.image->pixel_width  = ccd.pixel_width  * exposure.xbin;
    exposure.image->pixel_height = ccd.pixel_height * exposure.ybin;
    strcpy(exposure.image->camera, exposure.ccd->camera);
    /*
     * Bias frame.
     */
    if (bias_file && (bias = ccd_image_new(bias_file)))
    {
        /*
         * Prompt before bias frame calibration.
         */
        printf("\nCalibrating bias frame.  Cover camera and press <RETURN> when ready.\n");
        exposure.msec = 0;
        getchar();
        row_ave(&exposure, bias, BIAS_ROW_COUNT);
        ccd_image_write_fits_data_row(bias);
    }
    /*
     * Scaled dark frame.
     */
    if (dark_file && (dark = ccd_image_new(dark_file)))
    {
        /*
         * Prompt before dark frame calibration.
         */
        printf("\nCalibrating dark frame.  Cover camera and press <RETURN> when ready.\n");
        getchar();
        exposure.msec = 2000;
        row_ave(&exposure, dark, DARK_ROW_COUNT);
        ccd_image_calibrate_row(dark, bias, NULL, NULL);
        ccd_image_write_fits_data_row(dark);
    }
    /*
     * Flat frame.
     */
    if (flat_file && (flat = ccd_image_new(flat_file)))
    {
        /*
         * Prompt before flat frame calibration.
         */
        printf("\nCalibrating flat frame.  Dimly illuminate camera and press <RETURN> when ready.\n");
        getchar();
        exposure.msec = 0;
        row_ave(&exposure, flat, FLAT_ROW_COUNT);
        ccd_image_calibrate_row(flat, bias, dark, NULL);
        ccd_image_write_fits_data_row(flat);
    }
    /*
     * All done.
     */
    ccd_image_delete(exposure.image);
    if (bias) ccd_image_delete(bias);
    if (dark) ccd_image_delete(dark);
    if (flat) ccd_image_delete(flat);
    ccd_release(&ccd);
}
