/* GCCD - Gnome CCD Camera Controller
 * Copyright (C) 2001, 2002 David Schmenk
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
#include <termios.h>
#include <unistd.h>
#include <math.h>
#include <error.h>
#include <errno.h>
#include <sys/time.h>
#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "config.h"
#ifndef min
#define min(a,b)    (((a)<(b))?(a):(b))
#endif
#ifndef max
#define max(a,b)    (((a)>(b))?(a):(b))
#endif
/*
 * View image palettes.
 */
#define LINEAR_GREY     0
#define GAMMALOG1_GREY  1
#define GAMMALOG2_GREY  2
#define GAMMALOG3_GREY  3
#define GAMMALOG4_GREY  4
#define LINEAR_RED      5
#define GAMMALOG1_RED   6
#define GAMMALOG2_RED   7
#define GAMMALOG3_RED   8
#define GAMMALOG4_RED   9
#define INVERSION       10
#define FALSE_COLOR1    11
#define FALSE_COLOR2    12
#define FALSE_COLOR3    13
#define MINMAX_HIGHLITE 14
#define LEVEL_4         15
#define NUM_PALETTES    16
/*
 * Color mask to name.
 */
#define COLOR_MASK_TO_NAME(m) (_(ColorName[((m) & 0x01) | (((m) >> 4) & 0x02) | (((m) >> 8) & 0x04)]))
/*
 * Pixel size switch statement for all supported pixel depths.
 */
#define PIXEL_SIZE_CASE(size)           \
    switch (size)                       \
    {                                   \
        case 1:                         \
            PIXEL_LOOP(unsigned char);  \
            break;                      \
        case 2:                         \
            PIXEL_LOOP(unsigned short); \
            break;                      \
        case 4:                         \
            PIXEL_LOOP(unsigned long);  \
            break;                      \
    }

#define DEFAULT_STRING_LENGTH   68
#define DATE_STRING_LENGTH      DEFAULT_STRING_LENGTH
#define TIME_STRING_LENGTH      DEFAULT_STRING_LENGTH
#define CAMERA_STRING_LENGTH    DEFAULT_STRING_LENGTH
#define TELESCOPE_STRING_LENGTH DEFAULT_STRING_LENGTH
#define OBSERVER_STRING_LENGTH  DEFAULT_STRING_LENGTH
#define OBJECT_STRING_LENGTH    DEFAULT_STRING_LENGTH
#define LOCATION_STRING_LENGTH  DEFAULT_STRING_LENGTH
#define DIR_STRING_LENGTH       PATH_MAX
#define NAME_STRING_LENGTH      64
#define EXT_STRING_LENGTH       16
#define MAX_PROCESS_HISTORY     32
#define HISTORY_STRING_LENGTH   68
#define MAX_COMMENTS            32
#define COMMENT_STRING_LENGTH   68
#define HISTOGRAM_BINS          512

