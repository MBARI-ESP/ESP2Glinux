/*
 * GCCD - Gnome CCD Camera Controller
 * Copyright (C) 2001 David Schmenk
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include "gccd.h"
#define DUPLICATE_FIRST_REGISTERED_IMAGE
//#define CCD_DEBUG

/*
 * Handy image list manipulators.
 */
static struct ccd_image *image_list_head = NULL;
static void ccd_image_append_list(struct ccd_image *image)
{
    struct ccd_image *image_list;

    image->next = NULL;
    if (image_list_head == NULL)
    {
        image_list_head = image;
    }
    else
    {
        for (image_list = image_list_head; image_list->next; image_list = image_list->next);
        image_list->next = image;
    }
}
static void ccd_image_remove_list(struct ccd_image *image)
{
    struct ccd_image *image_list;

    if (image_list_head == image)
    {
        image_list_head = image->next;
    }
    else
    {
        for (image_list = image_list_head; image_list && image_list->next != image; image_list = image_list->next);
        if (image_list->next == image)
            image_list->next = image->next;
        else
            fprintf(stderr, "ccd_image_remove_list:Removing unknown image.\n");
    }
}
struct ccd_image *ccd_image_first(void)
{
    return (image_list_head);
}
struct ccd_image *ccd_image_next(struct ccd_image *image)
{
    return (image->next);
}
struct ccd_image *ccd_image_find_by_name(char *name)
{
    struct ccd_image *image_list;

    for (image_list = image_list_head; image_list; image_list = image_list->next)
        if (!strcmp(image_list->name, name))
            return (image_list);
    return (NULL);
}
struct ccd_image *ccd_image_find_by_path(char *path)
{
    char filename[PATH_MAX];
    struct ccd_image *image_list;

    for (image_list = image_list_head; image_list; image_list = image_list->next)
    {
        filename[0] = '\0';
        strcpy(filename, image_list->dir);
        strcat(filename, "/");
        strcat(filename, image_list->name);
        if (image_list->ext[0])
        {
            strcat(filename, ".");
            strcat(filename, image_list->ext);
        }
        if (!strcmp(filename, path))
            return (image_list);
    }
    return (NULL);
}
/*
 * Constructors and destructors.
 */
struct ccd_image *ccd_image_new(char *path)
{
    struct ccd_image *image = (struct ccd_image *)malloc(sizeof(struct ccd_image));
    memset(image, 0, sizeof(struct ccd_image));
    ccd_image_set_dir_name_ext(image, path);
    ccd_image_append_list(image);
    return (image);
}
struct ccd_image *ccd_image_new_from_file(char *path)
{
    struct ccd_image *image = (struct ccd_image *)malloc(sizeof(struct ccd_image));
    memset(image, 0, sizeof(struct ccd_image));
    ccd_image_set_dir_name_ext(image, path);
    if (ccd_image_load_fits(image) < 0)
    {
        free(image);
        image = NULL;
    }
    else
    {
        ccd_image_append_list(image);
    }
    return (image);
}
struct ccd_image *ccd_image_dup(struct ccd_image *image_orig)
{
    struct ccd_image *image = (struct ccd_image *)malloc(sizeof(struct ccd_image));
    memcpy(image, image_orig, sizeof(struct ccd_image));
    ccd_image_append_list(image);
    return (image);
}
void ccd_image_delete(struct ccd_image *image)
{
    ccd_image_remove_list(image);
    if (image->pixels)
        free(image->pixels);
    free(image);
}
/*
 * Extract directory, base and extension of file path.
 */
int ccd_image_set_dir_name_ext(struct ccd_image *image, char *path)
{
    char *dir, *name, *ext;

    if (!path)
        return (-1);
    if ((dir = strrchr(path, '/')))
    {
        image->dir[0] = '\0';
        strncat(image->dir, path, dir - path);
        name = dir + 1;
    }
    else
    {
        getcwd(image->dir, DIR_STRING_LENGTH);
        name = path;
    }
    if (!name[0])
        return (-1);
    if ((ext = strrchr(name, '.')))
    {
        image->name[0] = '\0';
        strncat(image->name, name, ext - name);
        strcpy(image->ext, ext + 1);
    }
    else
    {
        strcpy(image->name, name);
        image->ext[0] = '\0';
    }
    return (0);
}
void ccd_image_histogram(struct ccd_image *image)
{
    unsigned long i, j, pixel, pixel_size;
    float pixel_sum, histogram_scale;

    /*
     * Find min/ave/max values.
     */
    if (image->pixmax == 0)
    {
        image->pixmin = ~0;
        pixel_sum     = 0;
        pixel_size    = ((image->depth + 7) / 8);
        /*
         * Clear histogram.
         */
        histogram_scale = (float)(HISTOGRAM_BINS - 1) / (1 << image->depth);
        for (i = 0; i < HISTOGRAM_BINS; i++)
            image->histogram[i] = 0;

#define PIXEL_LOOP(pixel_type)                                                  \
        for (j = 0; j < image->height; j++)                                     \
            for (i = 0; i < image->width; i++)                                  \
            {                                                                   \
                pixel = ((pixel_type *)image->pixels)[j * image->width + i];    \
                pixel_sum += pixel;                                             \
                image->histogram[(int)(pixel * histogram_scale)]++;             \
                if (pixel < image->pixmin)                                      \
                    image->pixmin = pixel;                                      \
                if (pixel > image->pixmax)                                      \
                    image->pixmax = pixel;                                      \
            }

        PIXEL_SIZE_CASE(pixel_size);
#undef PIXEL_LOOP

        image->pixave = pixel_sum / (image->height * image->width);
    }
    if (image->datamax == 0)
        image->datamax = ~0UL >> (32 - image->depth);
}
/*
 * FITS file routines.
 */
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
static void read_string(char *src, char *dst)
{
    while (*src && *src != '\'')
        src++;
    if (*src)
        src++;
    while (*src && *src != '\'')
        *dst++ = *src++;
    *dst = '\0';
}
/*
 * Save image to FITS file.
 */
int ccd_image_save_fits(struct ccd_image *image)
{
    char           filename[PATH_MAX];
    char           record[FITS_CARD_COUNT][FITS_CARD_SIZE];
    unsigned char *fits_pixels;
    int            i, j, k, image_size, image_pitch, pixel_size, fd;

    /*
     * Create file.
     */
    filename[0] = '\0';
    strcpy(filename, image->dir);
    strcat(filename, "/");
    strcat(filename, image->name);
    if (image->ext[0])
    {
        strcat(filename, ".");
        strcat(filename, image->ext);
    }
    fd = creat(filename, 0666);
    /*
     * Fill header records.
     */
    memset(record, ' ', FITS_RECORD_SIZE);
    i = 0;
    sprintf(record[i++], "SIMPLE  = %20c", 'T');
    sprintf(record[i++], "BITPIX  = %20d", image->depth);
    sprintf(record[i++], "NAXIS   = %20d", 2);
    sprintf(record[i++], "NAXIS1  = %20d",   image->width);
    sprintf(record[i++], "NAXIS2  = %20d",   image->height);
    sprintf(record[i++], "BZERO   = %20f", 0.0);
    sprintf(record[i++], "BSCALE  = %20f", 1.0);
    sprintf(record[i++], "DATAMIN = %20u", image->datamin);
    sprintf(record[i++], "DATAMAX = %20u", image->datamax);
    if (image->color != CCD_COLOR_MONOCHROME)
        sprintf(record[i++], "CFORMAT = %20d",    image->color);
    if (image->filter != CCD_COLOR_MONOCHROME)
        sprintf(record[i++], "FILTER  = %20d",    image->filter);
    sprintf(record[i++], "XPIXSZ  = %20f", image->pixel_width);
    sprintf(record[i++], "YPIXSZ  = %20f", image->pixel_height);
    sprintf(record[i++], "CREATOR = 'Gnome CCD Camera Controller %s'", VERSION);
    sprintf(record[i++], "DATE-OBS= '%sT%s'", image->date, image->time);
    sprintf(record[i++], "EXPOSURE= %20f", (float)image->exposure / 1000.0);
    sprintf(record[i++], "OBJECT  = '%s'", image->object);
    sprintf(record[i++], "INSTRUME= '%s'", image->camera);
    sprintf(record[i++], "OBSERVER= '%s'", image->observer);
    sprintf(record[i++], "TELESCOP= '%s'", image->telescope);
    sprintf(record[i++], "LOCATION= '%s'", image->location);
    for (j = 0; j < MAX_PROCESS_HISTORY; j++)
    {
        if (image->history[j][0])
        {
            sprintf(record[i++], "HISTORY %s", image->history[j]);
            if (i >= FITS_CARD_COUNT)
            {
                for (k = 0; k < FITS_RECORD_SIZE; k++)
                    if (((char *)record)[k] == '\0')
                        ((char *)record)[k] = ' ';
                write(fd, record, FITS_RECORD_SIZE);
                memset(record, ' ', FITS_RECORD_SIZE);
                i = 0;
            }
        }
    }
    for (j = 0; j < MAX_COMMENTS; j++)
    {
        if (image->comments[j][0])
        {
            sprintf(record[i++], "COMMENT %s", image->comments[j]);
            if (i >= FITS_CARD_COUNT)
            {
                for (k = 0; k < FITS_RECORD_SIZE; k++)
                    if (((char *)record)[k] == '\0')
                        ((char *)record)[k] = ' ';
                write(fd, record, FITS_RECORD_SIZE);
                memset(record, ' ', FITS_RECORD_SIZE);
                i = 0;
            }
        }
    }
    sprintf(record[i++], "END");
    for (k = 0; k < FITS_RECORD_SIZE; k++)
        if (((char *)record)[k] == '\0')
            ((char *)record)[k] = ' ';
    write(fd, record, FITS_RECORD_SIZE);
    /*
     * Convert and write image data.
     */
    pixel_size  = ((image->depth + 7) / 8);
    image_pitch = image->width  * pixel_size;
    image_size  = image->height * image_pitch;
    fits_pixels = malloc(image_pitch);
    for (i = 0; i < image->height; i++)
    {
        convert_pixels(image->pixels + image_size - (i+1) * image_pitch, fits_pixels, 0, pixel_size, image->width);
        write(fd, fits_pixels, image_pitch);
    }
    free(fits_pixels);
    /*
     * Pad remaining record size with zeros and close.
     */
    memset(record, 0, FITS_RECORD_SIZE);
    if (image_size % FITS_RECORD_SIZE)
        write(fd, record, FITS_RECORD_SIZE - (image_size % FITS_RECORD_SIZE));
    close(fd);
    return (0);
}
/*
 * Load image from FITS file.
 */
