#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <malloc.h>
/*
 * Reverse scanline order.
 */
void flip_image_vert(unsigned char *image, int width, int height)
{
    int   i, j, tmp;
    unsigned char *top    = image;
    unsigned char *bottom = image + (height - 1) * width;
    
    for (j = 0; j < height / 2; j++)
    {
        for (i = 0; i < width; i++)
        {
            tmp       = top[i];
            top[i]    = bottom[i];
            bottom[i] = tmp;
        }
        top    += width;
        bottom -= width;
    }
}
/*
 * Save image to FITS file.
 */
void load_fits(char *filename, unsigned char **image, int *width, int *height)
{
#define FITS_HEADER_SIZE    2880

    char header[FITS_HEADER_SIZE+1];
    int depthbytes, image_size, fd, i;
    
    if ((fd = open(filename, O_RDONLY, 0)) > 0)
    {
        read(fd, header, FITS_HEADER_SIZE);
        for (i = 79; i < FITS_HEADER_SIZE; i += 80)
            header[i] = '\0';
        sscanf(&header[80*1+27], "%d", &depthbytes);
        sscanf(&header[80*3+27], "%d", width);
        sscanf(&header[80*4+27], "%d", height);
        depthbytes = (depthbytes + 7) / 8;
        printf("Image depth, width, height = %d %d %d\n", depthbytes, *width, *height);
        image_size = *width * *height;
        *image = malloc(image_size * depthbytes);
        read(fd, *image, image_size * depthbytes);
        close(fd);
        if (depthbytes == 2)
        {         
            unsigned short *short_image = (unsigned short *)*image;
            *image = malloc(image_size);
            for (i = 0; i < image_size; i++)
                (*image)[i] = (short_image[i] ^ 0x8000) >> 8;    
            free(short_image);
        }
        flip_image_vert(*image, *width, *height);
    }
}
void save_ppm(char *filename, unsigned char *image, int width, int height)
{
    char header[64];
    int fd;

    sprintf(header, "P6 %d %d 255\n", width, height);
    if ((fd = open(filename, O_RDWR|O_CREAT, 0666)) > 0)
    {
        write(fd, header, strlen(header));
        write(fd, image, width * height * 3);
    }
    close(fd);
}
/*
 * Convert greyscale FITS to RGB PPM.
 */
int main(int argc, char **argv)
{
    unsigned char *raw_image, *dark_image, *rgb_image;
    int width, height;
    int dark_width, dark_height;
    int x, y;
    int r, g, b;

    if (argc < 4)
        return (-1);
    load_fits(argv[1], &raw_image, &width, &height);
    load_fits(argv[2], &dark_image, &dark_width, &dark_height);
    for (y = 0; y < (height-1); y++)
        for (x = 0; x < (width-1); x++)
        {
            if (raw_image[y * width + x] < dark_image[y * dark_width + x])
                raw_image[y * width + x] = 0;
            else
                raw_image[y * width + x] -= dark_image[y * dark_width + x];
        }
    rgb_image = malloc(width * height * 3);
    for (y = 1; y < (height-1); y++)
        for (x = 1; x < (width-1); x++)
        {
            if (!(x & 1) && !(y & 1))
            {
                r =  raw_image[ y    * width + x];
                g = (raw_image[(y-1) * width + x]   + raw_image[(y+1) * width + x]
                  +  raw_image[ y    * width + x-1] + raw_image[y     * width + x+1]) / 4;
                b = (raw_image[(y-1) * width + x-1] + raw_image[(y-1) * width + x+1]
                  +  raw_image[(y+1) * width + x-1] + raw_image[(y+1) * width + x+1]) / 4;
            }
            else if (x & 1 && !(y & 1))
            {
                r = (raw_image[ y    * width + x-1] + raw_image[y     * width + x+1]) / 2;
                g =  raw_image[ y    * width + x];
                b = (raw_image[(y-1) * width + x]   + raw_image[(y+1) * width + x])   / 2;
            }
            else if (!(x & 1) && (y & 1))
            {
                r = (raw_image[(y-1) * width + x]   + raw_image[(y+1) * width + x])   / 2;
                g =  raw_image[ y    * width + x];
                b = (raw_image[ y    * width + x-1] + raw_image[y     * width + x+1]) / 2;
            }
            else /* if ((x & 1) && (y & 1) */
            {
                r = (raw_image[(y-1) * width + x-1] + raw_image[(y-1) * width + x+1]
                  +  raw_image[(y+1) * width + x-1] + raw_image[(y+1) * width + x+1]) / 4;
                g = (raw_image[(y-1) * width + x]   + raw_image[(y+1) * width + x]
                  +  raw_image[ y    * width + x-1] + raw_image[y     * width + x+1]) / 4;
                b =  raw_image[ y    * width + x];
            }
            rgb_image[y * width * 3 + x * 3 + 0] = r;
            rgb_image[y * width * 3 + x * 3 + 1] = g;
            rgb_image[y * width * 3 + x * 3 + 2] = b;
        }
    save_ppm(argv[3], rgb_image, width, height);
    return (0);
}