struct view_prefs
{
    int AspectStretch;
    int BinStretch;
    int ContrastStretch;
    int Color;
    int Palette;
    int Toolbar;
};
struct ccd_image
{
    unsigned int      width;
    unsigned int      height;
    unsigned int      depth;
    unsigned int      color;
    unsigned int      datamin;
    unsigned int      datamax;
    unsigned long     pixmin;
    unsigned long     pixmax;
    unsigned long     pixave;
    unsigned char    *pixels;
    unsigned int      filter;
    unsigned int      exposure;
    unsigned int      xbin;
    unsigned int      ybin;
    float             zero;
    float             scale;
    float             pixel_width;
    float             pixel_height;
    char              date[DATE_STRING_LENGTH+1];
    char              time[TIME_STRING_LENGTH+1];
    char              camera[CAMERA_STRING_LENGTH+1];
    char              observer[OBSERVER_STRING_LENGTH+1];
    char              telescope[TELESCOPE_STRING_LENGTH+1];
    char              object[OBJECT_STRING_LENGTH+1];
    char              location[LOCATION_STRING_LENGTH+1];
    char              history[MAX_PROCESS_HISTORY][HISTORY_STRING_LENGTH+1];
    char              comments[MAX_COMMENTS][COMMENT_STRING_LENGTH+1];
    char              dir[DIR_STRING_LENGTH];
    char              name[NAME_STRING_LENGTH];
    char              ext[EXT_STRING_LENGTH];
    int               changed;
    unsigned long     histogram[HISTOGRAM_BINS];
    struct ccd_image *next;
    struct view_prefs view;
    GtkWidget        *histogram_view;
    GtkWidget        *histogram_label;
    GdkPixmap        *pixmap;
    GtkWidget        *draw_area;
    GnomeMDIGenericChild *child;
};
struct ccd_dev
{
    char            filename[NAME_STRING_LENGTH];
    int             fd;
    unsigned int    width;
    unsigned int    height;
    unsigned int    depth;
    unsigned int    fields;
    unsigned int    dac_bits;
    unsigned int    color;
    float           pixel_width;
    float           pixel_height;
    char            camera[CAMERA_STRING_LENGTH+1];
    struct ccd_dev *base;
    struct ccd_dev *odd_field;
    struct ccd_dev *even_field;
};
struct ccd_exp
{
    struct ccd_dev   *ccd;
    struct ccd_image *image;
    unsigned int      xoffset;
    unsigned int      yoffset;
    unsigned int      width;
    unsigned int      height;
    unsigned int      xbin;
    unsigned int      ybin;
    unsigned int      dac_bits;
    unsigned int      flags;
    unsigned int      time_count;
    unsigned int      time_scale;
    unsigned int      msec;
    unsigned int      start;
    unsigned int      read_row;
    gint              input_tag;
    guint             timeout_id;
    int               downloading;
};
struct scope_dev
{
    char           filename[NAME_STRING_LENGTH];
    int            fd;
    struct termios save_term;
             int   iface;
             int   init_delay;
    unsigned int   flags;
};
struct wheel_dev
{
    char           filename[NAME_STRING_LENGTH];
    int            fd;
    struct termios save_term;
    int            num_filters;
    int            current;
    unsigned int   status;
};
/*
 * Preferences.
 */
struct _prefs
{
    gchar        WorkingDirectory[DIR_STRING_LENGTH + 1];
    gboolean     CloseWarning;
    gboolean     ViewContrast;
    gboolean     ViewAspect;
    gboolean     ViewBin;
    gboolean     ViewToolbar;
    gboolean     ViewColor;
    gint         ViewPalette;
    GnomeMDIMode ViewMode;
    gint         PortScope;
    gint         PortWheel;
    gint         ScopeIFace;
    gint         ScopeRA;
    gint         ScopeDec;
    gint         ScopeSwap;
    gint         ScopeInitDelay;
    gfloat       RegSig;
    gint         RegXRad;
    gint         RegYRad;
    gint         RegXRange;
    gint         RegYRange;
    gint         TrackTrain;
    gfloat       TrackFieldOffset;
    gfloat       TrackMin;
    gint         TrackMsec;
    gint         TrackSelf;
    gfloat       TrackUp;
    gfloat       TrackDown;
    gfloat       TrackLeft;
    gfloat       TrackRight;
    gint         Filter[7];
    gint         FilterExp[7];
};
extern struct _prefs prefs;
/*
 * Image Object.
 */