int ccd_image_load_fits(struct ccd_image *image)
{
    char           filename[PATH_MAX];
    char           record[FITS_CARD_COUNT+1][FITS_CARD_SIZE];
    char           key[10];
    unsigned char *fits_pixels;
    int            i, j, k, l, image_size, image_pitch, pixel_size, fd, done;

    /*
     * Clear out all image fields.
     */
    memset(image, 0, (unsigned int)&image->dir - (unsigned int)image);
    image->zero         = 0.0;
    image->scale        = 1.0;
    image->color        = CCD_COLOR_MONOCHROME;
    image->filter       = CCD_COLOR_MONOCHROME;
    image->pixel_width  = 10.0;
    image->pixel_height = 10.0;
    /*
     * Read file.
     */
    strcpy(filename, image->dir);
    strcat(filename, "/");
    strcat(filename, image->name);
    if (image->ext[0])
    {
        strcat(filename, ".");
        strcat(filename, image->ext);
    }
    if ((fd = open(filename, O_RDONLY, 0)) > 0)
    {
        /*
         * Read header records.
         */
        read(fd, record, FITS_RECORD_SIZE);
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
                sscanf(&record[i++][10], "%f", &image->zero);
            else if (!strcmp(key, "BSCALE"))
                sscanf(&record[i++][10], "%f", &image->scale);
            else if (!strcmp(key, "NAXIS1"))
                sscanf(&record[i++][10], "%d", &image->width);
            else if (!strcmp(key, "NAXIS2"))
                sscanf(&record[i++][10], "%d", &image->height);
            else if (!strcmp(key, "DATAMIN"))
                sscanf(&record[i++][10], "%u", &image->datamin);
            else if (!strcmp(key, "DATAMAX"))
                sscanf(&record[i++][10], "%u", &image->datamax);
            else if (!strcmp(key, "PIXWIDTH"))
                sscanf(&record[i++][10], "%f", &image->pixel_width);
            else if (!strcmp(key, "XPIXSZ"))
                sscanf(&record[i++][10], "%f", &image->pixel_width);
            else if (!strcmp(key, "PIXHEIGH"))
                sscanf(&record[i++][10], "%f", &image->pixel_height);
            else if (!strcmp(key, "YPIXSZ"))
                sscanf(&record[i++][10], "%f", &image->pixel_height);
            else if (!strcmp(key, "CFORMAT"))
                sscanf(&record[i++][10], "%d", &image->color);
            else if (!strcmp(key, "FILTER"))
                sscanf(&record[i++][10], "%d", &image->filter);
            else if (!strcmp(key, "EXPOSURE"))
            {
                float f;
                sscanf(&record[i++][10], "%f", &f);
                image->exposure = f * 1000.0;
            }
            else if (!strcmp(key, "DATE-OBS"))
            {
                read_string(&record[i++][10], image->date);
                if (image->date[10] == 'T')
                {
                    strcpy(image->time, &image->date[11]);
                    image->date[10] = '\0';
                }
            }
            else if (!strcmp(key, "TIME-OBS"))
               read_string(&record[i++][10], image->time);
            else if (!strcmp(key, "OBJECT"))
               read_string(&record[i++][10], image->object);
            else if (!strcmp(key, "OBSERVER"))
               read_string(&record[i++][10], image->observer);
            else if (!strcmp(key, "INSTRUME"))
               read_string(&record[i++][10], image->camera);
            else if (!strcmp(key, "TELESCOP"))
               read_string(&record[i++][10], image->telescope);
            else if (!strcmp(key, "LOCATION"))
                read_string(&record[i++][10], image->location);
            else if (!strcmp(key, "HISTORY"))
                strcpy(image->history[k++], &record[i++][8]);
            else if (!strcmp(key, "COMMENT"))
                strcpy(image->comments[j++], &record[i++][8]);
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
            convert_pixels(fits_pixels, image->pixels + image_size - (i+1) * image_pitch, image->zero == 0.0 ? 0 : 0x80, pixel_size, image->width);
        }
        free(fits_pixels);
        close(fd);
        ccd_image_histogram(image);
    }
    else
        return (-1);
    return (0);
}
/*
 * Quick-sort pixels.
 */
static void sort_pixels(unsigned char *pixels, unsigned int size, unsigned int count)
{
    unsigned long left, right, part, last, i, tmp;
    unsigned long stack, right_prev[32];

    if (count < 2)
        return;
    stack = 0;
    left  = 0;
    right = count - 1;

#define PIXEL_LOOP(pixel_type)                                                                      \
    while (left < count - 1)                                                                        \
    {                                                                                               \
        last = left;                                                                                \
        while (right > left)                                                                        \
        {                                                                                           \
            last                         = (left + right) / 2;                                      \
            part                         = ((pixel_type *)pixels)[last];                            \
            ((pixel_type *)pixels)[last] = ((pixel_type *)pixels)[left];                            \
            last                         = left;                                                    \
            for (i = left + 1; i <= right; i++)                                                     \
            {                                                                                       \
                if (((pixel_type *)pixels)[i] < part)                                               \
                {                                                                                   \
                    tmp                          = ((pixel_type *)pixels)[i];                       \
                    ((pixel_type *)pixels)[i]    = ((pixel_type *)pixels)[++last];                  \
                    ((pixel_type *)pixels)[last] = tmp;                                             \
                }                                                                                   \
            }                                                                                       \
            ((pixel_type *)pixels)[left] = ((pixel_type *)pixels)[last];                            \
            ((pixel_type *)pixels)[last] = part;                                                    \
            /*                                                                                      \
             * Save right endpoint on stack.                                                        \
             */                                                                                     \
            right_prev[stack++] = right;                                                            \
            right = last;                                                                           \
        }                                                                                           \
        left  = last + 1;                                                                           \
        right = right_prev[--stack];                                                                \
    }

    PIXEL_SIZE_CASE(size);
#undef PIXEL_LOOP

}
/*
 * Invert pixels.
 */
void ccd_image_invert(struct ccd_image *image)
{
    unsigned int x, y;

#define PIXEL_LOOP(pixel_type)                                                                                          \
    for (y = 0; y < image->height; y++)                                                                                 \
        for (x = 0; x < image->width; x++)                                                                              \
            ((pixel_type *)image->pixels)[y * image->width + x] = ~((pixel_type *)image->pixels)[y * image->width + x];

    PIXEL_SIZE_CASE((image->depth + 7) / 8);
#undef PIXEL_LOOP

    image->pixmin = image->pixmax = 0;
    ccd_image_histogram(image);
}
/*
 * Reverse scanline order.
 */
void ccd_image_flip_vert(struct ccd_image *image)
{
    unsigned int   pixel_size, image_pitch, x, y;
    unsigned long  tmp;
    unsigned char *top, *bottom;

    pixel_size  = ((image->depth + 7) / 8);
    image_pitch = image->width  * pixel_size;
    top         = image->pixels;
    bottom      = top + (image->height - 1) * image_pitch;

#define PIXEL_LOOP(pixel_type)                                      \
    for (y = 0; y < image->height / 2; y++)                         \
    {                                                               \
        for (x = 0; x < image->width; x++)                          \
        {                                                           \
            tmp                       = ((pixel_type *)top)[x];     \
            ((pixel_type *)top)[x]    = ((pixel_type *)bottom)[x];  \
            ((pixel_type *)bottom)[x] = tmp;                        \
        }                                                           \
        top    += image_pitch;                                      \
        bottom -= image_pitch;                                      \
    }

    PIXEL_SIZE_CASE(pixel_size);
#undef PIXEL_LOOP

}
/*
 * Reverse scanline pixel order.
 */
void ccd_image_flip_horiz(struct ccd_image *image)
{
    unsigned int   pixel_size, image_pitch, x, y;
    unsigned long  tmp;
    unsigned char *scanline;

    pixel_size  = ((image->depth + 7) / 8);
    image_pitch = image->width  * pixel_size;
    scanline    = image->pixels;

#define PIXEL_LOOP(pixel_type)                                                                              \
    for (y = 0; y < image->height; y++)                                                                     \
    {                                                                                                       \
        for (x = 0; x < image->width / 2; x++)                                                              \
        {                                                                                                   \
            tmp                                            = ((pixel_type *)scanline)[x];                   \
            ((pixel_type *)scanline)[x]                    = ((pixel_type *)scanline)[image->width - x - 1];\
            ((pixel_type *)scanline)[image->width - x - 1] = tmp;                                           \
        }                                                                                                   \
        scanline += image_pitch;                                                                            \
    }

    PIXEL_SIZE_CASE(pixel_size);
#undef PIXEL_LOOP

}
/*
 * Rotate image in   one of 90 degree directions.
 */
