/***************************************************************************\
    
    TDI - Time Delay Integration Utilities
    
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

/***************************************************************************\
*                                                                           *
*                       Image and FITS File Routines                        *
*                                                                           *
\***************************************************************************/

#define FITS_CARD_COUNT     36
#define FITS_CARD_SIZE      80
#define FITS_RECORD_SIZE    (FITS_CARD_COUNT*FITS_CARD_SIZE)
/*
 * Convert unsigned LE pixels to signed BE pixels.
 */
static void convert_pixels(unsigned char *src, unsigned char *dst, unsigned int sign_bit, int pixel_size, int count)
{
    unsigned int pixel;

    switch (pixel_size)
    {
        case 1:
            memcpy(dst, src, count);
            break;
        case 2:
            while (count--)
            {
                pixel = *(unsigned short *)src ^ sign_bit;
                *(unsigned short *)dst = ((pixel & 0xFF00) >> 8) | ((pixel & 0x00FF) << 8);
                src += 2;
                dst += 2;
            }
            break;
        case 4:
            while (count--)
            {
                pixel = *(unsigned long *)src ^ sign_bit;
                *(unsigned long *)dst = ((pixel & 0xFF000000) >> 24) 
                                    | ((pixel & 0x00FF0000) >> 8)
                                    | ((pixel & 0x0000FF00) << 8)
                                    | ((pixel & 0x000000FF) << 24);
                src += 4;
                dst += 4;
            }
            break;
    }
}
/*
 * Write the FITS header.
 */
int ccd_image_write_fits_header(struct ccd_image *image)
{
    char           record[FITS_CARD_COUNT][FITS_CARD_SIZE];
    unsigned char *fits_pixels;
    int            i, j, k;

    /*
     * Seek to the beginning of the file.
     */
    lseek(image->fd, 0, SEEK_SET);
    /*
     * Fill header records.
     */
    memset(record, ' ', FITS_RECORD_SIZE);
    i = 0;
    sprintf(record[i++], "SIMPLE  = %19c", 'T');
    sprintf(record[i++], "BITPIX  = %19d", image->depth);
    sprintf(record[i++], "NAXIS   = %19d", 2);
    sprintf(record[i++], "NAXIS1  = %19d", image->width);
    sprintf(record[i++], "NAXIS2  = %19d", image->height);
    if (image->depth == 16)
    {
        sprintf(record[i++], "BZERO   = %19f", 32768.0);
        sprintf(record[i++], "BSCALE  = %19f", 1.0);
    }
    if (image->depth == 32)
    {
        sprintf(record[i++], "BZERO   = %19f", 2147483647.0);
        sprintf(record[i++], "BSCALE  = %19f", 1.0);
    }
    sprintf(record[i++], "DATAMIN = %19d", image->datamin);
    sprintf(record[i++], "DATAMAX = %19d", image->datamax);
    sprintf(record[i++], "CTYPE1  = 'DEC'");
    sprintf(record[i++], "CPIX1   = %19f", 1.0);
    sprintf(record[i++], "CRVAL1  = %19f", image->declination);
    sprintf(record[i++], "CRDELT1 = %19f", image->pixel_fov_width);
    sprintf(record[i++], "CROTA1  = %19f", 1.0);
    sprintf(record[i++], "CTYPE2  = 'RA'");
    sprintf(record[i++], "CPIX2   = %19f", 1.0);
    sprintf(record[i++], "CRVAL2  = %19f", image->right_ascension);
    sprintf(record[i++], "CRDELT2 = %19f", image->pixel_fov_height);
    sprintf(record[i++], "CROTA2  = %19f", 1.0);
    sprintf(record[i++], "CREATOR = 'Time Delay Integration App 0.1'");
    sprintf(record[i++], "DATE-OBS= '%sT%s'", image->date, image->time);
    sprintf(record[i++], "EXPOSURE= %19f", (float)image->exposure / 1000.0);
    sprintf(record[i++], "OBJECT  = '%s'", image->object);
    sprintf(record[i++], "INSTRUME= '%s'", image->camera);
    sprintf(record[i++], "OBSERVER= '%s'", image->observer);
    sprintf(record[i++], "TELESCOP= '%s'", image->telescope);
    sprintf(record[i++], "LOCATION= '%s'", image->location);
    for (j = 0; j < MAX_PROCESS_HISTORY; j++)
    {
        if (image->history[j][0])
        {
            sprintf(record[i++], "HISTORY  %s", image->history[j]);
            if (i >= FITS_CARD_COUNT)
            {
                for (k = 0; k < FITS_RECORD_SIZE; k++)
                    if (((char *)record)[k] == '\0')
                        ((char *)record)[k] = ' ';
                write(image->fd, record, FITS_RECORD_SIZE);
                memset(record, ' ', FITS_RECORD_SIZE);
                i = 0;
            }
        }
    }
    for (j = 0; j < MAX_COMMENTS; j++)
    {
        if (image->comments[j][0])
        {
            sprintf(record[i++], "COMMENT  %s", image->comments[j]);
            if (i >= FITS_CARD_COUNT)
            {
                for (k = 0; k < FITS_RECORD_SIZE; k++)
                    if (((char *)record)[k] == '\0')
                        ((char *)record)[k] = ' ';
                write(image->fd, record, FITS_RECORD_SIZE);
                memset(record, ' ', FITS_RECORD_SIZE);
                i = 0;
            }
        }
    }
    sprintf(record[i++], "END");
    for (k = 0; k < FITS_RECORD_SIZE; k++)
        if (((char *)record)[k] == '\0')
            ((char *)record)[k] = ' ';
    write(image->fd, record, FITS_RECORD_SIZE);
}
/*
 * Write one row of image data to FITS file.
 */