#define CCD_IMAGE_COMBINE_MEDIAN        1
#define CCD_IMAGE_COMBINE_MIN           2
#define CCD_IMAGE_COMBINE_MAX           3
#define CCD_IMAGE_COMBINE_MEAN          4
#define CCD_IMAGE_COMBINE_DIFF          5
#define CCD_IMAGE_COMBINE_SUM           6
#define CCD_IMAGE_COMBINE_INTERLEAVE    7
#define CCD_IMAGE_DECONVOLVE_RICHARDSON_LUCY    1
#define CCD_IMAGE_DECONVOLVE_VAN_CITTERT        2
struct ccd_image *ccd_image_new(char *path);
struct ccd_image *ccd_image_new_from_file(char *path);
struct ccd_image *ccd_image_dup(struct ccd_image *image_orig);
void ccd_image_delete(struct ccd_image *image);
int ccd_image_set_dir_name_ext(struct ccd_image *image, char *path);
void ccd_image_histogram(struct ccd_image *image);
int ccd_image_save_fits(struct ccd_image *image);
int ccd_image_load_fits(struct ccd_image *image);
void ccd_image_invert(struct ccd_image *image);
void ccd_image_flip_horiz(struct ccd_image *image);
void ccd_image_flip_vert(struct ccd_image *image);
void ccd_image_rotate(struct ccd_image *image, int angle);
void ccd_image_scale(struct ccd_image *image, unsigned int scale_width, unsigned int scale_height);
unsigned char *ccd_image_convolve(struct ccd_image *image, unsigned char *conv_frame, unsigned xradius, unsigned yradius, float *kernel);
unsigned char *ccd_image_deconvolve(struct ccd_image *image, unsigned char *current_frame, unsigned char *next_frame, unsigned xradius, unsigned yradius, float *kernel, float noise_adj, int op);
void ccd_image_calibrate(struct ccd_image *raw, struct ccd_image *bias, struct ccd_image *dark, struct ccd_image *flat);
unsigned long ccd_image_average(struct ccd_image *image);
void ccd_image_add(struct ccd_image *image, unsigned long offset);
void ccd_image_sub(struct ccd_image *image, unsigned long offset);
void ccd_image_mul(struct ccd_image *image, unsigned long factor);
void ccd_image_div(struct ccd_image *image, unsigned long factor);
void ccd_image_fmul(struct ccd_image *image, float factor);
int ccd_image_find_best_centroid(struct ccd_image *image, unsigned char *pixels, float *x_centroid, float *y_centroid, int x_range, int y_range, int *x_max_radius, int *y_max_radius, float sigs);
unsigned char *ccd_image_register_centroid(struct ccd_image *image, unsigned char *pixels, float x_centroid, float y_centroid, float *x_prev, float *y_prev, int x_range, int y_range, int x_max_radius, int y_max_radius, float sigs, int frame_num);
int ccd_image_register_frames(struct ccd_image *image, unsigned char **pixels, unsigned char **registered_pixels, float *x_centroid, float *y_centroid, int x_range, int y_range, int x_max_radius, int y_max_radius, float sigs, unsigned int frame_count);
void ccd_image_combine_frames(struct ccd_image *image, unsigned char **pixels, unsigned int frame_count, int op);
int ccd_image_split_frames(struct ccd_image *image, unsigned char *pixels[5], unsigned int colors[5], unsigned int lrgb_split);
struct ccd_image *ccd_image_first(void);
struct ccd_image *ccd_image_next(struct ccd_image *image);
struct ccd_image *ccd_image_find_by_name(char *name);
struct ccd_image *ccd_image_find_by_path(char *name);
/*
 * Device control.
 */
#include "ccd_msg.h"
#define SCOPE_MANUAL                -1
#define SCOPE_STAR2K                0
#define SCOPE_LX200                 1
#define SCOPE_STOP                  0
#define SCOPE_LEFT                  1
#define SCOPE_RIGHT                 2
#define SCOPE_UP                    3
#define SCOPE_DOWN                  4
#define SCOPE_FOCUS_STOP            10
#define SCOPE_FOCUS_IN              11
#define SCOPE_FOCUS_OUT             12
#define SCOPE_REV_RA                1
#define SCOPE_REV_DEC               2
#define SCOPE_SLEW                  4
#define SCOPE_SLEW_TOGGLE           8
#define SCOPE_SWAP_XY               16
#define SCOPE_FOCUS_SLOW            32
#define SCOPE_FOCUS_MED             64
#define SCOPE_FOCUS_FAST            128
#define WHEEL_IDLE                  0
#define WHEEL_BUSY                  1
int  ccd_connect(struct ccd_dev *ccd);
int  ccd_release(struct ccd_dev *ccd);
void ccd_expose_frame(struct ccd_exp *exposure);
int  ccd_load_frame(struct ccd_exp *exposure);
void ccd_abort_exposures(struct ccd_exp *exposure);
int  scope_connect(struct scope_dev *scope);
int  scope_release(struct scope_dev *scope);
void scope_move(struct scope_dev *scope, unsigned int dir);
int wheel_read(struct wheel_dev *wheel);
int wheel_connect(struct wheel_dev *wheel);
int wheel_release(struct wheel_dev *wheel);
int wheel_reset(struct wheel_dev *wheel);
int wheel_query(struct wheel_dev *wheel);
int wheel_goto(struct wheel_dev *wheel, int pos);
/*
 * Acquire Dialog.
 */
void imageUpdateList(void);
void activateAcquireImage(void (*)(struct ccd_image *));
void deactivateAcquireImage(int reactivate);
/*
 * Global data.
 */
extern GtkObject *mdi;
extern GdkCursor *cursorWait;
extern unsigned int verbose;
extern unsigned long view_palettes[NUM_PALETTES][256];
extern gchar *ColorName[8];