void ccd_image_rotate(struct ccd_image *image, int angle)
{
    int            x, y;
    unsigned char *rot_pixels = malloc(image->width * image->height * ((image->depth + 7) / 8));
    switch (angle)
    {
        case 270:
        case -90:
#define PIXEL_LOOP(pixel_type)                                                                                                                                              \
            for (y = 0; y < image->height; y++)                                                                                                                         \
                for (x = 0; x < image->width; x++)                                                                                                                      \
                    ((pixel_type *)rot_pixels)[x * image->height + (image->height - 1 - y)] = ((pixel_type *)image->pixels)[y * image->width + x];  \

            PIXEL_SIZE_CASE((image->depth + 7) / 8);
#undef PIXEL_LOOP
            free(image->pixels);
            image->pixels = rot_pixels;
            x             = image->width;
            image->width  = image->height;
            image->height = x;
            break;
        case 180:
        case -180:
#define PIXEL_LOOP(pixel_type)                                                                                                                                              \
            for (y = 0; y < image->height; y++)                                                                                                                         \
                for (x = 0; x < image->width; x++)                                                                                                                      \
                    ((pixel_type *)rot_pixels)[(image->height - 1 - y) * image->width + (image->width - 1 - x)] = ((pixel_type *)image->pixels)[y * image->width + x];  \

            PIXEL_SIZE_CASE((image->depth + 7) / 8);
#undef PIXEL_LOOP
            free(image->pixels);
            image->pixels = rot_pixels;
            break;
        case 90:
        case -270:
#define PIXEL_LOOP(pixel_type)                                                                                                                          \
            for (y = 0; y < image->height; y++)                                                                                                     \
                for (x = 0; x < image->width; x++)                                                                                                  \
                    ((pixel_type *)rot_pixels)[(image->width - 1 - x) * image->height + y] = ((pixel_type *)image->pixels)[y * image->width + x];   \

            PIXEL_SIZE_CASE((image->depth + 7) / 8);
#undef PIXEL_LOOP
            free(image->pixels);
            image->pixels = rot_pixels;
            x             = image->width;
            image->width  = image->height;
            image->height = x;
            break;
        case 0:
        default:
            free(rot_pixels);
            break;
    }
}
/*
 * Scale image.
 */
void ccd_image_scale(struct ccd_image *image, unsigned int scale_width, unsigned int scale_height)
{
    float             scale_x, scale_y, x, y, x_frac, y_frac;
    unsigned int      x_int, y_int, x_scale, y_scale;
    unsigned char    *scale_pixels;

    scale_pixels = malloc(scale_width * scale_height * ((image->depth + 7) / 8));
    scale_x      = (float)image->width  / (float)scale_width;
    scale_y      = (float)image->height / (float)scale_height;
    /*
     * LERP the scaled pixel from the original.
     */

#define PIXEL_LOOP(pixel_type)                                                                                                                                                                  \
    for (y_scale = 0; y_scale < scale_height; y_scale++)                                                                                                                                    \
    {                                                                                                                                                                                       \
        y      = y_scale * scale_y;                                                                                                                                                         \
        y_frac = y - floor(y);                                                                                                                                                              \
        y_int  = (unsigned int)y;                                                                                                                                                           \
        for (x_scale = 0; x_scale < scale_width; x_scale++)                                                                                                                                 \
        {                                                                                                                                                                                   \
            x      = x_scale * scale_x;                                                                                                                                                     \
            x_frac = x - floor(x);                                                                                                                                                          \
            x_int  = (unsigned int)x;                                                                                                                                                       \
            if ((y_int < image->height - 1) && (x_int < image->width - 1))                                                                                                                  \
                ((pixel_type *)scale_pixels)[y_scale * scale_width + x_scale] = ((1.0 - x_frac) * (1.0 - y_frac)) * ((pixel_type *)image->pixels)[y_int * image->width       + x_int]       \
                                                                              + (       x_frac  * (1.0 - y_frac)) * ((pixel_type *)image->pixels)[y_int * image->width       + x_int + 1]   \
                                                                              + ((1.0 - x_frac) *        y_frac)  * ((pixel_type *)image->pixels)[(y_int + 1) * image->width + x_int]       \
                                                                              + (       x_frac  *        y_frac)  * ((pixel_type *)image->pixels)[(y_int + 1) * image->width + x_int + 1];  \
            else if ((y_int == image->height - 1) && (x_int == image->width - 1))                                                                                                           \
                ((pixel_type *)scale_pixels)[y_scale * scale_width + x_scale] = ((pixel_type *)image->pixels)[y_int * image->width + x_int];                                                \
            else if (y_int == image->height - 1)                                                                                                                                            \
                ((pixel_type *)scale_pixels)[y_scale * scale_width + x_scale] = (1.0 - x_frac) * ((pixel_type *)image->pixels)[y_int * image->width + x_int]                                \
                                                                              + (      x_frac) * ((pixel_type *)image->pixels)[y_int * image->width + x_int + 1];                           \
            else /*if (x_int == image->width - 1) */                                                                                                                                        \
                ((pixel_type *)scale_pixels)[y_scale * scale_width + x_scale] = (1.0 - y_frac) * ((pixel_type *)image->pixels)[y_int * image->width       + x_int]                          \
                                                                              + (      y_frac) * ((pixel_type *)image->pixels)[(y_int + 1) * image->width + x_int];                         \
        }                                                                                                                                                                                   \
    }

    PIXEL_SIZE_CASE((image->depth + 7) / 8);
#undef PIXEL_LOOP

    free(image->pixels);
    image->pixels = scale_pixels;
    image->width  = scale_width;
    image->height = scale_height;
}
/*
 * Convolve image with kernel and return resultant frame.
 */
unsigned char *ccd_image_convolve(struct ccd_image *image, unsigned char *conv_frame, unsigned xradius, unsigned yradius, float *kernel)
{
    unsigned int pixel_size, image_pitch, image_size;
    int          x, y, i, j, kwidth, kheight, kx, ky;
    float        ksum, pixel;

    pixel_size    = ((image->depth + 7) / 8);
    image_pitch   = image->width  * pixel_size;
    image_size    = image->height * image_pitch;
    if (conv_frame == NULL)
        conv_frame = malloc(image_size);
    kwidth        = xradius * 2 + 1;
    kheight       = yradius * 2 + 1;
    ksum          = 0.0;
    for (j = 0; j < kheight; j++)
        for (i = 0; i < kwidth; i++)
            ksum += kernel[j * kwidth + i];
    if (fabs(ksum) < 1.0e-9)
        ksum = (ksum < 0.0) ? -1.0e-9 : 1.0e-9;
    ksum = 1.0 / ksum;

#define PIXEL_LOOP(pixel_type)                                                                                                                  \
    for (y = 0; y < image->height; y++)                                                                                                         \
        for (x = 0; x < image->width; x++)                                                                                                      \
        {                                                                                                                                       \
            pixel = 0.0;                                                                                                                        \
            for (j = 0; j < kheight; j++)                                                                                                       \
                for (i = 0; i < kwidth; i++)                                                                                                    \
                {                                                                                                                               \
                    ky = y + j - yradius;                                                                                                       \
                    kx = x + i - xradius;                                                                                                       \
                    if (ky > 0 && ky < image->height && kx > 0 && kx < image->width)                                                            \
                        pixel += ((pixel_type *)image->pixels)[ky * image->width + kx] * kernel[j * kwidth + i];                                \
                }                                                                                                                               \
            pixel *= ksum;                                                                                                                      \
            ((pixel_type *)conv_frame)[y * image->width + x] = min(max(0, pixel), image->datamax);                                              \
        }

    PIXEL_SIZE_CASE(pixel_size);
#undef PIXEL_LOOP

    return (conv_frame);
}
/*
 * Deconvolve image and return resultant frame.
 * Meant to be called iteratively.
 */