int ccd_image_write_fits_data_row(struct ccd_image *image)
{
    unsigned int   pixel, image_pitch, pixel_size, count, sign_bit;
    unsigned char *fits_pixels, *src, *dst;

    /*
     * Convert and write image data.
     */
    pixel_size  = ((image->depth + 7) / 8);
    image_pitch = image->width  * pixel_size;
    fits_pixels = malloc(image_pitch);
    convert_pixels(image->pixels, fits_pixels, 1 << (pixel_size*8-1), pixel_size, image->width);
    write(image->fd, fits_pixels, image_pitch);
    free(fits_pixels);
    return (0);
}
/*
 * Load image from FITS file.
 */
int ccd_image_load_fits(struct ccd_image *image, char *filename)
{
    char           record[FITS_CARD_COUNT+1][FITS_CARD_SIZE];
    char           key[10];
    unsigned char *fits_pixels;
    int            i, j, k, l, image_size, image_pitch, pixel_size, fd, done;
    float          zero, scale;
    
    /*
     * Clear out all image fields.
     */
    memset(image, 0, (unsigned int)&image->pixel_width - (unsigned int)image);
    zero         = 0.0;
    scale        = 1.0;
    /*
     * Read file.
     */
    if ((fd = open(filename, O_RDONLY, 0)) > 0)
    {
        /*
         * Read header records.
         */
        if (read(fd, record, FITS_RECORD_SIZE) < 0)
        {
            close(fd);
            return (-1);
        }
        for (i = 0; i < FITS_CARD_COUNT; i++)
            record[i][FITS_CARD_SIZE-1] = '\0';
        i = j = k = 0;
        done = 0;
        sscanf(record[i], "%s", key);
        key[8] = '\0';
        if (strcmp(key, "SIMPLE"))
        {
            close(fd);
            return (-1);
        }
        do
        {
            sscanf(record[i], "%s", key);
            key[8] = '\0';
            if (!strcmp(key, "BITPIX"))
                sscanf(&record[i++][10], "%d", &image->depth);
            else if (!strcmp(key, "NAXIS"))
            {
                sscanf(&record[i++][10], "%d", &l);
                if (l != 2)
                {
                    close(fd);
                    return (-1);
                }
            }
            else if (!strcmp(key, "BZERO"))
                sscanf(&record[i++][10], "%f", &zero);
            else if (!strcmp(key, "BSCALE"))
            {
                sscanf(&record[i++][10], "%f", &scale);
                if (scale != 1.0)
                {
                    close(fd);
                    return (-1);
                }
            }
            else if (!strcmp(key, "NAXIS1"))
                sscanf(&record[i++][10], "%d", &image->width);
            else if (!strcmp(key, "NAXIS2"))
                sscanf(&record[i++][10], "%d", &image->height);
            else if (!strcmp(key, "EXPOSURE"))
            {
                float f;
                sscanf(&record[i++][10], "%f", &f);
                image->exposure = f * 1000.0;
            }
            else if (!strcmp(key, "END"))
                done = 1;
            else
                i++;
            if (i >= FITS_CARD_COUNT)
            {
                read(fd, record, FITS_RECORD_SIZE);
                for (l = 0; l < FITS_CARD_COUNT; l++)
                    record[l][FITS_CARD_SIZE-1] = '\0';
                i = 0;
            }

        } while (!done);
        /*
         * Read and convert image data.
         */
        pixel_size    = ((image->depth + 7) / 8);
        image_pitch   = image->width  * pixel_size;
        image_size    = image->height * image_pitch;
        image->pixels = malloc(image_size);
        fits_pixels   = malloc(image_pitch);
        for (i = 0; i < image->height; i++)
        {
            read(fd, fits_pixels, image_pitch);
            convert_pixels(fits_pixels, image->pixels + image_size - (i+1) * image_pitch, zero == 0.0 ? 0 : 0x80, pixel_size, image->width);
        }
        free(fits_pixels);
        close(fd);
    }
    else
        return (-1);
    return (0);
}
/*
 * Create and fill image structure and optional FITS file.
 */