unsigned char *ccd_image_deconvolve(struct ccd_image *image, unsigned char *current_frame, unsigned char *next_frame, unsigned xradius, unsigned yradius, float *kernel, float noise_adj, int op)
{
    unsigned int pixel_size, image_pitch, image_size, kwidth, kheight;
    int          x, y, kx, ky, i, j;
    float        ksum, pixel, w;

    pixel_size    = ((image->depth + 7) / 8);
    image_pitch   = image->width  * pixel_size;
    image_size    = image->height * image_pitch;
    if (next_frame == NULL)
        next_frame = malloc(image_size);
    if (current_frame == NULL)
        current_frame = image->pixels;
    kwidth        = xradius * 2 + 1;
    kheight       = yradius * 2 + 1;
    ksum          = 0.0;
    for (j = 0; j < kheight; j++)
        for (i = 0; i < kwidth; i++)
            ksum += kernel[j * kwidth + i];
    if (fabs(ksum) < 1.0e-9)
        ksum = (ksum < 0.0) ? -1.0e-9 : 1.0e-9;
    ksum = 1.0 / ksum;

#define PIXEL_LOOP(pixel_type)                                                                                                                  \
    for (y = 0; y < image->height; y++)                                                                                                         \
        for (x = 0; x < image->width; x++)                                                                                                      \
        {                                                                                                                                       \
            pixel = 0.0;                                                                                                                        \
            for (j = 0; j < kheight; j++)                                                                                                       \
                for (i = 0; i < kwidth; i++)                                                                                                    \
                {                                                                                                                               \
                    ky = y + j - yradius;                                                                                                       \
                    kx = x + i - xradius;                                                                                                       \
                    if (ky > 0 && ky < image->height && kx > 0 && kx < image->width)                                                            \
                        pixel += ((pixel_type *)current_frame)[ky * image->width + kx] * kernel[j * kwidth + i];                                \
                }                                                                                                                               \
            pixel *= ksum;                                                                                                                      \
            pixel = min(max(0, pixel), image->datamax);                                                                                         \
            if (op == CCD_IMAGE_DECONVOLVE_VAN_CITTERT)                                                                                         \
            {                                                                                                                                   \
                pixel = ((pixel_type *)image->pixels)[y * image->width + x] - pixel;                                                            \
                if (pixel <= image->pixmin)                                                                                                     \
                    w = 0.0;                                                                                                                    \
                else if (pixel >= image->pixmax)                                                                                                \
                    w = 1.0;                                                                                                                    \
                else                                                                                                                            \
                    w = pow(sin(M_PI_2 * (pixel - image->pixmin) / (image->pixmax - image->pixmin)), noise_adj);                                \
                ((pixel_type *)next_frame)[y * image->width + x] = ((pixel_type *)current_frame)[y * image->width + x] + w;                     \
            }                                                                                                                                   \
            else /* RICHARDSON_LUCY */                                                                                                          \
            {                                                                                                                                   \
                pixel = (pixel > 1.0e-9) ? ((pixel_type *)image->pixels)[y * image->width + x] / pixel : 1.0;                                   \
                if (((pixel_type *)current_frame)[y * image->width + x] <= image->pixmin)                                                       \
                    w = 0.0;                                                                                                                    \
                else if (((pixel_type *)current_frame)[y * image->width + x] >= image->pixmax)                                                  \
                    w = 1.0;                                                                                                                    \
                else                                                                                                                            \
                    w = pow(sin(M_PI_2 * (((pixel_type *)current_frame)[y * image->width + x] - image->pixmin) / (image->pixmax - image->pixmin)), noise_adj);\
                ((pixel_type *)next_frame)[y * image->width + x] = ((pixel_type *)current_frame)[y * image->width + x] * (w * (pixel - 1.0) + 1.0);\
            }                                                                                                                                   \
        }

    PIXEL_SIZE_CASE(pixel_size);
#undef PIXEL_LOOP

    return (next_frame);
}
/*
 * Image registration.
 */
static void calc_centroid(struct ccd_image *image, unsigned char *pixels, unsigned int x, unsigned int y, unsigned int x_radius, unsigned int y_radius, float *x_centroid, float *y_centroid, unsigned long min)
{
    unsigned int i, j;
    float        sum;

    *x_centroid = 0.0;
    *y_centroid = 0.0;
    sum         = 0.0;

#define PIXEL_LOOP(pixel_type)                                                  \
    for (j = y - y_radius; j <= y + y_radius; j++)                              \
        for (i = x - x_radius; i <= x + x_radius; i++)                          \
            if (((pixel_type *)pixels)[j * image->width + i] > min)             \
            {                                                                   \
                *x_centroid += i * ((pixel_type *)pixels)[j * image->width + i];\
                *y_centroid += j * ((pixel_type *)pixels)[j * image->width + i];\
                sum         +=     ((pixel_type *)pixels)[j * image->width + i];\
            }

    PIXEL_SIZE_CASE((image->depth + 7) / 8);
#undef PIXEL_LOOP

    if (sum != 0.0)
    {
        *x_centroid /= sum;
        *y_centroid /= sum;
    }
}
int ccd_image_find_best_centroid(struct ccd_image *image, unsigned char *pixels, float *x_centroid, float *y_centroid, int x_range, int y_range, int *x_max_radius, int *y_max_radius, float sigs)
{
    int           i, j, x, y, x_min, x_max, y_min, y_max, x_radius, y_radius;
    unsigned long pixel, pixel_min, pixel_max;
    float         pixel_ave, pixel_sig;

    x           = (int)*x_centroid;
    y           = (int)*y_centroid;
    x_min       = x - x_range;
    x_max       = x + x_range;
    y_min       = y - y_range;
    y_max       = y + y_range;
    x           = -1;
    y           = -1;
    x_radius    = 0;
    y_radius    = 0;
    pixel_ave   = 0.0;
    pixel_sig   = 0.0;
    if (x_min < 0)             x_min = 0;
    if (x_max > image->width)  x_max = image->width;
    if (y_min < 0)             y_min = 0;
    if (y_max > image->height) y_max = image->height;

#define PIXEL_LOOP(pixel_type)                                                                                                                      \
    for (j = y_min; j < y_max; j++)                                                                                                                 \
        for (i = x_min; i < x_max; i++)                                                                                                             \
            pixel_ave += ((pixel_type *)pixels)[j * image->width + i];                                                                              \
    pixel_ave /= (y_max - y_min) * (x_max - x_min);                                                                                                 \
    for (j = y_min; j < y_max; j++)                                                                                                                 \
        for (i = x_min; i < x_max; i++)                                                                                                             \
            pixel_sig += (pixel_ave - ((pixel_type *)pixels)[j * image->width + i]) * (pixel_ave - ((pixel_type *)pixels)[j * image->width + i]);   \
    pixel_sig = sqrt(pixel_sig / ((y_max - y_min) * (x_max - x_min) - 1));                                                                          \
    pixel_max = pixel_min = pixel_ave + pixel_sig * sigs;                                                                                           \
    for (j = y_min; j < y_max; j++)                                                                                                                 \
    {                                                                                                                                               \
        for (i = x_min; i < x_max; i++)                                                                                                             \
        {                                                                                                                                           \
            if (((pixel_type *)pixels)[j * image->width + i] > pixel_max)                                                                           \
            {                                                                                                                                       \
                pixel = ((pixel_type *)pixels)[j * image->width + i];                                                                               \
                /*                                                                                                                                  \
                 * Local maxima.                                                                                                                    \
                 */                                                                                                                                 \
                if ((pixel >= ((pixel_type *)pixels)[j * image->width + i + 1])                                                                     \
                 && (pixel >= ((pixel_type *)pixels)[j * image->width + i - 1])                                                                     \
                 && (pixel >= ((pixel_type *)pixels)[(j + 1) * image->width + i])                                                                   \
                 && (pixel >= ((pixel_type *)pixels)[(j - 1) * image->width + i]))                                                                  \
                {                                                                                                                                   \
                    /*                                                                                                                              \
                     * Avoid hot pixels.                                                                                                            \
                    if ((pixel_min < ((pixel_type *)pixels)[j * image->width + i + 1])                                                              \
                     && (pixel_min < ((pixel_type *)pixels)[j * image->width + i - 1])                                                              \
                     && (pixel_min < ((pixel_type *)pixels)[(j + 1) * image->width + i])                                                            \
                     && (pixel_min < ((pixel_type *)pixels)[(j - 1) * image->width + i]))                                                           \
                     */                                                                                                                             \
                    {                                                                                                                               \
                        /*                                                                                                                          \
                         * Find radius of highlight.                                                                                                \
                         */                                                                                                                         \
                        for (y_radius = 1; (y_radius <= *y_max_radius)                                                                              \
                                        && ((((pixel_type *)pixels)[(j + y_radius) * image->width + i] > pixel_min)                                 \
                                         || (((pixel_type *)pixels)[(j - y_radius) * image->width + i] > pixel_min)); y_radius++);                  \
                        for (x_radius = 1; (x_radius <= *x_max_radius)                                                                              \
                                        && ((((pixel_type *)pixels)[j * image->width + i + x_radius] > pixel_min)                                   \
                                         || (((pixel_type *)pixels)[j * image->width + i - x_radius] > pixel_min)); x_radius++);                    \
                        /*                                                                                                                          \
                         * If its really big, skip it.  Something like the moon.                                                                    \
                         */                                                                                                                         \
                        if (x_radius < *x_max_radius && y_radius < *y_max_radius)                                                                   \
                        {                                                                                                                           \
                            pixel_max = pixel;                                                                                                      \
                            calc_centroid(image, pixels, i, j, x_radius, y_radius, x_centroid, y_centroid, pixel_min);                              \
                            x = (int)*x_centroid;                                                                                                   \
                            y = (int)*y_centroid;                                                                                                   \
                            calc_centroid(image, pixels, x, y, x_radius, y_radius, x_centroid, y_centroid, pixel_min);                              \
                        }                                                                                                                           \
                    }                                                                                                                               \
                }                                                                                                                                   \
            }                                                                                                                                       \
        }                                                                                                                                           \
    }

    PIXEL_SIZE_CASE((image->depth + 7) / 8);
#undef PIXEL_LOOP

    if  (x >= 0 || y >= 0)
    {
        *x_max_radius = x_radius;
        *y_max_radius = y_radius;
    }
#if 0
#define PIXEL_LOOP(pixel_type)                                                                                                                      \
    x = (int)*x_centroid;                                                                                                                           \
    y = (int)*y_centroid;                                                                                                                           \
    for (j = y - 5; j < y + 5; j++)                                                                                                                 \
    {                                                                                                                                               \
        ((pixel_type *)pixels)[j * image->width + x - 5] = 0xFFFFFFFFUL;                                                                            \
        ((pixel_type *)pixels)[j * image->width + x + 5] = 0xFFFFFFFFUL;                                                                            \
    }                                                                                                                                               \
    for (i = x - 5; i < x + 5; i++)                                                                                                                 \
    {                                                                                                                                               \
        ((pixel_type *)pixels)[(y - 5) * image->width + i] = 0xFFFFFFFFUL;                                                                          \
        ((pixel_type *)pixels)[(y + 5) * image->width + i] = 0xFFFFFFFFUL;                                                                          \
    }

    PIXEL_SIZE_CASE((image->depth + 7) / 8);
#undef PIXEL_LOOP
#endif
    return (x >= 0 || y >= 0);
}
unsigned char *ccd_image_register_centroid(struct ccd_image *image, unsigned char *pixels, float x_centroid, float y_centroid, float *x_prev, float *y_prev, int x_range, int y_range, int x_max_radius, int y_max_radius, float sigs, int frame_num)
{
    int            x, y, x_offset, y_offset, x_min, y_min, x_max, y_max;
    unsigned int   pixel_size, image_pitch, image_size;
    unsigned char *registered_pixels;
    float          x_reg_centroid, y_reg_centroid, x_frac, y_frac, lerp[2][2];

    pixel_size        = ((image->depth + 7) / 8);
    image_pitch       = image->width  * pixel_size;
    image_size        = image->height * image_pitch;
    registered_pixels = malloc(image_size);
    memset(registered_pixels, 0, image_size);
    /*
     * Interpolate where the image might be.
     */
    if (frame_num > 1)
    {
        x_reg_centroid = *x_prev + (*x_prev - x_centroid) / (frame_num - 1);
        y_reg_centroid = *y_prev + (*y_prev - y_centroid) / (frame_num - 1);
    }
    else
    {
        x_reg_centroid = x_centroid;
        y_reg_centroid = y_centroid;
    }
    ccd_image_find_best_centroid(image, pixels, &x_reg_centroid, &y_reg_centroid, x_range, y_range, &x_max_radius, &y_max_radius, sigs);
    /*
     * Resample image with current centroid at original centroid.
     */
    x_offset = floor(x_reg_centroid) - floor(x_centroid);
    y_offset = floor(y_reg_centroid) - floor(y_centroid);
    x_frac   = (x_reg_centroid - floor(x_reg_centroid)) - (x_centroid - floor(x_centroid));
    y_frac   = (y_reg_centroid - floor(y_reg_centroid)) - (y_centroid - floor(y_centroid));
    if (x_frac < 0.0)
    {
        x_offset -= 1;
        x_frac   += 1.0;
    }
    if (y_frac < 0.0)
    {
        y_offset -= 1;
        y_frac   += 1.0;
    }
    if (x_frac >= 1.0 || y_frac >= 1.0)
    {
        fprintf(stderr, "fraction >= 1.0 in frame register!\n");
    }
    lerp[0][0] = (1.0 - x_frac) * (1.0 - y_frac);
    lerp[0][1] = x_frac         * (1.0 - y_frac);
    lerp[1][0] = (1.0 - x_frac) * y_frac;
    lerp[1][1] = x_frac         * y_frac;
    x_min = max(0, x_offset);
    x_max = min(image->width, image->width + x_offset) - 1;
    y_min = max(0, y_offset);
    y_max = min(image->height, image->height + y_offset) - 1;

#define PIXEL_LOOP(pixel_type)                                                                                                                                      \
    for (y = y_min; y < y_max; y++)                                                                                                                                 \
        for (x = x_min; x < x_max; x++)                                                                                                                             \
            ((pixel_type *)registered_pixels)[(y - y_offset) * image->width + x - x_offset] = lerp[0][0] * ((pixel_type *)pixels)[(y + 0) * image->width + x + 0]   \
                                                                                            + lerp[0][1] * ((pixel_type *)pixels)[(y + 0) * image->width + x + 1]   \
                                                                                            + lerp[1][0] * ((pixel_type *)pixels)[(y + 1) * image->width + x + 0]   \
                                                                                            + lerp[1][1] * ((pixel_type *)pixels)[(y + 1) * image->width + x + 1];

    PIXEL_SIZE_CASE(pixel_size);
#undef PIXEL_LOOP

    *x_prev = x_reg_centroid;
    *y_prev = y_reg_centroid;
    return (registered_pixels);
}
int ccd_image_register_frames(struct ccd_image *image, unsigned char **pixels, unsigned char **registered_pixels, float *x_centroid, float *y_centroid, int x_range, int y_range, int x_max_radius, int y_max_radius, float sigs, unsigned int frame_count)
{
    unsigned int pixel_size, image_pitch, image_size, i, j;
    float        x_prev, y_prev;

    /*
     * Register all images to a common star's centroid.
     */
    pixel_size  = ((image->depth + 7) / 8);
    image_pitch = image->width  * pixel_size;
    image_size  = image->height * image_pitch;
    if (*x_centroid > 0.0 && *y_centroid > 0.0)
    {
        x_prev = *x_centroid;
        y_prev = *y_centroid;
        registered_pixels[0] = ccd_image_register_centroid(image, pixels[0], *x_centroid, *y_centroid, &x_prev, &y_prev, x_range, y_range, x_max_radius, y_max_radius, sigs, 0);
    }
    else
    {
        *x_centroid = image->width  / 2;
        *y_centroid = image->height / 2;
        i           = x_max_radius;
        j           = y_max_radius;
        if (!ccd_image_find_best_centroid(image, pixels[0], x_centroid, y_centroid, image->width*3/8, image->height*3/8, &i, &j, sigs))
        {
            *x_centroid = 0.0;
            *y_centroid = 0.0;
            return (0);
        }
        registered_pixels[0] = malloc(image_size);
        memcpy(registered_pixels[0], pixels[0], image_size);
        x_prev = *x_centroid;
        y_prev = *y_centroid;
    }
    for (i = 1; i < frame_count; i++)
      registered_pixels[i] = ccd_image_register_centroid(image, pixels[i], *x_centroid, *y_centroid, &x_prev, &y_prev, x_range, y_range, x_max_radius, y_max_radius, sigs, i);
    return (1);
}
/*
 * Image combining.
 * The following combine a number of frames into a single image.
 */
static void image_mean_frames(struct ccd_image *image, unsigned char **pixels, unsigned int frame_count)
{
    unsigned int    i, x, y, pixel_size, pixel_offset;
    float           pixel, pixel_scale, pixel_max;

    pixel_size  = (image->depth + 7) / 8;
    pixel_scale = 1.0/frame_count;
    pixel_max   = (1 << image->depth) - 1/*image->datamax*/;

#define PIXEL_LOOP(pixel_type)                                                      \
    for (y = 0; y < image->height; y++)                                             \
    {                                                                               \
        for (x = 0; x < image->width; x++)                                          \
        {                                                                           \
            pixel_offset = y * image->width + x;                                    \
            pixel        = 0.0;                                                     \
            for (i = 0; i < frame_count; i++)                                       \
                pixel += ((pixel_type *)pixels[i])[pixel_offset];                   \
            pixel *= pixel_scale;                                                   \
            ((pixel_type *)image->pixels)[pixel_offset] = min(pixel, pixel_max);    \
        }                                                                           \
    }

    PIXEL_SIZE_CASE(pixel_size);
#undef PIXEL_LOOP

}
static void image_rank_frames(struct ccd_image *image, unsigned char **pixels, unsigned int frame_count, unsigned int rank)
{
    unsigned int    i, j, k, x, y, pixel_size, pixel_offset, image_pitch;
    unsigned long  *sorted_pixels, *median_pixel;

    pixel_size    = ((image->depth + 7) / 8);
    image_pitch   = image->width  * pixel_size;
    sorted_pixels = malloc(sizeof(unsigned long) * (frame_count + 1));
    median_pixel = &sorted_pixels[rank];

#define PIXEL_LOOP(pixel_type)                                                      \
    for (y = 0; y < image->height; y++)                                             \
    {                                                                               \
        for (x = 0; x < image->width; x++)                                          \
        {                                                                           \
            pixel_offset = y * image->width + x;                                    \
            for (i = 0; i < frame_count; i++)                                       \
                sorted_pixels[i] = 0xFFFFFFFF;                                      \
            for (i = 0; i < frame_count; i++)                                       \
                for (j = 0; j <= i; j++)                                            \
                    if (((pixel_type *)pixels[i])[pixel_offset] < sorted_pixels[j]) \
                    {                                                               \
                        for (k = i; k > j; k--)                                     \
                            sorted_pixels[k] = sorted_pixels[k - 1];                \
                        sorted_pixels[j] = ((pixel_type *)pixels[i])[pixel_offset]; \
                        break;                                                      \
                    }                                                               \
            ((pixel_type *)image->pixels)[pixel_offset] = *median_pixel;            \
        }                                                                           \
    }

    PIXEL_SIZE_CASE(pixel_size);
#undef PIXEL_LOOP

    free (sorted_pixels);
}
static void image_sum_frames(struct ccd_image *image, unsigned char **pixels, unsigned int frame_count)
{
    unsigned int    i, x, y, pixel_size, pixel_offset;
    float           pixel, pixel_max;

    pixel_size = ((image->depth + 7) / 8);
    pixel_max  = (1 << image->depth) - 1/*image->datamax*/;

#define PIXEL_LOOP(pixel_type)                                                      \
    for (y = 0; y < image->height; y++)                                             \
        for (x = 0; x < image->width; x++)                                          \
        {                                                                           \
            pixel_offset = y * image->width + x;                                    \
            pixel        = 0.0;                                                     \
            for (i = 0; i < frame_count; i++)                                       \
                pixel += ((pixel_type *)pixels[i])[pixel_offset];                   \
            ((pixel_type *)image->pixels)[pixel_offset] = min(pixel, pixel_max);    \
        }

    PIXEL_SIZE_CASE(pixel_size);
#undef PIXEL_LOOP

}
static void image_diff_frames(struct ccd_image *image, unsigned char **pixels, unsigned int frame_count)
{
    unsigned int    i, x, y, pixel_size, pixel_offset;
    float           pixel, pixel_max;

    pixel_size = ((image->depth + 7) / 8);
    pixel_max  = (1 << image->depth) - 1/*image->datamax*/;

#define PIXEL_LOOP(pixel_type)                                                      \
    for (y = 0; y < image->height; y++)                                             \
        for (x = 0; x < image->width; x++)                                          \
        {                                                                           \
            pixel_offset = y * image->width + x;                                    \
            pixel        = 0.0;                                                     \
            for (i = 0; i < frame_count; i++)                                       \
                pixel = abs(pixel - ((pixel_type *)pixels[i])[pixel_offset]);       \
            ((pixel_type *)image->pixels)[pixel_offset] = pixel;                    \
        }

    PIXEL_SIZE_CASE(pixel_size);
#undef PIXEL_LOOP

}
static void image_interleave_frames(struct ccd_image *image, unsigned char **pixels)
{
    unsigned int    x, y;

    image->height       *= 2;
    image->pixel_height /= 2.0;

#define PIXEL_LOOP(pixel_type)                                                                                              \
    for (y = 0; y < image->height; y++)                                                                                     \
        for (x = 0; x < image->width; x++)                                                                                  \
            ((pixel_type *)image->pixels)[y * image->width + x] = ((pixel_type *)pixels[y & 1])[(y / 2) * image->width + x];\

    PIXEL_SIZE_CASE((image->depth + 7) / 8);
#undef PIXEL_LOOP

}
/*
 * Combine images into one.
 */