struct ccd_image *ccd_image_new(char *filename)
{
    struct ccd_image *image = (struct ccd_image *)malloc(sizeof(struct ccd_image));
    memset(image, 0, sizeof(struct ccd_image));
    if (filename)
    {
        /*
         * Create a FITS file for the image.
         */
        image->fd = creat(filename, 0666);
        /*
         * Write dummy header so data follows.
         */
        ccd_image_write_fits_header(image);
    }
    return (image);
}
/*
 * Create and fill image structure from FITS file.
 */
struct ccd_image *ccd_image_load(char *filename)
{
    struct ccd_image *image = (struct ccd_image *)malloc(sizeof(struct ccd_image));
    memset(image, 0, sizeof(struct ccd_image));
    if (!filename || ccd_image_load_fits(image, filename))
    {
        free(image);
        image = NULL;
    }
    return (image);
}
/*
 * Pad FITS image with zeros and close, free up memory.
 */
void ccd_image_delete(struct ccd_image *image)
{
    char record[FITS_CARD_COUNT][FITS_CARD_SIZE];
    int  image_size, image_pitch, pixel_size;

    if (image)
    {
        if (image->fd)
        {
            pixel_size  = ((image->depth + 7) / 8);
            image_pitch = image->width  * pixel_size;
            image_size  = image->height * image_pitch;
            /*
             * Pad remaining record size with zeros.
             */
            if (image_size % FITS_RECORD_SIZE)
            {
                lseek(image->fd, 0, SEEK_END);
                memset(record, 0, FITS_RECORD_SIZE);
                write(image->fd, record, FITS_RECORD_SIZE - (image_size % FITS_RECORD_SIZE));
            }
            /*
             * Update the header and close.
             */
            ccd_image_write_fits_header(image);
            close(image->fd);
        }
        if (image->pixels)
            free(image->pixels);
        free(image);
    }
}

/***************************************************************************\
*                                                                           *
*                     Low level CCD camera control                          *
*                                                                           *
\***************************************************************************/

/*
 * Connect to CCD camera.
 */
int ccd_connect(struct ccd_dev *ccd)
{
    int           fd, msg_len, i;
    CCD_ELEM_TYPE msg[CCD_MSG_CCD_LEN/CCD_ELEM_SIZE];

    ccd->fd = 0;
    if ((fd = open(ccd->filename, O_RDWR, 0)) < 0)
        return (FALSE);
    /*
     * Request CCD parameters.
     */
    msg[CCD_MSG_HEADER_INDEX]    = CCD_MSG_HEADER;
    msg[CCD_MSG_LENGTH_LO_INDEX] = CCD_MSG_QUERY_LEN;
    msg[CCD_MSG_LENGTH_HI_INDEX] = 0;
    msg[CCD_MSG_INDEX]           = CCD_MSG_QUERY;
    write(fd, (char *)msg, CCD_MSG_QUERY_LEN);
    if ((msg_len = read(fd, (char *)msg, CCD_MSG_CCD_LEN)) != CCD_MSG_CCD_LEN)
    {
        fprintf(stderr, "CCD message length wrong: %d\n", msg_len);
        return (FALSE);
    }
    /*
     * Response from CCD query.
     */
    if (msg[CCD_MSG_INDEX] != CCD_MSG_CCD)
    {
        fprintf(stderr, "Wrong message returned from query: 0x%04X", msg[CCD_MSG_INDEX]);
        return (FALSE);
    }
    ccd->fd           = fd;
    ccd->width        = msg[CCD_CCD_WIDTH_INDEX];
    ccd->height       = msg[CCD_CCD_HEIGHT_INDEX];
    ccd->pixel_width  = (int)((msg[CCD_CCD_PIX_WIDTH_INDEX]  / 25.6) + 0.5) / 10.0;
    ccd->pixel_height = (int)((msg[CCD_CCD_PIX_HEIGHT_INDEX] / 25.6) + 0.5) / 10.0;
    ccd->fields       = msg[CCD_CCD_FIELDS_INDEX];
    ccd->depth        = msg[CCD_CCD_DEPTH_INDEX];
    ccd->dac_bits     = msg[CCD_CCD_DAC_INDEX];
    ccd->color        = msg[CCD_CCD_COLOR_INDEX];
    ccd->caps         = msg[CCD_CCD_CAPS_INDEX];
    strncpy(ccd->camera, (char *)&msg[CCD_CCD_NAME_INDEX], CCD_CCD_NAME_LEN);
    ccd->camera[CCD_CCD_NAME_LEN] = '\0';
    for (i = CCD_CCD_NAME_LEN - 1; i && (ccd->camera[i] == '\0' || ccd->camera[i] == ' '); i--)
        ccd->camera[i] = '\0'; // Strip off trailing spaces
    return (TRUE);
}
int ccd_release(struct ccd_dev *ccd)
{
    int fd;

    if ((fd = ccd->fd))
    {
        ccd->fd = 0;
        return (close(fd));
    }
    return (0);
}
/*
 * Request exposure.
 */