void ccd_image_combine_frames(struct ccd_image *image, unsigned char **pixels, unsigned int frame_count, int op)
{
    if (frame_count > 0)
    {
        image->pixels = malloc(image->height * (op == CCD_IMAGE_COMBINE_INTERLEAVE ? 2 : 1) * image->width * ((image->depth + 7) / 8));
        switch (op)
        {
            case CCD_IMAGE_COMBINE_MEDIAN:
                image_rank_frames(image, pixels, frame_count, frame_count/2);
                break;
            case CCD_IMAGE_COMBINE_MIN:
                image_rank_frames(image, pixels, frame_count, 0);
                break;
            case CCD_IMAGE_COMBINE_MAX:
                image_rank_frames(image, pixels, frame_count, frame_count - 1);
                break;
            case CCD_IMAGE_COMBINE_MEAN:
                image_mean_frames(image, pixels, frame_count);
                break;
            case CCD_IMAGE_COMBINE_DIFF:
                image_diff_frames(image, pixels, frame_count);
                break;
            case CCD_IMAGE_COMBINE_SUM:
                image_sum_frames(image, pixels, frame_count);
                break;
            case CCD_IMAGE_COMBINE_INTERLEAVE:
                if (frame_count  >= 2)
                    image_interleave_frames(image, pixels);
                break;
        }
        image->pixmin = image->pixmax = 0;
        ccd_image_histogram(image);
    }
}
static int set_color_matrix_filter(unsigned int filter[4][2], unsigned int color, unsigned int mask)
{
    if (((mask & 0x0F) == ((mask >> 4) & 0x0F)
     &&  (mask & 0x0F) == ((mask >> 8) & 0x0F)))
        return (0);
    filter[0][0] = (mask & 0x0100 ? 0x0F00 : 0x0000)  // Red
                 | (mask & 0x0010 ? 0x00F0 : 0x0000)  // Green
                 | (mask & 0x0001 ? 0x000F : 0x0000); // Blue
    filter[0][1] = (mask & 0x0200 ? 0x0F00 : 0x0000)  // Red
                 | (mask & 0x0020 ? 0x00F0 : 0x0000)  // Green
                 | (mask & 0x0002 ? 0x000F : 0x0000); // Blue
    filter[1][0] = (mask & 0x0400 ? 0x0F00 : 0x0000)  // Red
                 | (mask & 0x0040 ? 0x00F0 : 0x0000)  // Green
                 | (mask & 0x0004 ? 0x000F : 0x0000); // Blue
    filter[1][1] = (mask & 0x0800 ? 0x0F00 : 0x0000)  // Red
                 | (mask & 0x0080 ? 0x00F0 : 0x0000)  // Green
                 | (mask & 0x0008 ? 0x000F : 0x0000); // Blue
    if (color & CCD_COLOR_MATRIX_ALT_EVEN)
    {
        filter[2][1] = (mask & 0x0100 ? 0x0F00 : 0x0000)  // Red
                     | (mask & 0x0010 ? 0x00F0 : 0x0000)  // Green
                     | (mask & 0x0001 ? 0x000F : 0x0000); // Blue
        filter[2][0] = (mask & 0x0200 ? 0x0F00 : 0x0000)  // Red
                     | (mask & 0x0020 ? 0x00F0 : 0x0000)  // Green
                     | (mask & 0x0002 ? 0x000F : 0x0000); // Blue
    }
    else
    {
        filter[2][0] = (mask & 0x0100 ? 0x0F00 : 0x0000)  // Red
                     | (mask & 0x0010 ? 0x00F0 : 0x0000)  // Green
                     | (mask & 0x0001 ? 0x000F : 0x0000); // Blue
        filter[2][1] = (mask & 0x0200 ? 0x0F00 : 0x0000)  // Red
                     | (mask & 0x0020 ? 0x00F0 : 0x0000)  // Green
                     | (mask & 0x0002 ? 0x000F : 0x0000); // Blue
    }
    if (color & CCD_COLOR_MATRIX_ALT_ODD)
    {
        filter[3][1] = (mask & 0x0400 ? 0x0F00 : 0x0000)  // Red
                     | (mask & 0x0040 ? 0x00F0 : 0x0000)  // Green
                     | (mask & 0x0004 ? 0x000F : 0x0000); // Blue
        filter[3][0] = (mask & 0x0800 ? 0x0F00 : 0x0000)  // Red
                     | (mask & 0x0080 ? 0x00F0 : 0x0000)  // Green
                     | (mask & 0x0008 ? 0x000F : 0x0000); // Blue
    }
    else
    {
        filter[3][0] = (mask & 0x0400 ? 0x0F00 : 0x0000)  // Red
                     | (mask & 0x0040 ? 0x00F0 : 0x0000)  // Green
                     | (mask & 0x0004 ? 0x000F : 0x0000); // Blue
        filter[3][1] = (mask & 0x0800 ? 0x0F00 : 0x0000)  // Red
                     | (mask & 0x0080 ? 0x00F0 : 0x0000)  // Green
                     | (mask & 0x0008 ? 0x000F : 0x0000); // Blue
    }
    return (1);
}
/*
 * Split image into color components.
 */