void ccd_expose(struct ccd_exp *exposure)
{
    char           str[32];
    struct timeval now;
    CCD_ELEM_TYPE  msg[CCD_MSG_EXP_LEN/CCD_ELEM_SIZE];
    static int     image_num = 1;
    /*
     * Send the capture request.
     */
    msg[CCD_MSG_HEADER_INDEX]    = CCD_MSG_HEADER;
    msg[CCD_MSG_LENGTH_LO_INDEX] = CCD_MSG_EXP_LEN;
    msg[CCD_MSG_LENGTH_HI_INDEX] = 0;
    msg[CCD_MSG_INDEX]           = CCD_MSG_EXP;
    msg[CCD_EXP_WIDTH_INDEX]     = exposure->width;
    msg[CCD_EXP_HEIGHT_INDEX]    = exposure->height;
    msg[CCD_EXP_XOFFSET_INDEX]   = exposure->xoffset;
    msg[CCD_EXP_YOFFSET_INDEX]   = exposure->yoffset;
    msg[CCD_EXP_XBIN_INDEX]      = exposure->xbin;
    msg[CCD_EXP_YBIN_INDEX]      = exposure->ybin;
    msg[CCD_EXP_DAC_INDEX]       = exposure->dac_bits;
    msg[CCD_EXP_FLAGS_INDEX]     = exposure->flags;
    msg[CCD_EXP_MSEC_LO_INDEX]   = exposure->msec & 0xFFFF;
    msg[CCD_EXP_MSEC_HI_INDEX]   = exposure->msec >> 16;
    write(exposure->ccd->fd, (char *)msg, CCD_MSG_EXP_LEN);
    exposure->read_row = 0;
    /*
     * Set start time.
     */
    gettimeofday(&now, NULL);
    exposure->start = now.tv_sec  * 1000 + now.tv_usec  / 1000;
    if (exposure->image->time[0] == '\0')
    {
        time_t     now = time(NULL);
        struct tm *utc = gmtime(&now);
        sprintf(exposure->image->date, "%04d-%02d-%02d", utc->tm_year+1900, utc->tm_mon + 1, utc->tm_mday);
        sprintf(exposure->image->time, "%02d:%02d:%02d", utc->tm_hour, utc->tm_min, utc->tm_sec);
    }
    /*
     * Set exposure data.
     */
    if (!exposure->image->pixels)
        exposure->image->pixels = malloc(exposure->image->width * ((exposure->image->depth + 7)/8) + CCD_MSG_IMAGE_LEN);
}
/*
 * Read exposed image row.
 */