int ccd_image_split_frames(struct ccd_image *image, unsigned char *pixels[5], unsigned int colors[5], unsigned int lrgb_split)
{
    unsigned int   pixel_size, pixel_offset, image_pitch, image_size, color_pitch, color_size, filter[4][2];
    int            num_colors, x_matrix, y_matrix, x, y, i, j, k, neighbor;
    float          pixel_max, sum, kernel[3][3];

    pixel_size  = ((image->depth + 7) / 8);
    image_pitch = image->width  * pixel_size;
    image_size  = image->height * image_pitch;
    color_pitch = image->width/2  * pixel_size;
    color_size  = image->height/2 * color_pitch;
    pixel_max   = image->datamax;
    num_colors  = 0;
    pixels[0]   = NULL;
    pixels[1]   = NULL;
    pixels[2]   = NULL;
    pixels[3]   = NULL;
    colors[0]   = 0;
    colors[1]   = 0;
    colors[2]   = 0;
    colors[3]   = 0;
    if ((image->color & 0xC000) == CCD_COLOR_MATRIX_2X2)
    {
        struct ccd_image scaled_image;
        memcpy(&scaled_image, image, sizeof(struct ccd_image));
        scaled_image.width  /= 2;
        scaled_image.height /= 2;
        /*
         * Split image based on 2X2 color matrix.
         */
        if (!set_color_matrix_filter(filter, image->color, image->color & image->filter))
            return (0);
        for (y_matrix = 0; y_matrix < 2; y_matrix++)
            for (x_matrix = 0; x_matrix < 2; x_matrix++)
                /*
                 * Check for unique color.
                 */
                if (filter[y_matrix][x_matrix])
                {
                    for (i = 0; i < num_colors && colors[i] != filter[y_matrix][x_matrix]; i++);
                    if (i >= num_colors)
                    {
                        pixels[num_colors] = malloc(color_size);
                        colors[num_colors] = filter[y_matrix][x_matrix];
                        num_colors++;
                    }
                }

#define PIXEL_LOOP(pixel_type)                                                                                                                 \
        for (y = 0; y < image->height; y += 2)                                                                                 \
            for (x = 0; x < image->width; x += 2)                                                                              \
                for (i = 0; i < num_colors; i++)                                                                               \
                {                                                                                                              \
                    sum = neighbor = 0;                                                                                        \
                    if (filter[(y&2)+0][0] == colors[i])                                                                       \
                        {sum = ((pixel_type *)image->pixels)[y * image->width + x]; neighbor++;}                               \
                    if (filter[(y&2)+1][0] == colors[i])                                                                       \
                        {sum = ((pixel_type *)image->pixels)[(y+1) * image->width + x]; neighbor++;}                           \
                    if (filter[(y&2)+0][1] == colors[i])                                                                       \
                        {sum = ((pixel_type *)image->pixels)[y * image->width + x+1]; neighbor++;}                             \
                    if (filter[(y&2)+1][1] == colors[i])                                                                       \
                        {sum = ((pixel_type *)image->pixels)[(y+1) * image->width + x+1]; neighbor++;}                         \
                    ((pixel_type *)pixels[i])[(y/2) * (image->width/2) + x/2] = neighbor ? sum / neighbor : 0;                 \
                }

        PIXEL_SIZE_CASE(pixel_size);
#undef PIXEL_LOOP

        /*
         * Blur the color info slightly to remove matrix artifacts.
         */
        kernel[0][0] =
        kernel[0][2] =
        kernel[2][0] =
        kernel[2][2] = 1.0;
        kernel[0][1] =
        kernel[1][0] =
        kernel[1][2] =
        kernel[2][1] = 2.0;
        kernel[1][1] = 8.0;
        for (i = 0; i < num_colors; i++)
        {
            scaled_image.pixels = pixels[i];
            pixels[i]  = ccd_image_convolve(&scaled_image, NULL, 1, 1, (float *)kernel);
            free(scaled_image.pixels);
        }
        /*
         * Convert CMY to RGB.
         */
        if (lrgb_split)
        {
//            float cyan_ave = 0.0, yellow_ave = 0.0, magenta_ave = 0.0;
            int   cyan     = -1,  yellow     = -1,  magenta     = -1;
            for (i = 0; i < num_colors; i++)
                switch (colors[i])
                {
                    case 0xFF0: // Yellow
//                        scaled_image.pixels = pixels[i];
//                        scaled_image.pixmin = scaled_image.pixmax = 0;
//                        ccd_image_histogram(&scaled_image);
//                        yellow_ave = scaled_image.pixave;
                        yellow     = i;
                        break;
                    case 0x0FF: // Cyan
//                        scaled_image.pixels = pixels[i];
//                        scaled_image.pixmin = scaled_image.pixmax = 0;
//                        ccd_image_histogram(&scaled_image);
//                        cyan_ave = scaled_image.pixave;
                        cyan     = i;
                        break;
                    case 0xF0F: // Magenta
//                        scaled_image.pixels = pixels[i];
//                        scaled_image.pixmin = scaled_image.pixmax = 0;
//                        ccd_image_histogram(&scaled_image);
//                        magenta_ave = scaled_image.pixave;
                        magenta     = i;
                        break;
                }
            if (cyan >= 0 && yellow >= 0 && magenta >= 0)
            {
                unsigned char *red, *green, *blue, *color_pixels, *add1_pixels, *add2_pixels, *sub_pixels;
//                float          add1_scale, add2_scale, sub_scale, ave_scale;

#define PIXEL_LOOP(pixel_type)                                                                                  \
                for (y = 0; y < image->height/2; y++)                                                           \
                    for (x = 0; x < image->width/2; x++)                                                        \
                    {                                                                                           \
                        sum = /*add1_scale * */(float)((pixel_type *)add1_pixels)[y * (image->width/2) + x]         \
                            + /*add2_scale * */(float)((pixel_type *)add2_pixels)[y * (image->width/2) + x]         \
                            - /*sub_scale  * */(float)((pixel_type *)sub_pixels) [y * (image->width/2) + x];        \
                        ((pixel_type *)color_pixels)[y * (image->width/2) + x] = max(0, min(pixel_max, sum));   \
                    }

//                ave_scale    = (cyan_ave + yellow_ave + magenta_ave) / 3.0;
                color_pixels = red = malloc(color_size);
//                add1_scale   = ave_scale / yellow_ave;
//                add2_scale   = ave_scale / magenta_ave;
//                sub_scale    = ave_scale / cyan_ave;
                add1_pixels  = pixels[yellow];
                add2_pixels  = pixels[magenta];
                sub_pixels   = pixels[cyan];
                PIXEL_SIZE_CASE(pixel_size);
                color_pixels = green = malloc(color_size);
//                add1_scale   = ave_scale / yellow_ave;
//                add2_scale   = ave_scale / cyan_ave;
//                sub_scale    = ave_scale / magenta_ave;
                add1_pixels  = pixels[yellow];
                add2_pixels  = pixels[cyan];
                sub_pixels   = pixels[magenta];
                PIXEL_SIZE_CASE(pixel_size);
                color_pixels = blue = malloc(color_size);
//                add1_scale   = ave_scale / magenta_ave;
//                add2_scale   = ave_scale / cyan_ave;
//                sub_scale    = ave_scale / yellow_ave;
                add1_pixels  = pixels[magenta];
                add2_pixels  = pixels[cyan];
                sub_pixels   = pixels[yellow];
                PIXEL_SIZE_CASE(pixel_size);
#undef PIXEL_LOOP

                free(pixels[cyan]);
                free(pixels[magenta]);
                free(pixels[yellow]);
                pixels[cyan]    = red;
                colors[cyan]    = 0xF00;
                pixels[magenta] = green;
                colors[magenta] = 0x0F0;
                pixels[yellow]  = blue;
                colors[yellow]  = 0x00F;
            }
        }
        /*
         * Combine duplicates.
         */
        for (i = 0; i < num_colors; i++)
            if (pixels[i])
                for (j = i + 1; j < num_colors; j++)
                    if (colors[i] == colors[j])
                    {

                        /*
                         * Ave combine the same colors.
                         */

#define PIXEL_LOOP(pixel_type)                                                                                              \
                        for (y = 0; y < image->height/2; y++)                                                               \
                        {                                                                                                   \
                            for (x = 0; x < image->width/2; x++)                                                            \
                            {                                                                                               \
                                pixel_offset = y * (image->width/2) + x;                                                    \
                                ((pixel_type *)pixels[i])[pixel_offset] = ((pixel_type *)pixels[j])[pixel_offset] / 2       \
                                                                        + ((pixel_type *)pixels[i])[pixel_offset] / 2;      \
                            }                                                                                               \
                        }
                        PIXEL_SIZE_CASE(pixel_size);
#undef PIXEL_LOOP

                        free(pixels[j]);
                        for (k = j + 1; k < num_colors; k++)
                        {
                            pixels[k - 1] = pixels[k];
                            colors[k - 1] = colors[k];
                        }
                        pixels[num_colors - 1] = NULL;
                        colors[num_colors - 1] = 0;
                        num_colors--;
                    }
        /*
         * Scale color images 2X back to original image size.
         */
        for (i = 0; i < num_colors; i++)
            if (pixels[i])
            {
                scaled_image.width  = image->width  / 2;
                scaled_image.height = image->height / 2;
                scaled_image.pixels = pixels[i];
                ccd_image_scale(&scaled_image, image->width, image->height);
                pixels[i] = scaled_image.pixels;
            }
        if (lrgb_split)
        {
            struct ccd_image lum;
#if 0
            float filter_weight[4][2], color_weight[7];

            for (i = 0; i < num_colors; i++)
            {
                switch (colors[i])
                {
                    case 0xFFF:
                        color_weight[i] = 1.0;
                        break;
                    case 0x00F:
                        color_weight[i] = 0.333;
                        break;
                    case 0x0F0:
                        color_weight[i] = 0.333;
                        break;
                    case 0xF00:
                        color_weight[i] = 0.333;
                        break;
                    case 0x0FF:
                        color_weight[i] = 0.667;
                        break;
                    case 0xFF0:
                        color_weight[i] = 0.667;
                        break;
                    case 0xF0F:
                        color_weight[i] = 0.667;
                        break;
                    default:
                        color_weight[i] = 0.0;
                        break;
                }
            }
            for (y = 0; y < 4; y++)
                for (x = 0; x < 2; x++)
                    switch (filter[y][x])
                    {
                        case 0xFFF:
                            filter_weight[y][x] = 1.0;
                            break;
                        case 0x00F:
                            filter_weight[y][x] = 0.333;
                            break;
                        case 0x0F0:
                            filter_weight[y][x] = 0.333;
                            break;
                        case 0xF00:
                            filter_weight[y][x] = 0.333;
                            break;
                        case 0x0FF:
                            filter_weight[y][x] = 0.667;
                            break;
                        case 0xFF0:
                            filter_weight[y][x] = 0.667;
                            break;
                        case 0xF0F:
                            filter_weight[y][x] = 0.667;
                            break;
                        default:
                            filter_weight[y][x] = 0.0;
                            break;
                    }
            /*
             * Create luminance frame.
             */
            pixels[num_colors] = malloc(image_size);
            colors[num_colors] = CCD_COLOR_MONOCHROME;

#define PIXEL_LOOP(pixel_type)                                                                                                                                  \
            for (y = 0; y < image->height; y++)                                                                                                                 \
                for (x = 0; x < image->width; x++)                                                                                                              \
                {                                                                                                                                               \
                    pixel_offset = y * image->width + x;                                                                                                        \
                    for (i = 0; i < num_colors; i++)                                                                                                            \
                        if ((filter[y&3][x&1] & colors[i]) == 0)                                                                                                \
                            ((pixel_type *)pixels[num_colors])[pixel_offset] = min(((pixel_type *)image->pixels)[pixel_offset] * filter_weight[y&3][x&1] + ((pixel_type *)pixels[i])[pixel_offset] * color_weight[i], pixel_max);\
                }

            PIXEL_SIZE_CASE(pixel_size);
#else
            /*
             * Create luminance frame.
             */
            pixels[num_colors] = malloc(image_size);
            colors[num_colors] = CCD_COLOR_MONOCHROME;

#define PIXEL_LOOP(pixel_type)                                                                                                                                  \
            for (y = 0; y < image->height; y++)                                                                                                                 \
                for (x = 0; x < image->width; x++)                                                                                                              \
                {                                                                                                                                               \
                    pixel_offset = y * image->width + x;                                                                                                        \
                    for (i = 0; i < num_colors; i++)                                                                                                            \
                        if ((filter[y&3][x&1] & colors[i]) == 0)                                                                                                \
                            ((pixel_type *)pixels[num_colors])[pixel_offset] = min(((pixel_type *)image->pixels)[pixel_offset] + ((pixel_type *)pixels[i])[pixel_offset], pixel_max);\
                }

            PIXEL_SIZE_CASE(pixel_size);
#endif
#undef PIXEL_LOOP

            /*
             * Run luminance frame through a slight blur and slight enhance to smooth out the color matrix artifacts.
             */
            memcpy(&lum, image, sizeof(struct ccd_image));
            lum.pixels   = pixels[num_colors];
            kernel[0][0] =
            kernel[0][2] =
            kernel[2][0] =
            kernel[2][2] = 1.0;
            kernel[0][1] =
            kernel[1][0] =
            kernel[1][2] =
            kernel[2][1] = 2.0;
            kernel[1][1] = 4.0;
            pixels[num_colors] = ccd_image_convolve(&lum, NULL, 1, 1, (float *)kernel);
            free(lum.pixels);
            lum.pixels   = pixels[num_colors];
            kernel[0][0] =
            kernel[0][2] =
            kernel[2][0] =
            kernel[2][2] = 0.0;
            kernel[0][1] =
            kernel[1][0] =
            kernel[1][2] =
            kernel[2][1] = -1.0;
            kernel[1][1] = 5.0;
            pixels[num_colors] = ccd_image_convolve(&lum, NULL, 1, 1, (float *)kernel);
            free(lum.pixels);
            num_colors++;
        }
    }
    return (num_colors);
}
/*
 * Constant value image ops.
 */
unsigned long ccd_image_average(struct ccd_image *image)
{
    int   x, y;
    float pixel_ave;

    pixel_ave = 0.0;

#define PIXEL_LOOP(pixel_type)                                                                                                                  \
    for (y = 0; y < image->height; y++)                                                                                                         \
        for (x = 0; x < image->width; x++)                                                                                                      \
            pixel_ave += ((pixel_type *)image->pixels)[y * image->width + x];

    PIXEL_SIZE_CASE((image->depth + 7) / 8);
#undef PIXEL_LOOP

    pixel_ave /= image->height * image->width;
    return ((unsigned long)pixel_ave);
}
void ccd_image_add(struct ccd_image *image, unsigned long offset)
{
    unsigned int  pixel_size, pixel_max, x, y;

    pixel_size  = ((image->depth + 7) / 8);
    pixel_max   = image->datamax;

#define PIXEL_LOOP(pixel_type)                                                                                                                  \
    for (y = 0; y < image->height; y++)                                                                                                         \
        for (x = 0; x < image->width; x++)                                                                                                      \
            ((pixel_type *)image->pixels)[y * image->width + x] = (pixel_type)(((pixel_type *)image->pixels)[y * image->width + x] + offset) < ((pixel_type *)image->pixels)[y * image->width + x] ? pixel_max : ((pixel_type *)image->pixels)[y * image->width + x] + offset;

    PIXEL_SIZE_CASE((image->depth + 7) / 8);
#undef PIXEL_LOOP

    image->pixmax = image->pixmin = 0;
    ccd_image_histogram(image);
}
void ccd_image_sub(struct ccd_image *image, unsigned long offset)
{
    unsigned int x, y;


#define PIXEL_LOOP(pixel_type)                                                                                                                  \
    for (y = 0; y < image->height; y++)                                                                                                         \
        for (x = 0; x < image->width; x++)                                                                                                      \
            ((pixel_type *)image->pixels)[y * image->width + x] = (pixel_type)(((pixel_type *)image->pixels)[y * image->width + x] - offset) > ((pixel_type *)image->pixels)[y * image->width + x] ? 0 : ((pixel_type *)image->pixels)[y * image->width + x] - offset;

    PIXEL_SIZE_CASE((image->depth + 7) / 8);
#undef PIXEL_LOOP

    image->pixmax = image->pixmin = 0;
    ccd_image_histogram(image);
}
void ccd_image_mul(struct ccd_image *image, unsigned long factor)
{
    unsigned int x, y;

    if (factor == 0)
        factor = 1;

#define PIXEL_LOOP(pixel_type)                                                                                                                  \
    for (y = 0; y < image->height; y++)                                                                                                         \
        for (x = 0; x < image->width; x++)                                                                                                      \
            ((pixel_type *)image->pixels)[y * image->width + x] *= factor;

    PIXEL_SIZE_CASE((image->depth + 7) / 8);
#undef PIXEL_LOOP

    image->pixmax = image->pixmin = 0;
    ccd_image_histogram(image);
}
void ccd_image_div(struct ccd_image *image, unsigned long factor)
{
    unsigned int x, y;

    if (factor == 0)
        factor = 1;

#define PIXEL_LOOP(pixel_type)                                                                                                                  \
    for (y = 0; y < image->height; y++)                                                                                                         \
        for (x = 0; x < image->width; x++)                                                                                                      \
            ((pixel_type *)image->pixels)[y * image->width + x] /= factor;

    PIXEL_SIZE_CASE((image->depth + 7) / 8);
#undef PIXEL_LOOP

    image->pixmax = image->pixmin = 0;
    ccd_image_histogram(image);
}
void ccd_image_fmul(struct ccd_image *image, float factor)
{
    unsigned int  pixel_size, pixel_max, x, y;

    if (factor < 0.0)
        return;
    pixel_size  = ((image->depth + 7) / 8);
    pixel_max   = image->datamax;

#define PIXEL_LOOP(pixel_type)                                                                                                                  \
    for (y = 0; y < image->height; y++)                                                                                                         \
        for (x = 0; x < image->width; x++)                                                                                                      \
            ((pixel_type *)image->pixels)[y * image->width + x] = ((pixel_type *)image->pixels)[y * image->width + x] * factor > pixel_max      \
                                                                ? pixel_max                                                                     \
                                                                : ((pixel_type *)image->pixels)[y * image->width + x] * factor;

    PIXEL_SIZE_CASE(pixel_size);
#undef PIXEL_LOOP

    image->pixmax = image->pixmin = 0;
    ccd_image_histogram(image);
}
/*
 * Calibrate raw image.
 */
void ccd_image_calibrate(struct ccd_image *raw, struct ccd_image *bias, struct ccd_image *dark, struct ccd_image *flat)
{
    unsigned int  pixel_size, pixel_offset, x, y;
    float         pixel, pixel_max, dark_scale;
    static float  flat_ave;
    static struct ccd_image *prev_flat = NULL;

    if (!bias && !dark && !flat)
        return;
    pixel_size = ((raw->depth + 7) / 8);
    pixel_max  = raw->datamax;
    /*
     * Calc dark scale factor based on exposure ratio.
     */
    dark_scale = (dark && dark->exposure) ? ((float)raw->exposure / (float)dark->exposure) : 1.0;
    /*
     * Find flat average. Do a little optimization to avoid doing this for the same flat over and over.
     */
    if (flat && prev_flat != flat)
    {
        unsigned int quartery = flat->height / 4;
        unsigned int quarterx = flat->width  / 4;
        flat_ave = 0.0;

#define PIXEL_LOOP(pixel_type)                                                  \
        for (y = quartery; y < flat->height - quartery; y++)                    \
            for (x = quarterx; x < flat->width - quarterx; x++)                 \
                flat_ave += ((pixel_type *)flat->pixels)[y * flat->width + x];

        PIXEL_SIZE_CASE(pixel_size);
#undef PIXEL_LOOP

        flat_ave /= (flat->height - quartery * 2) * (flat->width - quarterx * 2);
        prev_flat = flat;
    }
    /*
     * Apply calibration to raw image.
     */

#define PIXEL_LOOP(pixel_type)                                                                                                                                          \
    for (y = 0; y < raw->height; y++)                                                                                                                                   \
    {                                                                                                                                                                   \
        for (x = 0; x < raw->width; x++)                                                                                                                                \
        {                                                                                                                                                               \
            pixel_offset = y * raw->width + x;                                                                                                                          \
            pixel = ((pixel_type *)raw->pixels)[pixel_offset];                                                                                                          \
            if (bias) pixel  = ((pixel_type *)bias->pixels)[pixel_offset]              > pixel ? 0.0 : pixel - ((pixel_type *)bias->pixels)[pixel_offset];              \
            if (dark) pixel  = ((pixel_type *)dark->pixels)[pixel_offset] * dark_scale > pixel ? 0.0 : pixel - ((pixel_type *)dark->pixels)[pixel_offset] * dark_scale; \
            if (flat) pixel *= flat_ave / (((pixel_type *)flat->pixels)[pixel_offset] ? ((pixel_type *)flat->pixels)[pixel_offset] : 1.0);                              \
            ((pixel_type *)raw->pixels)[pixel_offset] = pixel > pixel_max ? ~0UL : (pixel_type)pixel;                                                                   \
        }                                                                                                                                                               \
    }

    PIXEL_SIZE_CASE(pixel_size);
#undef PIXEL_LOOP

    raw->pixmin = raw->pixmax = 0;
    ccd_image_histogram(raw);
}