int ccd_read_row(struct ccd_exp *exposure)
{
    int            msg_len;
    int            sizeof_pixel = (exposure->ccd->depth + 7) / 8;
    int            row_bytes    = (exposure->width / exposure->xbin) * sizeof_pixel;
    CCD_ELEM_TYPE *msg          = (CCD_ELEM_TYPE *)exposure->image->pixels;

    if (exposure->read_row == 0)
    {
        /*
         * Get header plus first scanline.
         */
        if ((msg_len = read(exposure->ccd->fd, exposure->image->pixels, row_bytes + CCD_MSG_IMAGE_LEN)) > 0)
        {
            if (msg[CCD_MSG_INDEX] == CCD_MSG_IMAGE)
            {
                /* 
                 * Validate message length.
                 */
                if ((msg[CCD_MSG_LENGTH_LO_INDEX] + (msg[CCD_MSG_LENGTH_HI_INDEX] << 16)) != (row_bytes * (exposure->height / exposure->ybin) + CCD_MSG_IMAGE_LEN))
                {
                    fprintf(stderr, "Image size discrepency! Read %d, expected %d\n", msg[CCD_MSG_LENGTH_LO_INDEX] + (msg[CCD_MSG_LENGTH_HI_INDEX] << 16), row_bytes * (exposure->height / exposure->ybin) + CCD_MSG_IMAGE_LEN);
                    exposure->read_row = 0;
                    return (0);
                }
                /*
                 * Read rest of first scanline if it didn't make it (should never happen).
                 */
                if (msg_len != row_bytes + CCD_MSG_IMAGE_LEN)
                    read(exposure->ccd->fd, &exposure->image->pixels[msg_len - CCD_MSG_IMAGE_LEN], row_bytes - msg_len - CCD_MSG_IMAGE_LEN);
                exposure->read_row = (exposure->height / exposure->ybin > 1) ? 1 : 0;
                /*
                 * Move image data down to replace header.
                 */
                memcpy(exposure->image->pixels, exposure->image->pixels + CCD_MSG_IMAGE_LEN, row_bytes);
            }
            else
                fprintf(stderr, "Error: wrong message 0x%04X\n", msg[CCD_MSG_INDEX]);
        }
        else
            fprintf(stderr, "Error reading exposure:%s\n", strerror(errno));
    }
    else 
    {
        read(exposure->ccd->fd, exposure->image->pixels, row_bytes);
        if (++(exposure->read_row) == exposure->height / exposure->ybin)
            /*
             * Loaded entire frame.
             */
            exposure->read_row = 0;
    }
    return (exposure->read_row != 0);
}
/*
 * Calibrate raw image.
 */
void ccd_image_calibrate_row(struct ccd_image *raw, struct ccd_image *bias, struct ccd_image *dark, struct ccd_image *flat)
{
    unsigned int  pixel_size, x;
    float         pixel, pixel_max;
    static float  flat_ave, dark_scale;
    static struct ccd_image *prev_flat = NULL;
    static struct ccd_image *prev_dark = NULL;

    if (!bias && !dark && !flat)
        return;
    pixel_size = ((raw->depth + 7) / 8);
    pixel_max  = raw->datamax;
    /*
     * Calc dark scale factor based on exposure ratio.
     */
    if (dark && prev_dark != dark)
        dark_scale = (dark->exposure) ? ((float)raw->exposure / (float)dark->exposure) : 1.0;
    /*
     * Find flat average. Do a little optimization to avoid this for the same flat over and over.
     */
    if (flat && prev_flat != flat)
    {
        unsigned int quarterx = flat->width  / 4;
        flat_ave = 0.0;

#define PIXEL_LOOP(pixel_type)                                              \
        for (x = quarterx; x < flat->width - quarterx; x++)                 \
            flat_ave += ((pixel_type *)flat->pixels)[x];

        PIXEL_SIZE_CASE(pixel_size);
#undef PIXEL_LOOP

        flat_ave /= flat->width - quarterx * 2;
        prev_flat = flat;
    }
    /*
     * Apply calibration to raw image.
     */

#define PIXEL_LOOP(pixel_type)                                                                                                                \
    for (x = 0; x < raw->width; x++)                                                                                                          \
    {                                                                                                                                         \
        pixel = ((pixel_type *)raw->pixels)[x];                                                                                               \
        if (bias) pixel  = ((pixel_type *)bias->pixels)[x]              > pixel ? 0.0 : pixel - ((pixel_type *)bias->pixels)[x];              \
        if (dark) pixel  = ((pixel_type *)dark->pixels)[x] * dark_scale > pixel ? 0.0 : pixel - ((pixel_type *)dark->pixels)[x] * dark_scale; \
        if (flat) pixel *= flat_ave / (((pixel_type *)flat->pixels)[x] ? ((pixel_type *)flat->pixels)[x] : 1.0);                              \
        ((pixel_type *)raw->pixels)[x] = pixel > pixel_max ? (pixel_type)pixel_max : (pixel_type)pixel;                                                        \
    }

    PIXEL_SIZE_CASE(pixel_size);
#undef PIXEL_LOOP

}


