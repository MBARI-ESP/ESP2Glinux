/*
 * GCCD - Gnome CCD Camera Controller
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

#include "gccd.h"
#define MAX_CCD_DEVICES     4
#define MAX_EXPOSURES       100
#define GCCD_SET_BIAS       1
#define GCCD_SET_DARK       2
#define GCCD_SET_FLAT       3
#define GCCD_NO_COMB        0
#define EXPOSE_IN_PROGRESS  1
#define EXPOSE_INIT         2
#define FOCUS_SCALE_4_1     1
#define FOCUS_SCALE_2_1     2
#define FOCUS_SCALE_1_1     3
#define FOCUS_SCALE_1_2     4
#define FOCUS_SCALE_1_4     5
#define FOCUS_STOP          0
#define FOCUS_PENDING       1
#define FOCUS_START         2
#define GUIDE_WIDTH         35
#define GUIDE_HEIGHT        25
#define GUIDE_SCALE         5
#define GUIDE_SELECT_BIN    2
#define GUIDE_MIN_OFFSET    0.25
#define GUIDE_MIN_RATE      0.1
#define GUIDE_AUTO          1
#define GUIDE_SELF          2
#define GUIDE_ACQUIRE       16
#define GUIDE_ACQUIRE_COUNT 2
#define FIELD_BOTH          0
#define FIELD_ODD           1
#define FIELD_EVEN          2
#define FIELD_SUM           CCD_IMAGE_COMBINE_SUM
#define FIELD_INTERLEAVE    CCD_IMAGE_COMBINE_INTERLEAVE
#define FIELD_DARK          (FIELD_INTERLEAVE+1)
#define MAX_FILTERS         6

static int              ccd_num_devices = 0;
static struct ccd_dev   ccd_devices[MAX_CCD_DEVICES][3];
static struct scope_dev scope_state = {{0},0,{0},0,-1,0};
static struct
{
    struct wheel_dev  wheel;
    gint              input_tag;
    unsigned int      filter[MAX_FILTERS];
    unsigned int      filter_mask[10];
    gint              current_sequence;
    unsigned int      sequence[MAX_FILTERS];
    unsigned int      exp_percent[MAX_FILTERS];
    struct ccd_image *flat_frame[MAX_FILTERS];
    GList            *list_filters;
} wheel_state = {{{0},0},0};
static struct
{
    int               exposing;
    int               focusing;
    int               guiding;
    int               sequencing;
    int               training;
    struct ccd_exp    exposure;
    struct ccd_exp    interleave;
    struct ccd_exp    focus;
    struct ccd_exp    guide;
    struct ccd_image *bias_frame;
    struct ccd_image *dark_frame;
    struct ccd_image *flat_frame;
    unsigned char    *exp_pixels[MAX_EXPOSURES];
    unsigned char    *matrix_pixels[MAX_EXPOSURES][5];
    unsigned int      matrix_colors[5];
    unsigned int      matrix_num_frames;
    char              exp_basename[NAME_STRING_LENGTH/2 + 1];
    unsigned int      exp_basenum;
    unsigned int      exp_count;
    unsigned int      exp_current;
    unsigned int      exp_delay;
    unsigned int      exp_comb;
    int               exp_tracknstack;
    unsigned int      exp_fields;
    unsigned int      exp_loadtime;
    unsigned int      bin_fields;
    float             track_x_centroid;
    float             track_y_centroid;
    float             reg_noise_sigs;
    int               reg_x_max_radius;
    int               reg_y_max_radius;
    int               reg_x_range;
    int               reg_y_range;
    unsigned int      focus_scale;
    unsigned int      focus_width;
    unsigned int      focus_height;
    unsigned int      focus_view_xoffset;
    unsigned int      focus_view_yoffset;
    unsigned int      focus_view_width;
    unsigned int      focus_view_height;
    unsigned int      guide_msec;
    unsigned int      guide_fields;
    unsigned int      guide_dark;
    unsigned int      guide_acquire_count;
    float             guide_track_up;
    float             guide_track_down;
    float             guide_track_left;
    float             guide_track_right;
    float             guide_x_centroid;
    float             guide_y_centroid;
    float             guide_dx_centroid;
    float             guide_dy_centroid;
    float             guide_interleave_offset;
    float             guide_min_offset;
    float             guide_train_msec;
} ccd_state = {0};
static struct
{
    GList     *list_images;
    int        beep_when_done;
    int        auto_save;
    int        current_notebook_page;
    GtkWidget *window;
    GtkWidget *notebook;
    GtkWidget *progress_bar;
    GtkWidget *status;
    GtkWidget *button_exp;
    GtkWidget *arrow_up;
    GtkWidget *arrow_down;
    GtkWidget *arrow_left;
    GtkWidget *arrow_right;
    GtkWidget *arrow_in;
    GtkWidget *arrow_out;
    GtkWidget *focus_control;
    GtkWidget *combo_bias;
    GtkWidget *combo_dark;
    GtkWidget *combo_flat;
    GtkWidget *spin_basenum;
    GtkWidget *view_focus;
    GtkWidget *draw_histogram;
    GtkWidget *label_minmax;
    GtkWidget *view_guide;
    GtkWidget *button_guide;
    GtkWidget *spin_track_left;
    GtkWidget *spin_track_right;
    GtkWidget *spin_track_up;
    GtkWidget *spin_track_down;
    GtkWidget *label_offset;
    GtkWidget *radio_filter[MAX_FILTERS];
    GtkWidget *combo_filterflat[MAX_FILTERS];
    GtkWidget *wheel_status;
} gui_state;
/*
 * Prototypes.
 */
static void (*imageAdd)(struct ccd_image *) = NULL;
static gboolean cbUpdateExposure(gpointer data);
static gboolean cbInterleaveExposure(gpointer data);
static void cbReadExposure(gpointer data, gint fd, GdkInputCondition in);
static void cbReadFocus(gpointer data, gint fd, GdkInputCondition in);
static void cbReadGuide(gpointer data, gint fd, GdkInputCondition in);

/***************************************************************************\
*                                                                           *
*                            Device utilities                               *
*                                                                           *
\***************************************************************************/

static struct ccd_dev *ccd_find_by_name(char *name)
{
    int i;

    for (i = 0; i < ccd_num_devices; i++)
        if (!strcmp(ccd_devices[i][0].camera, name))
            return (&ccd_devices[i][0]);
    return (NULL);
}
static int filter_find_by_name(char *name)
{
    int    i;
    GList *node;

    for (node = wheel_state.list_filters, i = 0; node; node = node->next, i++)
        if (!strcmp(node->data, name))
            return (i);
    return (0);
}
/*
 * Update arrow buttons and move scope.
 */
static void scope_update(unsigned int dir, int pressed)
{
           GtkWidget *arrow;
    static GtkWidget *prev_arrow = NULL;
    static int        stopped    = TRUE;
    unsigned int      stop_type  = SCOPE_STOP;
    switch (dir)
    {
        case SCOPE_UP:
            arrow = gui_state.arrow_up;
            break;
        case SCOPE_DOWN:
            arrow = gui_state.arrow_down;
            break;
        case SCOPE_LEFT:
            arrow = gui_state.arrow_left;
            break;
        case SCOPE_RIGHT:
            arrow = gui_state.arrow_right;
            break;
        case SCOPE_STOP:
            pressed = FALSE;
            arrow   = prev_arrow;
            break;
        case SCOPE_FOCUS_IN:
            arrow     = gui_state.arrow_in;
            stop_type = SCOPE_FOCUS_STOP;
            break;
        case SCOPE_FOCUS_OUT:
            arrow     = gui_state.arrow_out;
            stop_type = SCOPE_FOCUS_STOP;
            break;
        default:
            pressed = FALSE;
            arrow   = NULL;
    }
    if (pressed && prev_arrow)
    {
        gtk_widget_set_state(GTK_WIDGET(prev_arrow), GTK_STATE_NORMAL);
        scope_move(&scope_state, SCOPE_STOP);
        prev_arrow = NULL;
    }
    if (arrow)
    {
        gtk_widget_set_state(GTK_WIDGET(arrow), pressed ? GTK_STATE_ACTIVE : GTK_STATE_NORMAL);
        prev_arrow = pressed ? arrow : NULL;
    }
    /*
     * Don't slew when exposing.
     */
    if ((!(ccd_state.exposing || ccd_state.training) || ccd_state.focusing) && (scope_state.flags & SCOPE_SLEW_TOGGLE))
        scope_state.flags |=  SCOPE_SLEW;
    else
        scope_state.flags &= ~SCOPE_SLEW;
    /*
     * Don't send stop command if already stopped.
     */
    if (!(stopped && dir == SCOPE_STOP))
        scope_move(&scope_state, !pressed ? stop_type : dir);
    stopped = (dir == SCOPE_STOP) || (!pressed && stop_type == SCOPE_STOP);
}

/***************************************************************************\
*                                                                           *
*                            Focus utilities                                *
*                                                                           *
\***************************************************************************/

/*
 * Stop focus/find mode.
 */
static int focus_stop(gint done)
{
    int focusing;

    if ((focusing = ccd_state.focusing) == FOCUS_START)
    {
        if (ccd_state.focus.timeout_id)
        {
            gtk_timeout_remove(ccd_state.focus.timeout_id);
            ccd_state.focus.timeout_id = 0;
        }
        if (ccd_state.focus.input_tag)
        {
            gdk_input_remove(ccd_state.focus.input_tag);
            ccd_state.focus.input_tag = 0;
        }
        ccd_state.focus.downloading = FALSE;
        ccd_abort_exposures(&ccd_state.focus);
        ccd_release(ccd_state.focus.ccd);
        if (done)
        {
            if (gui_state.progress_bar)
                gtk_progress_bar_update(GTK_PROGRESS_BAR(gui_state.progress_bar), 0.0);
            if (gui_state.button_exp)
                gtk_object_set(GTK_OBJECT(gui_state.button_exp), "label", _("Begin"), NULL);
            if (gui_state.status)
                gtk_label_set_text(GTK_LABEL(gui_state.status), _("Idle"));
            if (verbose & 1) g_print("Idle\n");
            if (gui_state.notebook)
                gtk_widget_set_sensitive(gui_state.notebook, TRUE);
        }
    }
    ccd_state.focusing = FOCUS_STOP;
    return (focusing);
}
/*
 * Start focus/find.
 */
static gint focus_restart(gpointer data)
{
    unsigned int scale/*, focusing = (unsigned int)data*/;
    if (ccd_state.focusing == FOCUS_PENDING)
    {
        if (ccd_state.focus_scale > FOCUS_SCALE_1_1)
        {
            scale                   = (ccd_state.focus_scale - FOCUS_SCALE_1_1) * 2;
            ccd_state.focus.xoffset = ccd_state.focus_view_xoffset / scale;
            ccd_state.focus.yoffset = ccd_state.focus_view_yoffset / scale;
        }
        else
        {
            if ((scale = (FOCUS_SCALE_1_1 - ccd_state.focus_scale) * 2) == 0) scale = 1;
            ccd_state.focus.xoffset = ccd_state.focus_view_xoffset * scale;
            ccd_state.focus.yoffset = ccd_state.focus_view_yoffset * scale;
        }
        ccd_state.focus.msec = ccd_state.exposure.time_count * ccd_state.exposure.time_scale;
        if (!ccd_state.focus.ccd->fd)
            ccd_connect(ccd_state.focus.ccd);
        ccd_expose_frame(&ccd_state.focus);
        ccd_state.focus.timeout_id = gtk_timeout_add(ccd_state.focus.msec < 25000 ? 250 : ccd_state.focus.msec / 100, (GtkFunction)cbUpdateExposure, &ccd_state.focus);
        ccd_state.focus.input_tag  = gdk_input_add(ccd_state.focus.ccd->fd, GDK_INPUT_READ, (GdkInputFunction)cbReadFocus, NULL);
        ccd_state.focusing         = FOCUS_START;
    }
    return (FALSE);
}
static void focus_start(unsigned int focusing)
{
    if (focusing && ccd_state.focusing == FOCUS_STOP)
    {
        ccd_state.focusing = FOCUS_PENDING;
        gtk_idle_add(focus_restart, (gpointer)focusing);
    }
}
static void focus_recalc_viewport()
{
    unsigned int scale, focusing;

    focusing = focus_stop(FALSE);
    /*
     * Set focus exposure parameters based on visible area of focus window.
     */
    if (ccd_state.focus_scale > FOCUS_SCALE_1_1)
    {
        scale                   = (ccd_state.focus_scale - FOCUS_SCALE_1_1) * 2;
        ccd_state.focus.xbin    = 1;
        ccd_state.focus.ybin    = 1;
        ccd_state.focus.xoffset = ccd_state.focus_view_xoffset              / scale;
        ccd_state.focus.yoffset = ccd_state.focus_view_yoffset              / scale;
        ccd_state.focus.width   = (ccd_state.focus_view_width  + scale - 1) / scale;
        ccd_state.focus.height  = (ccd_state.focus_view_height + scale - 1) / scale;
        }
    else
    {
        scale                   = (FOCUS_SCALE_1_1 - ccd_state.focus_scale) * 2;
        if (scale == 0) scale   = 1;
        ccd_state.focus.xbin    = scale;
        ccd_state.focus.ybin    = scale;
        ccd_state.focus.xoffset = ccd_state.focus_view_xoffset * scale;
        ccd_state.focus.yoffset = ccd_state.focus_view_yoffset * scale;
        ccd_state.focus.width   = ccd_state.focus_view_width   * scale;
        ccd_state.focus.height  = ccd_state.focus_view_height  * scale;
    }
    if (ccd_state.focus.xoffset + ccd_state.focus.width > ccd_state.focus.ccd->width)
        ccd_state.focus.width   = ccd_state.focus.ccd->width - ccd_state.focus.xoffset;
    if (ccd_state.focus.yoffset + ccd_state.focus.height > ccd_state.focus.ccd->height)
        ccd_state.focus.height  = ccd_state.focus.ccd->height - ccd_state.focus.yoffset;
    /*
     * Set the image to match the exposure.
     */
    if (!ccd_state.focus.image)
    {
        ccd_state.focus.image = (struct ccd_image *)malloc(sizeof(struct ccd_image));
        memset(ccd_state.focus.image, 0, sizeof(struct ccd_image));
    }
    if (ccd_state.focus.image->pixels)
        free(ccd_state.focus.image->pixels);
    ccd_state.focus.image->width  = ccd_state.focus.width  / ccd_state.focus.xbin;
    ccd_state.focus.image->height = ccd_state.focus.height / ccd_state.focus.ybin;
    ccd_state.focus.image->depth  = ccd_state.focus.ccd->depth;
    ccd_state.focus.image->pixels = malloc(ccd_state.focus.image->width * ccd_state.focus.image->height * ((ccd_state.focus.image->depth + 7)/8));
    if (!ccd_state.focus.image->pixmap)
    {
        ccd_state.focus.image->pixmap = gdk_pixmap_new(gui_state.window->window, ccd_state.focus_width, ccd_state.focus_height, -1);
        gdk_draw_rectangle(ccd_state.focus.image->pixmap, gui_state.window->style->fg_gc[GTK_STATE_NORMAL], TRUE,
                           0, 0, ccd_state.focus_width, ccd_state.focus_height);
    }
    focus_start(focusing);
}

/***************************************************************************\
*                                                                           *
*                            Guide utilities                                *
*                                                                           *
\***************************************************************************/

/*
 * Stop guiding.
 */
static int guide_stop(void)
{
    if (ccd_state.guiding)
    {
        scope_update(SCOPE_STOP, FALSE);
        if (ccd_state.guide.input_tag)
        {
            gdk_input_remove(ccd_state.guide.input_tag);
            ccd_state.guide.input_tag = 0;
        }
        ccd_state.guide.downloading = FALSE;
        ccd_state.guiding          &= ~GUIDE_ACQUIRE;
        ccd_abort_exposures(&ccd_state.guide);
        ccd_release(ccd_state.guide.ccd);
        scope_release(&scope_state);
    }
    return (ccd_state.guiding);
}
/*
 * Start guiding.
 */
static void guide_start(int guiding)
{
    if (guiding && ccd_state.guiding)
    {
        if (!ccd_state.guide.ccd->fd)
            ccd_connect(ccd_state.guide.ccd);
        if (!scope_state.fd)
            while (scope_connect(&scope_state) < 0)
                gnome_warning_dialog_parented(_("Telescope busy.  Wait for GOTO to complete."), GTK_WINDOW(gui_state.window));
        /*
         * Set the image to match the exposure.
         */
        if (!ccd_state.guide.image)
        {
            ccd_state.guide.image = (struct ccd_image *)malloc(sizeof(struct ccd_image));
            memset(ccd_state.guide.image, 0, sizeof(struct ccd_image));
            ccd_state.guide.image->width  = ccd_state.guide.width  / ccd_state.guide.xbin;
            ccd_state.guide.image->height = ccd_state.guide.height / ccd_state.guide.ybin;
            ccd_state.guide.image->depth  = ccd_state.guide.ccd->depth;
        }
        if (!ccd_state.guide.image->pixels)
            ccd_state.guide.image->pixels = malloc(ccd_state.guide.image->width * ccd_state.guide.image->height * ((ccd_state.guide.image->depth + 7)/8));
        if (!ccd_state.guide.image->pixmap)
        {
            ccd_state.guide.image->pixmap = gdk_pixmap_new(gui_state.window->window, GUIDE_WIDTH*GUIDE_SCALE, GUIDE_HEIGHT*GUIDE_SCALE, -1);
            gdk_draw_rectangle(ccd_state.guide.image->pixmap, gui_state.window->style->fg_gc[GTK_STATE_NORMAL], TRUE,
                               0, 0, GUIDE_WIDTH*GUIDE_SCALE, GUIDE_HEIGHT*GUIDE_SCALE);
        }
        ccd_state.guide.msec  = ccd_state.guide_msec;
        ccd_state.guide.flags = 0;
        ccd_expose_frame(&ccd_state.guide);
        ccd_state.guide.flags = CCD_EXP_FLAGS_NOWIPE_FRAME | CCD_EXP_FLAGS_NOCLEAR_FRAME;
        ccd_state.guide.input_tag  = gdk_input_add(ccd_state.guide.ccd->fd, GDK_INPUT_READ, (GdkInputFunction)cbReadGuide, NULL);
        if (gui_state.status)
            gtk_label_set_text(GTK_LABEL(gui_state.status), _("Stabilize Guiding..."));
        if (verbose & 1) g_print("Stabilize Guiding...\n");
        /*
         * Don't return until guide star acquired GUIDE_ACQURE_COUNT times in a row.
         */
        if (!ccd_state.guide_dark)
        {
            ccd_state.guide_acquire_count = 0;
            ccd_state.guiding            |= GUIDE_ACQUIRE;
            while (ccd_state.guiding & GUIDE_ACQUIRE)
                gtk_main_iteration();
        }
        if (verbose & 2)
        {
            g_print("\"Guiding on %d msec interval\"\n", ccd_state.guide_msec);
            g_print("\"RA Error \",\t\"Dec Error\"\n");
            g_print("\"=========\",\t\"=========\"\n");
        }
    }
}

/***************************************************************************\
*                                                                           *
*                          Exposure utilities                               *
*                                                                           *
\***************************************************************************/

/*
 * Stop or cancel an exposure.
 */
static void exposure_stop(gint done)
{
    int i;

    if (ccd_state.exposing)
    {
        /*
         * Stop guiding.
         */
        guide_stop();
        /*
         * Stop/cancel the exposure request.
         */
        if (ccd_state.exposing == EXPOSE_IN_PROGRESS)
        {
            if (ccd_state.exposure.timeout_id)
            {
                gtk_timeout_remove(ccd_state.exposure.timeout_id);
                ccd_state.exposure.timeout_id = 0;
            }
            if (ccd_state.exposure.input_tag)
            {
                gdk_input_remove(ccd_state.exposure.input_tag);
                ccd_state.exposure.input_tag = 0;
            }
            ccd_state.exposure.timeout_id = 0;
            ccd_state.exposure.input_tag  = 0;
            if (ccd_state.exposure.ccd->fd)
            {
                ccd_abort_exposures(&ccd_state.exposure);
                ccd_release(ccd_state.exposure.ccd);
            }
            if (ccd_state.exposure.image)
            {
                ccd_image_delete(ccd_state.exposure.image);
                ccd_state.exposure.image = NULL;
            }
            for (i = 0; i < ccd_state.exp_current; i++)
                if (ccd_state.exp_pixels[i])
                {
                    free(ccd_state.exp_pixels[i]);
                    ccd_state.exp_pixels[i] = NULL;
                }
            if (ccd_state.interleave.timeout_id)
            {
                gtk_timeout_remove(ccd_state.interleave.timeout_id);
                ccd_state.interleave.timeout_id = 0;
            }
            if (ccd_state.interleave.ccd && ccd_state.interleave.ccd->fd)
                ccd_release(ccd_state.interleave.ccd);
            if (ccd_state.interleave.image)
            {
                ccd_image_delete(ccd_state.interleave.image);
                ccd_state.interleave.image = NULL;
            }
        }
        ccd_state.exp_current          = 0;
        ccd_state.exposing             = FALSE;
        ccd_state.exposure.downloading = FALSE;
        if (done)
        {
            if (gui_state.progress_bar)
                gtk_progress_bar_update(GTK_PROGRESS_BAR(gui_state.progress_bar), 0.0);
            if (gui_state.button_exp)
                gtk_object_set(GTK_OBJECT(gui_state.button_exp), "label", _("Begin"), NULL);
            if (gui_state.status)
                gtk_label_set_text(GTK_LABEL(gui_state.status), _("Idle"));
            if (verbose & 1) g_print("Idle\n");
            if (gui_state.notebook)
                gtk_widget_set_sensitive(gui_state.notebook, TRUE);
        }
    }
}
/*
 * Begin exposure.
 */
static gint exposure_start(gpointer data)
{
    gchar str[16];
    gint  guiding = (guint)data;
    if (ccd_state.exposing)
    {
        ccd_state.exposure.msec = ccd_state.exposure.time_count * ccd_state.exposure.time_scale;
        if (ccd_state.sequencing)
             ccd_state.exposure.msec = (ccd_state.exposure.msec * wheel_state.exp_percent[wheel_state.current_sequence]) / 100;
        if (ccd_state.exposure.msec <= 1000)
            guiding = FALSE;
        ccd_state.guide.ccd = ccd_state.guide.ccd->base;
        if (ccd_state.exp_fields == FIELD_BOTH)
        {
            ccd_state.exposure.ccd = ccd_state.exposure.ccd->base;
            if (guiding && (ccd_state.guide.ccd->base == ccd_state.exposure.ccd->base))
            {
                if (verbose & 1) g_print("Invalid exp_fields in exposure_start!\n");
                ccd_state.exposing = FALSE;
                return (FALSE);
            }
        }
        else if (ccd_state.exp_fields == FIELD_ODD)
        {
            ccd_state.exposure.ccd = ccd_state.exposure.ccd->odd_field;
            if (guiding && (ccd_state.exposure.ccd->base == ccd_state.guide.ccd->base))
            {
                ccd_state.guide.ccd = ccd_state.exposure.ccd->even_field;
                /*
                 * Move guide centroid down by 1/4 pixel
                 */
                ccd_state.guide_y_centroid += ccd_state.guide_interleave_offset;
            }
        }
        else  // ccd_state.exp_fields == FIELD_EVEN, FIELD_SUM ,FIELD_INTERLEAVE
        {
            ccd_state.exposure.ccd = ccd_state.exposure.ccd->even_field;
            if (guiding && (ccd_state.exposure.ccd->base == ccd_state.guide.ccd->base))
            {
                ccd_state.guide.ccd = ccd_state.exposure.ccd->odd_field;
                /*
                 * Move guide centroid up by 1/4 pixel
                 */
                ccd_state.guide_y_centroid -= ccd_state.guide_interleave_offset;
            }
        }
        guide_start(guiding);
        if (ccd_state.exposing)
        {
            ccd_state.exposing      = EXPOSE_IN_PROGRESS;
            ccd_state.exp_current   = 1;
            ccd_connect(ccd_state.exposure.ccd);
            ccd_expose_frame(&ccd_state.exposure);
            ccd_state.exposure.input_tag  = gdk_input_add(ccd_state.exposure.ccd->fd, GDK_INPUT_READ, (GdkInputFunction)cbReadExposure, NULL);
            ccd_state.exposure.timeout_id = gtk_timeout_add(ccd_state.exposure.msec < 25000 ? 250 : ccd_state.exposure.msec / 100, (GtkFunction)cbUpdateExposure, &ccd_state.exposure);
            if (ccd_state.exp_fields == FIELD_INTERLEAVE
             && (!ccd_state.guiding || (ccd_state.exposure.ccd->base != ccd_state.guide.ccd->base))
             && (ccd_state.exp_loadtime <= ccd_state.exposure.msec))
            {
                /*
                 * If interleaving without self-guiding, try and start the odd field exposure
                 * right after the even exposure.
                 */
                ccd_state.interleave            = ccd_state.exposure;
                ccd_state.interleave.ccd        = ccd_state.exposure.ccd->odd_field;
                ccd_state.interleave.flags      = CCD_EXP_FLAGS_NOWIPE_FRAME;
                ccd_state.interleave.input_tag  = 0;
                ccd_state.interleave.image      = NULL;
                ccd_state.interleave.msec      -= ccd_state.exp_loadtime/2;
                ccd_state.interleave.timeout_id = gtk_timeout_add(ccd_state.exp_loadtime, (GtkFunction)cbInterleaveExposure, NULL);
                ccd_connect(ccd_state.interleave.ccd);
            }
            if (ccd_state.sequencing)
                sprintf(str, "1/%d %s", ccd_state.exp_count, (char *)(g_list_nth(wheel_state.list_filters, wheel_state.filter[wheel_state.current_sequence])->data));
            else
                sprintf(str, "1/%d", ccd_state.exp_count);
            if (gui_state.status)
                gtk_label_set_text(GTK_LABEL(gui_state.status), str);
            if (verbose & 1) {g_print(str); g_print("\n");}
        }
    }
    return (FALSE);
}

/***************************************************************************\
*                                                                           *
*                           Guiding callbacks                               *
*                                                                           *
\***************************************************************************/

static gboolean cbUpdateGuide(gpointer data)
{
    scope_update(SCOPE_STOP, FALSE);
    ccd_state.guide.timeout_id = 0;
    if (fabs(ccd_state.guide_dx_centroid) >= fabs(ccd_state.guide_dy_centroid))
    {
        if (ccd_state.guide_dx_centroid <= -ccd_state.guide_min_offset)
        {
            if (data) usleep((guint)data);
            scope_update(SCOPE_LEFT, TRUE);
            ccd_state.guide.timeout_id  = gtk_timeout_add(-ccd_state.guide_dx_centroid / ccd_state.guide_track_left  * 1000, (GtkFunction)cbUpdateGuide, (gpointer)500);
            ccd_state.guide_dx_centroid = 0.0;
        }
        else if (ccd_state.guide_dx_centroid >= ccd_state.guide_min_offset)
        {
            if (data) usleep((guint)data);
            scope_update(SCOPE_RIGHT, TRUE);
            ccd_state.guide.timeout_id  = gtk_timeout_add(ccd_state.guide_dx_centroid / ccd_state.guide_track_right * 1000, (GtkFunction)cbUpdateGuide, (gpointer)500);
            ccd_state.guide_dx_centroid = 0.0;
        }
    }
    else
    {
        if (ccd_state.guide_dy_centroid <= -ccd_state.guide_min_offset)
        {
            if (data) usleep((guint)data);
            scope_update(SCOPE_UP, TRUE);
            ccd_state.guide.timeout_id  = gtk_timeout_add(-ccd_state.guide_dy_centroid / ccd_state.guide_track_up * 1000, (GtkFunction)cbUpdateGuide, (gpointer)500);
            ccd_state.guide_dy_centroid = 0.0;
        }
        else if (ccd_state.guide_dy_centroid >= ccd_state.guide_min_offset)
        {
            if (data) usleep((guint)data);
            scope_update(SCOPE_DOWN, TRUE);
            ccd_state.guide.timeout_id  = gtk_timeout_add(ccd_state.guide_dy_centroid / ccd_state.guide_track_down * 1000, (GtkFunction)cbUpdateGuide, (gpointer)500);
            ccd_state.guide_dy_centroid = 0.0;
        }
    }
    return (FALSE);
}

/***************************************************************************\
*                                                                           *
*                          Exposure callbacks                               *
*                                                                           *
\***************************************************************************/

static gboolean cbUpdateExposure(gpointer data)
{
    int             now_msec;
    gfloat          progress;
    struct timeval  now;
    struct ccd_exp *exposure = (struct ccd_exp *)data;
    if (gui_state.progress_bar)
    {
        gettimeofday(&now, NULL);
        now_msec = now.tv_sec * 1000 + now.tv_usec / 1000;
        progress = (gfloat)(now_msec - exposure->start) / (gfloat)exposure->msec;
        if (progress > 1.0)
            progress = 1.0;
        gtk_progress_bar_update(GTK_PROGRESS_BAR(gui_state.progress_bar), progress);
    }
    return (TRUE);
}
static gboolean cbInterleaveExposure(gpointer data)
{
    ccd_expose_frame(&ccd_state.interleave);
    ccd_state.interleave.timeout_id = 0;
    return (FALSE);
}
static void cbReadExposure(gpointer data, gint fd, GdkInputCondition in)
{
    int               i, j;
    unsigned long     start, stop;
    struct timeval    now;
    struct ccd_image *flat_frame;
    unsigned char    *comb_pixels[MAX_EXPOSURES];
    unsigned char    *registered_pixels[MAX_EXPOSURES];
    gchar             str[16];

    /*
     * Reset input & timer calls.
     */
    gtk_timeout_remove(ccd_state.exposure.timeout_id);
    gdk_input_remove(ccd_state.exposure.input_tag);
    ccd_state.exposure.timeout_id  = 0;
    ccd_state.exposure.input_tag   = 0;
    ccd_state.exposure.downloading = TRUE;
    /*
     * Stop guiding until download complete.
     */
    if (ccd_state.guiding)
        guide_stop();
    /*
     * Reset progress bar.
     */
    if (gui_state.progress_bar)
    {
        gtk_progress_bar_update(GTK_PROGRESS_BAR(gui_state.progress_bar), 1.0);
        gtk_main_iteration();
    }
    gettimeofday(&now, NULL);
    start = now.tv_sec * 1000 + now.tv_usec / 1000;
    while (ccd_state.exposure.downloading && ccd_load_frame(&ccd_state.exposure))
    {
        /*
         * Update load progress bar every 32 scanlines.
         */
        if (!(ccd_state.exposure.read_row & 0x0F) && gui_state.progress_bar)
        {
            gtk_progress_bar_update(GTK_PROGRESS_BAR(gui_state.progress_bar), 1.0 - (gfloat)(ccd_state.exposure.read_row * ccd_state.exposure.ybin) / (gfloat)ccd_state.exposure.height);
            gtk_main_iteration();
        }
    }
    gettimeofday(&now, NULL);
    stop = now.tv_sec * 1000 + now.tv_usec / 1000;
    if (verbose & 1) g_print("Downloaded image in %ld msecs\n", stop - start);
    if (gui_state.progress_bar)
        gtk_progress_bar_update(GTK_PROGRESS_BAR(gui_state.progress_bar), 0.0);
    /*
     * Bail out if operation canceled.
     */
    if (!ccd_state.exposure.downloading)
        return;
    ccd_state.exposure.downloading = FALSE;
    /*
     * If not interleaving a color one-shot, force color to monochrome.
     */
    if ((ccd_state.exposure.image->color != CCD_COLOR_MONOCHROME)
     && (ccd_state.exposure.ccd->base->fields > 1)
     && (ccd_state.exp_fields != FIELD_INTERLEAVE))
        ccd_state.exposure.image->color = CCD_COLOR_MONOCHROME;
    /*
     * Interleave field processing.
     */
    if ((ccd_state.exp_fields == FIELD_INTERLEAVE)
     && (!ccd_state.guiding || (ccd_state.exposure.ccd->base != ccd_state.guide.ccd->base)))
    {
        if (ccd_state.exp_loadtime <= ccd_state.exposure.msec)
        {
            /*
             * If interleaving without self-guiding, immediately download the odd field.
             */
            if (ccd_state.interleave.image->pixels == NULL)
            {
                if (verbose & 1) g_print("Interleaved exposure not started!\n");
                if (ccd_state.interleave.timeout_id)
                    gtk_timeout_remove(ccd_state.interleave.timeout_id);
                ccd_release(ccd_state.interleave.ccd);
                ccd_image_delete(ccd_state.interleave.image);
            }
            else
            {
                ccd_state.exposure.downloading = TRUE;
                if (gui_state.progress_bar)
                {
                    gtk_progress_bar_update(GTK_PROGRESS_BAR(gui_state.progress_bar), 1.0);
                    gtk_main_iteration();
                }
                while (ccd_state.exposure.downloading && ccd_load_frame(&ccd_state.interleave))
                {
                    if (!(ccd_state.interleave.read_row & 0x0F) && gui_state.progress_bar)
                    {
                        gtk_progress_bar_update(GTK_PROGRESS_BAR(gui_state.progress_bar), 1.0 - (gfloat)(ccd_state.interleave.read_row * ccd_state.interleave.ybin) / (gfloat)ccd_state.interleave.height);
                        gtk_main_iteration();
                    }
                }
                if (gui_state.progress_bar)
                    gtk_progress_bar_update(GTK_PROGRESS_BAR(gui_state.progress_bar), 0.0);
                if (!ccd_state.exposure.downloading)
                {
                    ccd_state.exp_loadtime = 0;
                    return;
                }
                ccd_state.exposure.downloading = FALSE;
                ccd_release(ccd_state.exposure.ccd);
                /*
                 * Fake out following field tests.
                 */
                ccd_state.exp_pixels[ccd_state.exp_current - 1] = ccd_state.exposure.image->pixels;
                ccd_state.exposure.image->pixels                = ccd_state.interleave.image->pixels;
                ccd_state.exposure.ccd                          = ccd_state.interleave.ccd;
                ccd_state.interleave.image->pixels              = NULL;
                ccd_image_delete(ccd_state.interleave.image);
                ccd_state.interleave.image                      = NULL;
            }
        }
        /*
         * Save the download time.
         */
        if (ccd_state.exposure.ccd == ccd_state.exposure.ccd->odd_field)
        {
            ccd_state.exp_loadtime = stop - start;
            if (ccd_state.exp_loadtime < 100)
                ccd_state.exp_loadtime = 100;
        }
    }
    if ((ccd_state.exp_fields >= FIELD_SUM) && (ccd_state.exposure.ccd == ccd_state.exposure.ccd->even_field))
    {
        /*
         * Save even frame and prepare for odd field exposure.
         */
        ccd_state.exp_pixels[--ccd_state.exp_current] = ccd_state.exposure.image->pixels;
        ccd_state.exposure.image->pixels              = NULL;
        ccd_release(ccd_state.exposure.ccd);
        /*
         * If self guided, stop guide on current field and restart on opposite field.
         */
        if (ccd_state.guiding && ccd_state.exposure.ccd->base == ccd_state.guide.ccd->base)
        {
            ccd_state.guide.ccd = ccd_state.guide.ccd->even_field;
            /*
             * Move guide centroid back down by 1/4 pixel if last exposure or 1/2 pixel for next field.
             */
            ccd_state.guide_y_centroid += (ccd_state.exp_current < ccd_state.exp_count) ? ccd_state.guide_interleave_offset * 2 : ccd_state.guide_interleave_offset;
        }
        ccd_state.exposure.ccd = ccd_state.exposure.ccd->odd_field;
        ccd_connect(ccd_state.exposure.ccd);
    }
    else
    {
        /*
         * Combine interleaved fields.
         */
        if (ccd_state.exp_fields >= FIELD_SUM)
        {
            /*
             * Save odd frame and combine with previous frame.
             */
            ccd_state.exp_pixels[ccd_state.exp_current] = ccd_state.exposure.image->pixels;
            ccd_state.exposure.image->pixels            = NULL;
            ccd_image_combine_frames(ccd_state.exposure.image, &ccd_state.exp_pixels[ccd_state.exp_current - 1], 2, ccd_state.exp_fields);
            /*
             * Free saved frames and release ccd.
             */
            free(ccd_state.exp_pixels[ccd_state.exp_current - 1]);
            free(ccd_state.exp_pixels[ccd_state.exp_current]);
            ccd_state.exp_pixels[ccd_state.exp_current - 1] = NULL;
            ccd_state.exp_pixels[ccd_state.exp_current]     = NULL;
            ccd_release(ccd_state.exposure.ccd);
            /*
             * If self guided, stop guide on current field and restart on opposite field.
             */
            if (ccd_state.guiding && ccd_state.exposure.ccd->base == ccd_state.guide.ccd->base)
            {
                ccd_state.guide.ccd = ccd_state.guide.ccd->odd_field;
                /*
                 * Move guide centroid back up by 1/4 pixel if last exposure or 1/2 pixel for next field.
                 */
                ccd_state.guide_y_centroid -= (ccd_state.exp_current < ccd_state.exp_count) ? ccd_state.guide_interleave_offset * 2 : ccd_state.guide_interleave_offset;
            }
            ccd_state.exposure.ccd = ccd_state.exposure.ccd->even_field;
            ccd_connect(ccd_state.exposure.ccd);
        }
        /*
         * Calibrate image.
         */
        if (!(flat_frame = ccd_state.flat_frame) && ccd_state.sequencing)
            flat_frame = wheel_state.flat_frame[wheel_state.current_sequence];
        if (ccd_state.bias_frame || ccd_state.dark_frame || flat_frame)
            ccd_image_calibrate(ccd_state.exposure.image, ccd_state.bias_frame, ccd_state.dark_frame, flat_frame);
        if (ccd_state.exp_comb != GCCD_NO_COMB && ccd_state.exp_count > 1)
        {
            gdk_window_set_cursor(gui_state.window->window, cursorWait);
            gdk_flush();
            /*
             * Save exposed pixels to list.
             */
            ccd_state.exp_pixels[ccd_state.exp_current - 1] = ccd_state.exposure.image->pixels;
            ccd_state.exposure.image->pixels                = NULL;
            if (ccd_state.exp_current == ccd_state.exp_count)
            {
                /*
                 * Make sure filter value is applied properly.
                 */
                if (wheel_state.wheel.fd == 0)
                    ccd_state.exposure.image->filter = CCD_COLOR_MONOCHROME;
                else
                    ccd_state.exposure.image->filter = wheel_state.filter_mask[wheel_state.filter[wheel_state.wheel.current]];
                /*
                 * Combine the exposed frames.
                 */
                if (ccd_state.exp_tracknstack)
                {
                    float x_centroid = 0.0;
                    float y_centroid = 0.0;
                    if (ccd_state.exposure.image->color != CCD_COLOR_MONOCHROME)
                    {
                        /*
                         * One-shot color cameras need to have the colors split apart before registration.
                         */
                        for (i = 0; i < ccd_state.exp_count; i++)
                        {
                            /*
                             * Split all saved frames into color frames.
                             */
                            ccd_state.exposure.image->pixels = ccd_state.exp_pixels[i];
                            ccd_state.matrix_num_frames      = ccd_image_split_frames(ccd_state.exposure.image, ccd_state.matrix_pixels[i], ccd_state.matrix_colors, 1);
                        }
                        /*
                         * Start at the last color (luminance) to get the best centroid.
                         */
                        for (i = ccd_state.matrix_num_frames - 1; i >= 0; i--)
                        {
                            /*
                             * Combine all frames of the same color.
                             */
                            for (j = 0; j < ccd_state.exp_count; j++)
                                comb_pixels[j] = ccd_state.matrix_pixels[j][i];
                            if (ccd_image_register_frames(ccd_state.exposure.image, comb_pixels, registered_pixels, &x_centroid, &y_centroid, ccd_state.exposure.image->width*ccd_state.reg_x_range/100, ccd_state.exposure.image->height*ccd_state.reg_y_range/100, ccd_state.reg_x_max_radius, ccd_state.reg_y_max_radius, ccd_state.reg_noise_sigs, ccd_state.exp_count))
                            {
                                ccd_image_combine_frames(ccd_state.exposure.image, registered_pixels, ccd_state.exp_count, ccd_state.exp_comb);
                                /*
                                 * Free up registered pixels.
                                 */
                                for (j = 0; j < ccd_state.exp_count; j++)
                                    free(registered_pixels[j]);
                            }
                            else
                            {
                                ccd_state.exposure.image->pixels = malloc(ccd_state.exposure.image->width * ccd_state.exposure.image->height * ((ccd_state.exposure.image->depth + 7) / 8));
                                memcpy(ccd_state.exposure.image->pixels, ccd_state.exp_pixels[ccd_state.exp_current - 1], ccd_state.exposure.image->width * ccd_state.exposure.image->height * ((ccd_state.exposure.image->depth + 7) / 8));
                            }
                            /*
                             * Free up pre-registered color frames.
                             */
                            for (j = 0; j < ccd_state.exp_count; j++)
                            {
                                free(ccd_state.matrix_pixels[j][i]);
                                ccd_state.matrix_pixels[j][i] = NULL;
                            }
                            /*
                             * Save it here for later.
                             */
                            ccd_state.matrix_pixels[0][i] = ccd_state.exposure.image->pixels;
                        }
                        ccd_state.exposure.image->pixels = NULL;
                    }
                    else
                    {
                        /*
                         * Register and combine saved frames.
                         */
                        if (ccd_image_register_frames(ccd_state.exposure.image, ccd_state.exp_pixels, registered_pixels, &x_centroid, &y_centroid, ccd_state.exposure.image->height*ccd_state.reg_x_range/100, ccd_state.exposure.image->height*ccd_state.reg_y_range/100, ccd_state.reg_x_max_radius, ccd_state.reg_y_max_radius, ccd_state.reg_noise_sigs, ccd_state.exp_count))
                        {
                            ccd_image_combine_frames(ccd_state.exposure.image, registered_pixels, ccd_state.exp_count, ccd_state.exp_comb);
                            /*
                             * Free up registered pixels.
                             */
                            for (j = 0; j < ccd_state.exp_count; j++)
                                free(registered_pixels[j]);
                        }
                        else
                        {
                            ccd_state.exposure.image->pixels = malloc(ccd_state.exposure.image->width * ccd_state.exposure.image->height * ((ccd_state.exposure.image->depth + 7) / 8));
                            memcpy(ccd_state.exposure.image->pixels, ccd_state.exp_pixels[ccd_state.exp_current - 1], ccd_state.exposure.image->width * ccd_state.exposure.image->height * ((ccd_state.exposure.image->depth + 7) / 8));
                        }
                    }
                }
                else
                    /*
                     * Combine saved frames.
                     */
                    ccd_image_combine_frames(ccd_state.exposure.image, ccd_state.exp_pixels, ccd_state.exp_count, ccd_state.exp_comb);
                /*
                 * Free all exposed frames.
                 */
                for (i = 0; i < ccd_state.exp_count; i++)
                {
                    free(ccd_state.exp_pixels[i]);
                    ccd_state.exp_pixels[i] = NULL;
                }
            }
            gdk_window_set_cursor(gui_state.window->window, NULL);
        }
        if (ccd_state.exp_comb == GCCD_NO_COMB || ccd_state.exp_current == ccd_state.exp_count)
        {
            time_t     now = time(NULL);
            struct tm *utc = gmtime(&now);

            /*
             * Add image.
             */
            sprintf(ccd_state.exposure.image->name, "%s-%d", ccd_state.exp_basename, ccd_state.exp_basenum++);
            if (gui_state.spin_basenum)
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(gui_state.spin_basenum), (gfloat)ccd_state.exp_basenum);
            sprintf(ccd_state.exposure.image->date, "%04d-%02d-%02d", utc->tm_year+1900, utc->tm_mon + 1, utc->tm_mday);
            sprintf(ccd_state.exposure.image->time, "%02d:%02d:%02d", utc->tm_hour, utc->tm_min, utc->tm_sec);
            if (wheel_state.wheel.fd == 0)
                ccd_state.exposure.image->filter = CCD_COLOR_MONOCHROME;
            else
                ccd_state.exposure.image->filter = wheel_state.filter_mask[wheel_state.wheel.current];
            if (ccd_state.sequencing)
            {
                strcat(ccd_state.exposure.image->name, "-");
                strcat(ccd_state.exposure.image->name, (char *)(g_list_nth(wheel_state.list_filters, wheel_state.filter[wheel_state.current_sequence])->data));
            }
            if (ccd_state.exp_comb == GCCD_NO_COMB && ccd_state.exp_count > 1 && ccd_state.exp_tracknstack)
            {
                gdk_window_set_cursor(gui_state.window->window, cursorWait);
                gdk_flush();
                /*
                 * Register new image with first image.
                 */
                if (ccd_state.exp_current == 1)
                {
                    ccd_state.track_x_centroid = ccd_state.exposure.width  / 2;
                    ccd_state.track_y_centroid = ccd_state.exposure.height / 2;
                    i                          = ccd_state.reg_x_max_radius;
                    j                          = ccd_state.reg_y_max_radius;
                    ccd_image_find_best_centroid(ccd_state.exposure.image, ccd_state.exposure.image->pixels, &ccd_state.track_y_centroid, &ccd_state.track_y_centroid, ccd_state.exposure.width*3/8, ccd_state.exposure.height*3/8, &i, &j, ccd_state.reg_noise_sigs);
                    if (ccd_state.exposure.image->color != CCD_COLOR_MONOCHROME)
                        ccd_state.matrix_num_frames = ccd_image_split_frames(ccd_state.exposure.image, ccd_state.matrix_pixels[0], ccd_state.matrix_colors, 1);
                }
                else
                {
                    float x_prev = ccd_state.track_x_centroid;
                    float y_prev = ccd_state.track_y_centroid;
                    if (ccd_state.exposure.image->color != CCD_COLOR_MONOCHROME)
                    {
                        ccd_state.matrix_num_frames = ccd_image_split_frames(ccd_state.exposure.image, ccd_state.matrix_pixels[0], ccd_state.matrix_colors, 1);
                        for (i = 0; i < ccd_state.matrix_num_frames; i++)
                        {
                            ccd_state.exp_pixels[0] = ccd_image_register_centroid(ccd_state.exposure.image, ccd_state.matrix_pixels[0][i], ccd_state.track_x_centroid, ccd_state.track_y_centroid, &x_prev, &y_prev, ccd_state.exposure.width*ccd_state.reg_x_range/100*ccd_state.exp_current, ccd_state.exposure.height*ccd_state.reg_y_range/100*ccd_state.exp_current, ccd_state.reg_x_max_radius, ccd_state.reg_y_max_radius, ccd_state.reg_noise_sigs, 1);
                            free(ccd_state.matrix_pixels[0][i]);
                            ccd_state.matrix_pixels[0][i] = ccd_state.exp_pixels[0];
                        }
                    }
                    else
                    {
                        ccd_state.exp_pixels[0] = ccd_image_register_centroid(ccd_state.exposure.image, ccd_state.exposure.image->pixels, ccd_state.track_x_centroid, ccd_state.track_y_centroid, &x_prev, &y_prev, ccd_state.exposure.width*ccd_state.reg_x_range/100*ccd_state.exp_current, ccd_state.exposure.height*ccd_state.reg_y_range/100*ccd_state.exp_current, ccd_state.reg_x_max_radius, ccd_state.reg_y_max_radius, ccd_state.reg_noise_sigs, 1);
                        free(ccd_state.exposure.image->pixels);
                        ccd_state.exposure.image->pixels = ccd_state.exp_pixels[0];
                        ccd_state.exp_pixels[0] = NULL;
                    }
                }
                gdk_window_set_cursor(gui_state.window->window, NULL);
            }
            if (ccd_state.exposure.image->color != CCD_COLOR_MONOCHROME && ccd_state.exp_count > 1 && ccd_state.exp_tracknstack)
            {
                struct ccd_image *new_image;
                /*
                 * Add the individual color frames from a one-shot camera.
                 */
                for (i = 0; i < ccd_state.matrix_num_frames; i++)
                {
                    new_image          = ccd_image_dup(ccd_state.exposure.image);
                    new_image->color   = ccd_state.matrix_colors[i];
                    new_image->pixels  = ccd_state.matrix_pixels[0][i];
                    new_image->changed = TRUE;
                    sprintf(new_image->name, "%s-%s", ccd_state.exposure.image->name, COLOR_MASK_TO_NAME(ccd_state.matrix_colors[i]));
                    if (gui_state.auto_save)
                    {
                        ccd_image_save_fits(new_image);
                        ccd_image_delete(new_image);
                    }
                    else
                    {
                        imageAdd(new_image);
                    }
                }
                /*
                 * No need for the unregistered, raw, color matrixed image.
                 */
                ccd_image_delete(ccd_state.exposure.image);
            }
            else
            {
                /*
                 * Save/Add the image.
                 */
                ccd_state.exposure.image->changed = TRUE;
                if (gui_state.auto_save)
                {
                    ccd_image_save_fits(ccd_state.exposure.image);
                    ccd_image_delete(ccd_state.exposure.image);
                }
                else
                {
                    imageAdd(ccd_state.exposure.image);
                }
            }
            ccd_state.exposure.image = NULL;
        }
    }
    if (ccd_state.exp_current < ccd_state.exp_count)
    {
        /*
         * Start next exposure.
         */
        guide_start((ccd_state.exposure.msec <= 1000) ? FALSE : ccd_state.guiding);
        ccd_expose_frame(&ccd_state.exposure);
        ccd_state.exposure.input_tag  = gdk_input_add(ccd_state.exposure.ccd->fd, GDK_INPUT_READ, (GdkInputFunction)cbReadExposure, NULL);
        ccd_state.exposure.timeout_id = gtk_timeout_add(ccd_state.exposure.msec < 25000 ? 250 : ccd_state.exposure.msec / 100, (GtkFunction)cbUpdateExposure, &ccd_state.exposure);
        if (ccd_state.exp_fields == FIELD_INTERLEAVE
         && (!ccd_state.guiding || (ccd_state.exposure.ccd->base != ccd_state.guide.ccd->base))
         && (ccd_state.exp_loadtime <= ccd_state.exposure.msec))
        {
            ccd_state.interleave.timeout_id = gtk_timeout_add(ccd_state.exp_loadtime, (GtkFunction)cbInterleaveExposure, NULL);
            ccd_connect(ccd_state.interleave.ccd);
        }
        if (ccd_state.sequencing)
            sprintf(str, "%d/%d %s", ++ccd_state.exp_current, ccd_state.exp_count, (char *)(g_list_nth(wheel_state.list_filters, wheel_state.filter[wheel_state.current_sequence])->data));
        else
            sprintf(str, "%d/%d", ++ccd_state.exp_current, ccd_state.exp_count);
        if (gui_state.status)
            gtk_label_set_text(GTK_LABEL(gui_state.status), str);
        if (verbose & 1) {g_print(str); g_print("\n");}
        if (gui_state.button_exp)
            gtk_widget_set_sensitive(GTK_WIDGET(gui_state.button_exp), TRUE);
    }
    else
    {
        ccd_release(ccd_state.exposure.ccd);
        if (ccd_state.sequencing)
            for (wheel_state.current_sequence++; wheel_state.current_sequence < MAX_FILTERS && !wheel_state.sequence[wheel_state.current_sequence]; wheel_state.current_sequence++);
        if (ccd_state.sequencing && wheel_state.current_sequence < MAX_FILTERS)
        {
            /*
             * Set filter wheel if sequencing and start next exposure.
             */
            ccd_state.exposing = EXPOSE_INIT;
            while (wheel_goto(&wheel_state.wheel, wheel_state.current_sequence + 1) < 0 && ccd_state.exposing) gtk_main_iteration();
            if (wheel_state.wheel.status != WHEEL_IDLE) gtk_label_set_text(GTK_LABEL(gui_state.wheel_status), _("Busy"));
            if (!ccd_state.exposing) return;
            while (wheel_state.wheel.status != WHEEL_IDLE && ccd_state.exposing) gtk_main_iteration();
            if (!ccd_state.exposing) return;
            exposure_start((gpointer)ccd_state.guiding);
        }
        else
        {
            exposure_stop(TRUE);
            /*
             * Beep to signal end of exposure.
             */
            if (gui_state.beep_when_done)
                gdk_beep();
        }
    }
}
static void cbReadFocus(gpointer data, gint fd, GdkInputCondition in)
{
    unsigned int   src_pixel, src_offset, x_offset, y_offset, width, height, scale;
    int            dst_rowstride, src_rowstride, src_depthbytes, x, y;
    float          contrast_scale;
    gchar         *dst_pixels, str[32];
    GdkPixbuf     *pixbuf;

    ccd_state.focus.downloading = TRUE;
    /*
     * Reset input & timer calls.
     */
    gtk_timeout_remove(ccd_state.focus.timeout_id);
    gdk_input_remove(ccd_state.focus.input_tag);
    ccd_state.focus.timeout_id = 0;
    ccd_state.focus.input_tag  = 0;
    if (gui_state.progress_bar)
    {
        gtk_progress_bar_update(GTK_PROGRESS_BAR(gui_state.progress_bar), 1.0);
        gtk_main_iteration();
    }
    while (ccd_state.focus.downloading && ccd_load_frame(&ccd_state.focus))
        /*
         * Update load progress bar every 32 scanlines.
         */
        if (!(ccd_state.focus.read_row & 0x1F) && gui_state.progress_bar)
        {
            gtk_progress_bar_update(GTK_PROGRESS_BAR(gui_state.progress_bar), 1.0 - (gfloat)(ccd_state.focus.read_row * ccd_state.focus.ybin) / (gfloat)ccd_state.focus.height);
            gtk_main_iteration();
        }
    if (gui_state.progress_bar)
    {
        gtk_progress_bar_update(GTK_PROGRESS_BAR(gui_state.progress_bar), 0.0);
        gtk_main_iteration();
    }
    /*
     * Bail out if operation canceled.
     */
    if (!ccd_state.focus.downloading)
        return;
    ccd_state.focus.downloading = FALSE;
    /*
     * Load image into displayable pixmap.
     */
    ccd_state.focus.image->pixmin = ccd_state.focus.image->pixmax = 0;
    ccd_image_histogram(ccd_state.focus.image);
    pixbuf         = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, ccd_state.focus.image->width, ccd_state.focus.image->height);
    dst_pixels     = gdk_pixbuf_get_pixels(pixbuf);
    dst_rowstride  = gdk_pixbuf_get_rowstride(pixbuf);
    src_depthbytes = (ccd_state.focus.image->depth + 7) / 8;
    src_rowstride  = ccd_state.focus.image->width * src_depthbytes;
    if (ccd_state.focus.image->pixmax == ccd_state.focus.image->pixmin)
    {
        src_offset     = ccd_state.focus.image->pixmin - 1;
        contrast_scale = 255.0 / (ccd_state.focus.image->pixmax - ccd_state.focus.image->pixmin);
    }
    else
    {
        src_offset     = ccd_state.focus.image->pixmin;
        contrast_scale = 255.0 / (ccd_state.focus.image->pixmax - ccd_state.focus.image->pixmin);
    }
    switch (src_depthbytes)
    {
        case 1:
            for (y = 0; y < ccd_state.focus.image->height; y++)
                for (x = 0; x < ccd_state.focus.image->width; x++)
                {
                    src_pixel = view_palettes[GAMMALOG1_GREY][(int)((ccd_state.focus.image->pixels[y * src_rowstride + x] - src_offset) * contrast_scale)];
                    dst_pixels[y * dst_rowstride + x * 3]     = src_pixel;
                    dst_pixels[y * dst_rowstride + x * 3 + 1] = src_pixel;
                    dst_pixels[y * dst_rowstride + x * 3 + 2] = src_pixel;
                }
            break;
        case 2:
            for (y = 0; y < ccd_state.focus.image->height; y++)
                for (x = 0; x < ccd_state.focus.image->width; x++)
                {
                    src_pixel = view_palettes[GAMMALOG1_GREY][(int)((*(unsigned short *)&ccd_state.focus.image->pixels[y * src_rowstride + x * 2] - src_offset) * contrast_scale)];
                    dst_pixels[y * dst_rowstride + x * 3]     = src_pixel;
                    dst_pixels[y * dst_rowstride + x * 3 + 1] = src_pixel;
                    dst_pixels[y * dst_rowstride + x * 3 + 2] = src_pixel;
                }
            break;
        case 4:
            for (y = 0; y < ccd_state.focus.image->height; y++)
                for (x = 0; x < ccd_state.focus.image->width; x++)
                {
                    src_pixel = view_palettes[GAMMALOG1_GREY][(int)((*(unsigned long *)&ccd_state.focus.image->pixels[y * src_rowstride + x * 4] - src_offset) * contrast_scale)];
                    dst_pixels[y * dst_rowstride + x * 3]     = src_pixel;
                    dst_pixels[y * dst_rowstride + x * 3 + 1] = src_pixel;
                    dst_pixels[y * dst_rowstride + x * 3 + 2] = src_pixel;
                }
            break;
    }
    if (ccd_state.focus_scale > FOCUS_SCALE_1_1)
    {
        GdkPixbuf *scale_pixbuf;

        scale                   = (ccd_state.focus_scale - FOCUS_SCALE_1_1) * 2;
        width                   = ccd_state.focus.image->width  * scale;
        height                  = ccd_state.focus.image->height * scale;
        x_offset                = ccd_state.focus.xoffset       * scale;
        y_offset                = ccd_state.focus.yoffset       * scale;
        ccd_state.focus.xoffset = ccd_state.focus_view_xoffset  / scale;
        ccd_state.focus.yoffset = ccd_state.focus_view_yoffset  / scale;
        scale_pixbuf            = gdk_pixbuf_scale_simple(pixbuf, width , height, GDK_INTERP_BILINEAR);
        gdk_pixbuf_unref(pixbuf);
        pixbuf = scale_pixbuf;
    }
    else
    {
        if ((scale = (FOCUS_SCALE_1_1 - ccd_state.focus_scale) * 2) == 0) scale = 1;
        width                   = ccd_state.focus.image->width;
        height                  = ccd_state.focus.image->height;
        x_offset                = ccd_state.focus.xoffset      / scale;
        y_offset                = ccd_state.focus.yoffset      / scale;
        ccd_state.focus.xoffset = ccd_state.focus_view_xoffset * scale;
        ccd_state.focus.yoffset = ccd_state.focus_view_yoffset * scale;
    }
    gdk_pixbuf_render_to_drawable(pixbuf, ccd_state.focus.image->pixmap, gui_state.window->style->fg_gc[GTK_STATE_NORMAL],
                                  0, 0, x_offset, y_offset, width, height,
                                  GDK_RGB_DITHER_MAX, 0, 0);
    gdk_pixbuf_unref(pixbuf);
    sprintf(str, "Min:%lu\nAve:%lu\nMax:%lu", ccd_state.focus.image->pixmin, ccd_state.focus.image->pixave, ccd_state.focus.image->pixmax);
    gtk_label_set_text(GTK_LABEL(gui_state.label_minmax), str);
    gtk_widget_queue_draw(gui_state.view_focus);
    gtk_widget_queue_draw(gui_state.draw_histogram);
    if (ccd_state.focusing)
    {
        //focus_start(TRUE);
    }
    else
    {
        /*
         * Stop focus mode.
         */
       if (gui_state.button_exp)
            gtk_object_set(GTK_OBJECT(gui_state.button_exp), "label", _("Begin"), NULL);
       ccd_release(ccd_state.focus.ccd);
    }
}
/*
 * Automatically find a guide star.
 */
static void cbReadGuideAutoSelect(gpointer data, gint fd, GdkInputCondition in)
{
    int x_radius, y_radius;

    /*
     * Reset input call.
     */
    gdk_input_remove(ccd_state.guide.input_tag);
    ccd_state.guide.input_tag   = 0;
    ccd_state.guide.downloading = TRUE;
    while (ccd_state.guide.downloading && ccd_load_frame(&ccd_state.guide))
        gtk_main_iteration();
    /*
     * Bail out if operation canceled.
     */
    if (!ccd_state.guide.downloading)
        return;
    /*
     * Look for a guide star.
     */
    ccd_state.guide.downloading = FALSE;
    ccd_state.guide_x_centroid  = ccd_state.guide.image->width  / 2;
    ccd_state.guide_y_centroid  = ccd_state.guide.image->height / 2;
    x_radius                    = ccd_state.reg_x_max_radius;
    y_radius                    = ccd_state.reg_x_max_radius;
    if (!ccd_image_find_best_centroid(ccd_state.guide.image, ccd_state.guide.image->pixels, &ccd_state.guide_x_centroid, &ccd_state.guide_y_centroid, (ccd_state.guide.image->width-GUIDE_WIDTH)/2, (ccd_state.guide.image->height-GUIDE_HEIGHT)/2, &x_radius, &y_radius, ccd_state.reg_noise_sigs * 2)
     && !ccd_state.guide_dark)
    {
        gnome_warning_dialog_parented(_("Unable to find suitable guide star."), GTK_WINDOW(gui_state.window));
        /*
         * Reset load progress bar and begin/cancel button.
         */
        exposure_stop(TRUE);
        ccd_state.exp_current      = 0;
        ccd_state.exposing         = FALSE;
        ccd_state.guide.xoffset    = 0;
        ccd_state.guide.yoffset    = 0;
        ccd_state.guide_x_centroid = 0.0;
        ccd_state.guide_y_centroid = 0.0;
    }
    else
    {
        ccd_state.guide_x_centroid *= ccd_state.guide.xbin;
        ccd_state.guide_y_centroid *= ccd_state.guide.ybin;
        ccd_state.guide.xoffset     = ccd_state.guide_x_centroid - GUIDE_WIDTH  / 2 + 0.5;
        ccd_state.guide.yoffset     = ccd_state.guide_y_centroid - GUIDE_HEIGHT / 2 + 0.5;
        ccd_state.guide_x_centroid  = GUIDE_WIDTH  / 2;
        ccd_state.guide_y_centroid  = GUIDE_HEIGHT / 2;
    }
    /*
     * Reset exposure options.
     */
    free(ccd_state.guide.image->pixels);
    ccd_state.guide.image->pixels = NULL;
    ccd_state.guide.dac_bits      = ccd_state.guide.ccd->dac_bits;
    ccd_state.guide.width         = GUIDE_WIDTH;
    ccd_state.guide.height        = GUIDE_HEIGHT;
    ccd_state.guide.xbin          = 1;
    ccd_state.guide.ybin          = 1;
    ccd_state.guide.msec          = ccd_state.guide_msec;
    ccd_state.guide.image->width  = ccd_state.guide.width  / ccd_state.guide.xbin;
    ccd_state.guide.image->height = ccd_state.guide.height / ccd_state.guide.ybin;
    ccd_state.guide.image->depth  = ccd_state.guide.ccd->depth;
    if (ccd_state.exposing && ccd_state.guiding)
    {
        if (ccd_state.guiding & GUIDE_ACQUIRE)
        {
            ccd_expose_frame(&ccd_state.guide);
            ccd_state.guide.input_tag = gdk_input_add(ccd_state.guide.ccd->fd, GDK_INPUT_READ, (GdkInputFunction)cbReadGuide, NULL);
        }
        else
            exposure_start((gpointer)TRUE);
    }
}
/*
 * Load image for user selection of guide object.
 */
static void cbReadGuideSelect(gpointer data, gint fd, GdkInputCondition in)
{
    unsigned int   src_pixel, src_offset;
    int            dst_rowstride, src_rowstride, src_depthbytes, x, y;
    float          contrast_scale;
    gchar         *dst_pixels;
    GdkPixbuf     *pixbuf;
    GtkWidget     **widget_list = (GtkWidget **)data;

    /*
     * Reset input call.
     */
    gdk_input_remove(ccd_state.guide.input_tag);
    ccd_state.guide.input_tag = 0;
    ccd_state.guide.downloading = TRUE;
    while (ccd_state.guide.downloading && ccd_load_frame(&ccd_state.guide))
        gtk_main_iteration();
    /*
     * Bail out if operation canceled.
     */
    if (!ccd_state.guide.downloading)
    {
        gdk_window_set_cursor(widget_list[0]->window, NULL);
        return;
    }
    ccd_state.guide.downloading = FALSE;
    ccd_release(ccd_state.guide.ccd);
    /*
     * Load image into displayable pixmap.
     */
    gtk_window_set_title(GTK_WINDOW(widget_list[0]), _("Select Guide Object"));
    ccd_state.guide.image->pixmin = ccd_state.guide.image->pixmax = 0;
    ccd_image_histogram(ccd_state.guide.image);
    pixbuf         = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, ccd_state.guide.image->width, ccd_state.guide.image->height);
    dst_pixels     = gdk_pixbuf_get_pixels(pixbuf);
    dst_rowstride  = gdk_pixbuf_get_rowstride(pixbuf);
    src_depthbytes = (ccd_state.guide.image->depth + 7) / 8;
    src_rowstride  = ccd_state.guide.image->width * src_depthbytes;
    if (ccd_state.guide.image->pixmax == ccd_state.guide.image->pixmin)
    {
        src_offset     = ccd_state.guide.image->pixmin - 1;
        contrast_scale = 255.0 / (ccd_state.guide.image->pixmax - ccd_state.guide.image->pixmin);
    }
    else
    {
        src_offset     = ccd_state.guide.image->pixmin;
        contrast_scale = 255.0 / (ccd_state.guide.image->pixmax - ccd_state.guide.image->pixmin);
    }
    switch (src_depthbytes)
    {
        case 1:
            for (y = 0; y < ccd_state.guide.image->height; y++)
                for (x = 0; x < ccd_state.guide.image->width; x++)
                {
                    src_pixel = view_palettes[GAMMALOG1_GREY][(int)((ccd_state.guide.image->pixels[y * src_rowstride + x] - src_offset) * contrast_scale)];
                    dst_pixels[y * dst_rowstride + x * 3]     = src_pixel;
                    dst_pixels[y * dst_rowstride + x * 3 + 1] = src_pixel;
                    dst_pixels[y * dst_rowstride + x * 3 + 2] = src_pixel;
                }
            break;
        case 2:
            for (y = 0; y < ccd_state.guide.image->height; y++)
                for (x = 0; x < ccd_state.guide.image->width; x++)
                {
                    src_pixel = view_palettes[GAMMALOG1_GREY][(int)((*(unsigned short *)&ccd_state.guide.image->pixels[y * src_rowstride + x * 2] - src_offset) * contrast_scale)];
                    dst_pixels[y * dst_rowstride + x * 3]     = src_pixel;
                    dst_pixels[y * dst_rowstride + x * 3 + 1] = src_pixel;
                    dst_pixels[y * dst_rowstride + x * 3 + 2] = src_pixel;
                }
            break;
        case 4:
            for (y = 0; y < ccd_state.guide.image->height; y++)
                for (x = 0; x < ccd_state.guide.image->width; x++)
                {
                    src_pixel = view_palettes[GAMMALOG1_GREY][(int)((*(unsigned long *)&ccd_state.guide.image->pixels[y * src_rowstride + x * 4] - src_offset) * contrast_scale)];
                    dst_pixels[y * dst_rowstride + x * 3]     = src_pixel;
                    dst_pixels[y * dst_rowstride + x * 3 + 1] = src_pixel;
                    dst_pixels[y * dst_rowstride + x * 3 + 2] = src_pixel;
                }
            break;
    }
    dst_pixels    = gdk_pixbuf_get_pixels(pixbuf);
    dst_rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    gdk_pixbuf_render_to_drawable(pixbuf, ccd_state.guide.image->pixmap, gui_state.window->style->fg_gc[GTK_STATE_NORMAL],
                                  0, 0, 0, 0, ccd_state.guide.image->width, ccd_state.guide.image->height,
                                  GDK_RGB_DITHER_MAX, 0, 0);
    gdk_pixbuf_unref(pixbuf);
    gtk_widget_set_sensitive(widget_list[1], TRUE);
    gtk_widget_set_sensitive(widget_list[2], TRUE);
    gtk_widget_queue_draw(widget_list[3]);
    gdk_window_set_cursor(widget_list[0]->window, NULL);
}
static void cbReadGuide(gpointer data, gint fd, GdkInputCondition in)
{
    static int     lost_count = 0;
    unsigned int   src_pixel, src_offset;
    int            dst_rowstride, src_rowstride, src_depthbytes, x, y, x_radius, y_radius;
    float          contrast_scale, x_centroid, y_centroid;
    gchar         *dst_pixels, str[80];
    GdkPixbuf     *pixbuf, *scale_pixbuf;

    /*
     * Reset input & timer calls.
     */
    if (ccd_state.guide.timeout_id)
    {
        gtk_timeout_remove(ccd_state.guide.timeout_id);
        scope_update(SCOPE_STOP, FALSE);
        ccd_state.guide.timeout_id = 0;
    }
    gdk_input_remove(ccd_state.guide.input_tag);
    ccd_state.guide.input_tag   = 0;
    ccd_state.guide.downloading = TRUE;
    while (ccd_state.guide.downloading && ccd_load_frame(&ccd_state.guide))
        gtk_main_iteration();
    /*
     * Bail out if operation canceled.
     */
    if (!ccd_state.guide.downloading)
        return;
    ccd_state.guide.downloading = FALSE;
    /*
     * Apply corrections to telescope.
     */
    x_centroid = GUIDE_WIDTH  / 2;
    y_centroid = GUIDE_HEIGHT / 2;
    x_radius   = ccd_state.reg_x_max_radius;
    y_radius   = ccd_state.reg_y_max_radius;
    if (!ccd_image_find_best_centroid(ccd_state.guide.image, ccd_state.guide.image->pixels, &x_centroid, &y_centroid, ccd_state.guide.image->width/2, ccd_state.guide.image->height/2, &x_radius, &y_radius, ccd_state.reg_noise_sigs)
     && !ccd_state.guide_dark)
    {
        /*
         * Wait for two consecutive lost frames before bailing.
         */
        if (++lost_count > 1)
        {
            /*
             * Lost star.
             */
            if (verbose & 1) g_print("Guide star lost\n");
            if ((ccd_state.guiding & GUIDE_ACQUIRE)
             && ((ccd_state.exp_fields < FIELD_SUM) || (ccd_state.exposure.ccd == ccd_state.exposure.ccd->even_field)))
            {
                /*
                 * Look for a new guide star.
                 */
                ccd_state.guide.dac_bits = ccd_state.guide.ccd->dac_bits / 2;
                ccd_state.guide.width    = ccd_state.guide.ccd->width;
                ccd_state.guide.height   = ccd_state.guide.ccd->height;
                ccd_state.guide.xoffset  = 0;
                ccd_state.guide.yoffset  = 0;
                ccd_state.guide.xbin     = GUIDE_SELECT_BIN;
                ccd_state.guide.ybin     = GUIDE_SELECT_BIN;
                ccd_state.guide.msec     = ccd_state.guide_msec / (GUIDE_SELECT_BIN * GUIDE_SELECT_BIN);
                free(ccd_state.guide.image->pixels);
                ccd_state.guide.image->pixels = NULL;
                ccd_state.guide.image->width  = ccd_state.guide.width  / ccd_state.guide.xbin;
                ccd_state.guide.image->height = ccd_state.guide.height / ccd_state.guide.ybin;
                ccd_state.guide.image->depth  = ccd_state.guide.ccd->dac_bits;
                ccd_expose_frame(&ccd_state.guide);
                ccd_state.guide.input_tag = gdk_input_add(ccd_state.guide.ccd->fd, GDK_INPUT_READ, (GdkInputFunction)cbReadGuideAutoSelect, NULL);
                ccd_state.guide.msec = ccd_state.guide_msec;
                if (gui_state.status)
                    gtk_label_set_text(GTK_LABEL(gui_state.status), _("Find New Guide Star..."));
                if (verbose & 1) g_print("Find New Guide Star...\n");
            }
            else
            {
                /*
                 * Cancel exposure.
                 */
                exposure_stop(TRUE);
                /*
                 * Message box notification.
                 */
                gnome_warning_dialog_parented(_("Guide star lost.  Select a new guide object or clear current object before restarting exposure"), GTK_WINDOW(gnome_mdi_get_active_window(GNOME_MDI(mdi))));
            }
            lost_count = 0;
            return;
        }
        x_centroid = ccd_state.guide_x_centroid;
        y_centroid = ccd_state.guide_y_centroid;
    }
    else
        lost_count = 0;
    x_centroid -= ccd_state.guide_x_centroid;
    y_centroid -= ccd_state.guide_y_centroid;
    ccd_state.guide_dx_centroid = x_centroid;
    ccd_state.guide_dy_centroid = y_centroid;
    /*
     * Notify when star acquired.
     */
    if (ccd_state.guiding & GUIDE_ACQUIRE)
    {
        if (fabs(x_centroid) < ccd_state.guide_min_offset
         && fabs(y_centroid) < ccd_state.guide_min_offset)
        {
            if (++ccd_state.guide_acquire_count >= GUIDE_ACQUIRE_COUNT)
                ccd_state.guiding &= ~GUIDE_ACQUIRE;
        }
        else
            ccd_state.guide_acquire_count = 0;
    }
    /*
     * Update centroid offset.
     */
    sprintf(str, _("Offset (%.2f, %.2f)"), ccd_state.guide_dx_centroid, ccd_state.guide_dy_centroid);
    if (verbose & 2) g_print("%.2f,\t%.2f\n", ccd_state.guide_dx_centroid, ccd_state.guide_dy_centroid);
    /*
     * Send commands to telescope.
     */
    if (!ccd_state.guide_dark)
        cbUpdateGuide(0);
    /*
     * Load image into displayable pixmap.
     */
    if (gui_state.current_notebook_page == 2)
    {
        ccd_state.guide.image->pixmin = ccd_state.guide.image->pixmax = 0;
        ccd_image_histogram(ccd_state.guide.image);
        pixbuf         = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, ccd_state.guide.image->width, ccd_state.guide.image->height);
        dst_pixels     = gdk_pixbuf_get_pixels(pixbuf);
        dst_rowstride  = gdk_pixbuf_get_rowstride(pixbuf);
        src_depthbytes = (ccd_state.guide.image->depth + 7) / 8;
        src_rowstride  = ccd_state.guide.image->width * src_depthbytes;
        if (ccd_state.guide.image->pixmax == ccd_state.guide.image->pixmin)
        {
            src_offset     = ccd_state.guide.image->pixmin - 1;
            contrast_scale = 255.0 / (ccd_state.guide.image->pixmax - ccd_state.guide.image->pixmin);
        }
        else
        {
            src_offset     = ccd_state.guide.image->pixmin;
            contrast_scale = 255.0 / (ccd_state.guide.image->pixmax - ccd_state.guide.image->pixmin);
        }
        switch (src_depthbytes)
        {
            case 1:
                for (y = 0; y < ccd_state.guide.image->height; y++)
                    for (x = 0; x < ccd_state.guide.image->width; x++)
                    {
                        src_pixel = ccd_state.guide_dark ? 0 - ((x & 1) ^ (y & 1)) : view_palettes[LINEAR_GREY][(int)((ccd_state.guide.image->pixels[y * src_rowstride + x] - src_offset) * contrast_scale)];
                        dst_pixels[y * dst_rowstride + x * 3]     = src_pixel;
                        dst_pixels[y * dst_rowstride + x * 3 + 1] = src_pixel;
                        dst_pixels[y * dst_rowstride + x * 3 + 2] = src_pixel;
                    }
                break;
            case 2:
                for (y = 0; y < ccd_state.guide.image->height; y++)
                    for (x = 0; x < ccd_state.guide.image->width; x++)
                    {
                        src_pixel = ccd_state.guide_dark ? 0 - ((x & 1) ^ (y & 1)) : view_palettes[LINEAR_GREY][(int)((*(unsigned short *)&ccd_state.guide.image->pixels[y * src_rowstride + x * 2] - src_offset) * contrast_scale)];
                        dst_pixels[y * dst_rowstride + x * 3]     = src_pixel;
                        dst_pixels[y * dst_rowstride + x * 3 + 1] = src_pixel;
                        dst_pixels[y * dst_rowstride + x * 3 + 2] = src_pixel;
                    }
                break;
            case 4:
                for (y = 0; y < ccd_state.guide.image->height; y++)
                    for (x = 0; x < ccd_state.guide.image->width; x++)
                    {
                        src_pixel = ccd_state.guide_dark ? 0 - ((x & 1) ^ (y & 1)) : view_palettes[LINEAR_GREY][(int)((*(unsigned long *)&ccd_state.guide.image->pixels[y * src_rowstride + x * 4] - src_offset) * contrast_scale)];
                        dst_pixels[y * dst_rowstride + x * 3]     = src_pixel;
                        dst_pixels[y * dst_rowstride + x * 3 + 1] = src_pixel;
                        dst_pixels[y * dst_rowstride + x * 3 + 2] = src_pixel;
                    }
                break;
        }
        scale_pixbuf  = gdk_pixbuf_scale_simple(pixbuf, GUIDE_WIDTH*GUIDE_SCALE, GUIDE_HEIGHT*GUIDE_SCALE, GDK_INTERP_BILINEAR);
        dst_pixels    = gdk_pixbuf_get_pixels(scale_pixbuf);
        dst_rowstride = gdk_pixbuf_get_rowstride(scale_pixbuf);
        /*
         * Draw red crosshairs.
         */
        y = GUIDE_HEIGHT*GUIDE_SCALE/2;
        for (x = 0; x < GUIDE_WIDTH*GUIDE_SCALE; x++)
        {
            dst_pixels[y * dst_rowstride + x * 3]     = 0xFF;
            dst_pixels[y * dst_rowstride + x * 3 + 1] = 0x00;
            dst_pixels[y * dst_rowstride + x * 3 + 2] = 0x00;
        }
        x = GUIDE_WIDTH*GUIDE_SCALE/2;
        for (y = 0; y < GUIDE_HEIGHT*GUIDE_SCALE; y++)
        {
            dst_pixels[y * dst_rowstride + x * 3]     = 0xFF;
            dst_pixels[y * dst_rowstride + x * 3 + 1] = 0x00;
            dst_pixels[y * dst_rowstride + x * 3 + 2] = 0x00;
        }
        gdk_pixbuf_render_to_drawable(scale_pixbuf, ccd_state.guide.image->pixmap, gui_state.window->style->fg_gc[GTK_STATE_NORMAL],
                                      0, 0, 0, 0, GUIDE_WIDTH*GUIDE_SCALE, GUIDE_HEIGHT*GUIDE_SCALE,
                                      GDK_RGB_DITHER_MAX, 0, 0);
        gdk_pixbuf_unref(pixbuf);
        gdk_pixbuf_unref(scale_pixbuf);
        gtk_label_set_text(GTK_LABEL(gui_state.label_offset), str);
        gtk_widget_queue_draw(gui_state.view_guide);
    }
    /*
     * Start next exposure.
     */
    ccd_expose_frame(&ccd_state.guide);
    ccd_state.guide.input_tag = gdk_input_add(ccd_state.guide.ccd->fd, GDK_INPUT_READ, (GdkInputFunction)cbReadGuide, NULL);
}

/***************************************************************************\
*                                                                           *
*                             UI callbacks                                  *
*                                                                           *
\***************************************************************************/

/*
 * Common widgets.
 */
static void cbSwitchPage(GtkWidget *widget, gpointer data1, guint page, gpointer data2)
{
    if ((gui_state.current_notebook_page = page) != 1 && ccd_state.focusing)
        focus_stop(TRUE);
}
static void cbDeviceChanged(GtkWidget *widget, gpointer data)
{
    struct ccd_dev *old_ccd = ccd_state.exposure.ccd;

    if (old_ccd != (ccd_state.exposure.ccd = ccd_find_by_name(gtk_entry_get_text(GTK_ENTRY(widget)))))
    {
        ccd_state.exposure.dac_bits = ccd_state.exposure.ccd->dac_bits;
        ccd_state.exposure.width    = ccd_state.exposure.ccd->width;
        ccd_state.exposure.height   = ccd_state.exposure.ccd->height;
        ccd_state.exposure.xoffset  = 0;
        ccd_state.exposure.yoffset  = 0;
        ccd_state.focus.ccd         = ccd_state.exposure.ccd;
        ccd_state.focus.dac_bits    = ccd_state.focus.ccd->dac_bits / 2;
        ccd_state.focus.width       = ccd_state.focus.ccd->width;
        ccd_state.focus.height      = ccd_state.focus.ccd->height;
        ccd_state.focus.xoffset     = 0;
        ccd_state.focus.yoffset     = 0;
        ccd_state.exp_loadtime      = 0;
    }
}
static void cbTimeChanged(GtkWidget *widget, gpointer data)
{
    struct ccd_exp *exposure = (struct ccd_exp *)data;
    exposure->time_count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}
static void cbTimeScaleChanged(GtkWidget *widget, gpointer data)
{
    struct ccd_exp *exposure = (struct ccd_exp *)data;
    gchar          *scale    = gtk_entry_get_text(GTK_ENTRY(widget));

    if (!strcmp(scale, _("minute")))
        exposure->time_scale = 60000;
    else if (!strcmp(scale, _("second")))
        exposure->time_scale = 1000;
    else /*if (!strcmp(scale, _("msec")))*/
        exposure->time_scale = 1;
}
static void cbBeepDone(GtkWidget *widget, gpointer data)
{
    gui_state.beep_when_done = GTK_TOGGLE_BUTTON(widget)->active;
}
/*
 * Begin or cancel capture.
 */
static void cbAcquireImage(GtkWidget *widget, gpointer data)
{
    int   i;
    gchar str[80];

    if (ccd_state.exposing || ccd_state.focusing)
    {
        if (ccd_state.focusing)
            /*
             * Cancel focus/find mode.
             */
            focus_stop(TRUE);
        else
            /*
             * Cancel the exposure request.
             */
            exposure_stop(TRUE);
    }
    else
    {
        if (gui_state.current_notebook_page == 1)
        {
            /*
             * Begin focus/find mode.
             */
            focus_start(TRUE);
            strcpy(str, _("Focus/Find"));
            gtk_label_set_text(GTK_LABEL(gui_state.status), str);
            if (verbose & 1) {g_print(str); g_print("\n");}
        }
        else
        {
            /*
             * Begin the exposure.
             */
            if (ccd_state.guiding && ccd_state.guide.ccd->base == ccd_state.exposure.ccd->base)
            {
                if (ccd_state.guide_fields == FIELD_BOTH)
                    ccd_state.exp_fields = ccd_state.bin_fields == FIELD_BOTH ? FIELD_SUM : FIELD_INTERLEAVE;
                else
                    ccd_state.exp_fields = ccd_state.guide_fields;
            }
            else if (ccd_state.exposure.ccd->fields > 1)
                ccd_state.exp_fields = ccd_state.bin_fields;
            else
                ccd_state.exp_fields = FIELD_BOTH;
            if (ccd_state.sequencing)
                for (wheel_state.current_sequence = 0; wheel_state.current_sequence < MAX_FILTERS && !wheel_state.sequence[wheel_state.current_sequence]; wheel_state.current_sequence++);
            if (ccd_state.bias_frame
             && (ccd_state.bias_frame->height != ccd_state.exposure.height / ccd_state.exposure.ybin * (ccd_state.exp_fields == CCD_IMAGE_COMBINE_INTERLEAVE ? 2 : 1)
              || ccd_state.bias_frame->width  != ccd_state.exposure.width  / ccd_state.exposure.xbin
              || ccd_state.bias_frame->depth  != ccd_state.exposure.ccd->depth))
            {
                gnome_warning_dialog_parented(_("Bias frame must match image dimensions"), GTK_WINDOW(gui_state.window));
                return;
            }
            if (ccd_state.dark_frame
             && (ccd_state.dark_frame->height != ccd_state.exposure.height / ccd_state.exposure.ybin * (ccd_state.exp_fields == CCD_IMAGE_COMBINE_INTERLEAVE ? 2 : 1)
              || ccd_state.dark_frame->width  != ccd_state.exposure.width  / ccd_state.exposure.xbin
              || ccd_state.dark_frame->depth  != ccd_state.exposure.ccd->depth))
            {
                gnome_warning_dialog_parented(_("Dark frame must match image dimensions"), GTK_WINDOW(gui_state.window));
                return;
            }
            if (ccd_state.flat_frame
             && (ccd_state.flat_frame->height != ccd_state.exposure.height / ccd_state.exposure.ybin * (ccd_state.exp_fields == CCD_IMAGE_COMBINE_INTERLEAVE ? 2 : 1)
              || ccd_state.flat_frame->width  != ccd_state.exposure.width  / ccd_state.exposure.xbin
              || ccd_state.flat_frame->depth  != ccd_state.exposure.ccd->depth))
            {
                gnome_warning_dialog_parented(_("Flat frame must match image dimensions"), GTK_WINDOW(gui_state.window));
                return;
            }
            else if (ccd_state.sequencing)
            {
                i = wheel_state.current_sequence;
                while (i < MAX_FILTERS)
                {
                    if (wheel_state.flat_frame[i]
                     && (wheel_state.flat_frame[i]->height != ccd_state.exposure.height / ccd_state.exposure.ybin * (ccd_state.exp_fields == CCD_IMAGE_COMBINE_INTERLEAVE ? 2 : 1)
                      || wheel_state.flat_frame[i]->width  != ccd_state.exposure.width  / ccd_state.exposure.xbin
                      || wheel_state.flat_frame[i]->depth  != ccd_state.exposure.ccd->depth))
                    {
                        gnome_warning_dialog_parented(_("Filter flat frame must match image dimensions"), GTK_WINDOW(gui_state.window));
                        return;
                    }
                    for (i++ ;i < MAX_FILTERS && !wheel_state.sequence[i]; i++);
                }
            }
            /*
             * Do all pre-exposure stuff.
             */
            ccd_state.exposing = EXPOSE_INIT;
            /*
             * Measure frame download time if required.
             */
            if (ccd_state.exp_fields == FIELD_INTERLEAVE
             && (!ccd_state.guiding || (ccd_state.exposure.ccd->base != ccd_state.guide.ccd->base))
             && ccd_state.exp_loadtime == 0)
            {
                int            start, stop, i;
                struct timeval now;

                ccd_state.interleave           = ccd_state.exposure;
                ccd_state.interleave.image     = NULL;
                ccd_state.interleave.ccd       = ccd_state.exposure.ccd->base;
                ccd_state.interleave.input_tag = 0;
                ccd_state.interleave.yoffset   = 0;
                ccd_state.interleave.height    = ccd_state.interleave.ybin * 8;
                ccd_state.interleave.msec      = 0;
                ccd_connect(ccd_state.interleave.ccd);
                ccd_expose_frame(&ccd_state.interleave);
                gettimeofday(&now, NULL);
                start = now.tv_sec * 1000 + now.tv_usec / 1000;
                for (i = 0; i < 8; i++)
                    ccd_load_frame(&ccd_state.interleave);
                gettimeofday(&now, NULL);
                stop = now.tv_sec * 1000 + now.tv_usec / 1000;
                ccd_release(ccd_state.interleave.ccd);
                if (start == stop)
                    stop = start + 1;
                ccd_state.exp_loadtime = (stop - start) * (ccd_state.exposure.height / ccd_state.exposure.ybin) / 8;
                ccd_image_delete(ccd_state.interleave.image);
                ccd_state.interleave.image = NULL;
            }
            /*
             * Set filter wheel if sequencing.
             */
            if (ccd_state.sequencing && wheel_state.current_sequence < MAX_FILTERS)
            {
                while (wheel_goto(&wheel_state.wheel, wheel_state.current_sequence + 1) < 0 && ccd_state.exposing) gtk_main_iteration();
                if (wheel_state.wheel.status != WHEEL_IDLE) gtk_label_set_text(GTK_LABEL(gui_state.wheel_status), _("Busy"));
                if (!ccd_state.exposing) return;
                while (wheel_state.wheel.status != WHEEL_IDLE && ccd_state.exposing) gtk_main_iteration();
                if (!ccd_state.exposing) return;
            }
            /*
             * Begin guiding if enabled.
             */
            if (ccd_state.guiding)
            {
                if ((ccd_state.exposure.ccd->fields < 2) && (ccd_state.exposure.ccd->base == ccd_state.guide.ccd->base))
                {
                    gnome_warning_dialog_parented(_("Unable to self-guide the selected camera."), GTK_WINDOW(gui_state.window));
                    ccd_state.exposing = FALSE;
                    return;
                }
                ccd_state.exposure.ccd = ccd_state.exposure.ccd->base;
                ccd_state.guide.ccd    = ccd_state.guide.ccd->base;
                ccd_state.guide.flags  = 0;
                if (ccd_state.guide_x_centroid <= 0.0 && ccd_state.guide_y_centroid <= 0.0)
                {
                    ccd_state.guide.dac_bits = ccd_state.guide.ccd->dac_bits / 2;
                    ccd_state.guide.width    = ccd_state.guide.ccd->width;
                    ccd_state.guide.height   = ccd_state.guide.ccd->height;
                    ccd_state.guide.xoffset  = 0;
                    ccd_state.guide.yoffset  = 0;
                    ccd_state.guide.xbin     = GUIDE_SELECT_BIN;
                    ccd_state.guide.ybin     = GUIDE_SELECT_BIN;
                    ccd_state.guide.msec     = ccd_state.guide_msec / 2;
                    if (ccd_state.guide.image)
                    {
                        if (ccd_state.guide.image->pixels)
                            free(ccd_state.guide.image->pixels);
                        ccd_state.guide.image->pixels = NULL;
                    }
                    else
                    {
                        ccd_state.guide.image = malloc(sizeof(struct ccd_image));
                        memset(ccd_state.guide.image, 0, sizeof(struct ccd_image));
                    }
                    ccd_state.guide.image->width  = ccd_state.guide.width  / ccd_state.guide.xbin;
                    ccd_state.guide.image->height = ccd_state.guide.height / ccd_state.guide.ybin;
                    ccd_state.guide.image->depth  = ccd_state.guide.ccd->dac_bits;
                    if (ccd_connect(ccd_state.guide.ccd))
                    {
                        ccd_expose_frame(&ccd_state.guide);
                        ccd_state.guide.input_tag = gdk_input_add(ccd_state.guide.ccd->fd, GDK_INPUT_READ, (GdkInputFunction)cbReadGuideAutoSelect, NULL);
                        ccd_state.guide.msec = ccd_state.guide_msec;
                    }
                    else
                    {
                        gnome_warning_dialog_parented(_("Unable to connect to guide camera."), GTK_WINDOW(gui_state.window));
                        ccd_state.guide.msec = ccd_state.guide_msec;
                        return;
                    }
                    strcpy(str, _("Find Guide Star..."));
                    gtk_label_set_text(GTK_LABEL(gui_state.status), str);
                    if (verbose & 1) {g_print(str); g_print("\n");}
                }
                else
                {
                    gtk_idle_add(exposure_start, (gpointer)TRUE);
                }
            }
            else
            {
                ccd_state.exposure.ccd = ccd_state.exposure.ccd->base;
                exposure_start((gpointer)FALSE);
            }
            gtk_widget_set_sensitive(gui_state.notebook, FALSE);
        }
        gtk_object_set(GTK_OBJECT(widget), "label", _("Cancel"), NULL);
    }
}
static void cbHide(GtkWidget *widget, gpointer data)
{
    gtk_widget_hide(gui_state.window);
}
static void cbScopeMode(GtkWidget *widget, gpointer data)
{
#if 1
    gchar *iface = gtk_entry_get_text(GTK_ENTRY(widget));
    if (!strcmp(iface, _("LX 200")))
        scope_state.iface = SCOPE_LX200;
    else if (!strcmp(iface, _("STAR 2000")))
        scope_state.iface  = SCOPE_STAR2K;
    else /*if (!strcmp(iface, _("Manual")))*/
        scope_state.iface  = SCOPE_MANUAL;
    if (scope_state.iface != SCOPE_MANUAL)
    {
        if (scope_connect(&scope_state) < 0)
            gnome_warning_dialog_parented(_("Unable to connect to telescope."), GTK_WINDOW(gui_state.window));
        scope_release(&scope_state);
    }
    if (gui_state.focus_control)
        gtk_widget_set_sensitive(gui_state.focus_control, scope_state.iface == SCOPE_LX200);
#else
    if (GTK_TOGGLE_BUTTON(widget)->active && scope_state.iface != (guint)data)
    {
        scope_state.iface = (guint)data;
        if (scope_state.iface != SCOPE_MANUAL)
        {
            if (scope_connect(&scope_state) < 0)
                gnome_warning_dialog_parented(_("Unable to connect to telescope."), GTK_WINDOW(gui_state.window));
            scope_release(&scope_state);
        }
        gtk_widget_set_sensitive(gui_state.focus_control, scope_state.iface == SCOPE_LX200);
    }
#endif
}
static void cbScopeOptions(GtkWidget *widget, gpointer data)
{
    if (GTK_TOGGLE_BUTTON(widget)->active)
        scope_state.flags |= (guint)data;
    else
        scope_state.flags &= ~(guint)data;
}
static gboolean cbScopeButton(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    if (event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE)
    {
        if (!(ccd_state.guiding && ccd_state.exposing) && !scope_state.fd && event->type == GDK_BUTTON_PRESS)
            while (scope_connect(&scope_state) < 0)
                gnome_warning_dialog_parented(_("Telescope busy.  Wait for GOTO to complete."), GTK_WINDOW(gui_state.window));
        scope_update((guint)data, event->type == GDK_BUTTON_PRESS);
        if (!(ccd_state.guiding && ccd_state.exposing) && scope_state.fd && event->type == GDK_BUTTON_RELEASE)
            scope_release(&scope_state);
        /*
         * Clear current guide star.
         */
        ccd_state.guide_x_centroid = 0.0;
        ccd_state.guide_y_centroid = 0.0;
    }
    return (TRUE);
}
/*
 * Exposure options.
 */
static void cbXBinMode(GtkWidget *widget, gpointer data)
{
    if (GTK_TOGGLE_BUTTON(widget)->active)
        ccd_state.exposure.xbin = (guint)data;
    ccd_state.exp_loadtime = 0;
}
static void cbInterleaveBinMode(GtkWidget *widget, gpointer data)
{
    if (GTK_TOGGLE_BUTTON(widget)->active)
    {
        ccd_state.exposure.ybin = 1;
        ccd_state.bin_fields    = FIELD_INTERLEAVE;
    }
    else
    {
        ccd_state.bin_fields    = FIELD_BOTH;
    }
    ccd_state.exp_loadtime = 0;
}
static void cbYBinMode(GtkWidget *widget, gpointer data)
{
    if (GTK_TOGGLE_BUTTON(widget)->active)
        ccd_state.exposure.ybin = (guint)data;
}
static void cbCalibrationChanged(GtkWidget *widget, gpointer data)
{
    struct ccd_image *frame = ccd_image_find_by_name(gtk_entry_get_text(GTK_ENTRY(widget)));

    switch ((guint)data)
    {
        case GCCD_SET_BIAS:
            ccd_state.bias_frame = frame;
            break;
        case GCCD_SET_DARK:
            ccd_state.dark_frame = frame;
            break;
        case GCCD_SET_FLAT:
            ccd_state.flat_frame = frame;
            break;
    }
}
static void cbBaseNameChanged(GtkWidget *widget, gpointer data)
{
    strcpy(ccd_state.exp_basename, gtk_entry_get_text(GTK_ENTRY(widget)));
}
static void cbBaseNumChanged(GtkWidget *widget, gpointer data)
{
    ccd_state.exp_basenum = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}
static void cbSaveMode(GtkWidget *widget, gpointer data)
{
    gui_state.auto_save = GTK_TOGGLE_BUTTON(widget)->active;
}
static void cbExpCountChanged(GtkWidget *widget, gpointer data)
{
    gchar str[16];

    ccd_state.exp_count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
    sprintf(str, "0/%d", ccd_state.exp_count);
    gtk_label_set_text(GTK_LABEL(gui_state.status), str);
    if (verbose & 1) {g_print(str); g_print("\n");}
}
static void cbCombMode(GtkWidget *widget, gpointer data)
{
    if (GTK_TOGGLE_BUTTON(widget)->active)
        ccd_state.exp_comb = (guint)data;
}
static void cbTrackMode(GtkWidget *widget, gpointer data)
{
    ccd_state.exp_tracknstack = GTK_TOGGLE_BUTTON(widget)->active;
}
/*
 * Focus and find.
 */
static void cbFocusExpose(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    GtkAdjustment *hadj, *vadj;
    int save_state = focus_stop(FALSE);
    hadj = gtk_viewport_get_hadjustment(GTK_VIEWPORT(widget->parent));
    vadj = gtk_viewport_get_vadjustment(GTK_VIEWPORT(widget->parent));
    /*
     * Check if viewport size (image scale) has changed.
     */
    if (ccd_state.focus_view_width  != (unsigned int)hadj->page_size
     || ccd_state.focus_view_height != (unsigned int)vadj->page_size)
    {
        ccd_state.focus_view_width   = (unsigned int)hadj->page_size;
        ccd_state.focus_view_height  = (unsigned int)vadj->page_size;
        ccd_state.focus_view_xoffset = (unsigned int)hadj->value;
        ccd_state.focus_view_yoffset = (unsigned int)vadj->value;
        focus_recalc_viewport();
    }
    else
    {
        ccd_state.focus_view_xoffset = (unsigned int)hadj->value;
        ccd_state.focus_view_yoffset = (unsigned int)vadj->value;
    }
    gdk_window_set_back_pixmap(widget->window, NULL, FALSE);
    if (ccd_state.focus.image && ccd_state.focus.image->pixmap)
    {
        gdk_window_copy_area(widget->window, widget->style->fg_gc[GTK_STATE_NORMAL],
                             event->area.x, event->area.y,
                             ccd_state.focus.image->pixmap,
                             event->area.x, event->area.y,
                             event->area.width, event->area.height);
        /*
         * Paint any areas outside the image with default color.
         */
        if (event->area.x + event->area.width > ccd_state.focus_width)
            gdk_draw_rectangle(widget->window, widget->style->bg_gc[GTK_STATE_NORMAL], TRUE,
                               ccd_state.focus_width, event->area.y,
                               event->area.x + event->area.width - ccd_state.focus_width, event->area.height);
        if (event->area.y + event->area.height > ccd_state.focus_height)
            gdk_draw_rectangle(widget->window, widget->style->bg_gc[GTK_STATE_NORMAL], TRUE,
                               event->area.x, ccd_state.focus_height,
                               event->area.width, event->area.y + event->area.height - ccd_state.focus_height);
    }
    else
        gdk_draw_rectangle(widget->window, widget->style->bg_gc[GTK_STATE_NORMAL], TRUE,
                           event->area.x, event->area.y,
                           event->area.width, event->area.height);
    focus_start(save_state);
}
static void cbHistogramExpose(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    int   i, j, k, max, top;
    float scale;

    for (i = max = 0; i < HISTOGRAM_BINS; i += HISTOGRAM_BINS/128)
    {
        for (j = k = 0; j < HISTOGRAM_BINS/128; j++, k += ccd_state.focus.image->histogram[i + j]);
        if (max < k) max = k;
    }
    scale = 255.0 / max;
    for (i = max = 0; i < HISTOGRAM_BINS; i += HISTOGRAM_BINS/128)
    {
        for (j = k = 0; j < HISTOGRAM_BINS/128; j++, k += ccd_state.focus.image->histogram[i + j]);
        top = (view_palettes[GAMMALOG1_GREY][(int)(k * scale)] & 0xFF) >> 1;
        gdk_draw_rectangle(widget->window, widget->style->fg_gc[GTK_STATE_NORMAL], TRUE, (i*128)/HISTOGRAM_BINS, 128 - top, 1, top);
        gdk_draw_rectangle(widget->window, widget->style->bg_gc[GTK_STATE_NORMAL], TRUE, (i*128)/HISTOGRAM_BINS, 0, 1, 128 - top);
    }
}
static void cbFocusScaleChanged(GtkObject *object, gpointer data)
{
    gchar          str[16];
    unsigned int   scale, save_state;

    if (ccd_state.focus_scale == (unsigned int)GTK_ADJUSTMENT(object)->value)
        return;
    save_state = focus_stop(FALSE);
    ccd_state.focus_scale = (unsigned int)GTK_ADJUSTMENT(object)->value;
    if (ccd_state.focus_scale > FOCUS_SCALE_1_1)
    {
        scale                   = (ccd_state.focus_scale - FOCUS_SCALE_1_1) * 2;
        ccd_state.focus_width   = ccd_state.focus.ccd->width  * scale;
        ccd_state.focus_height  = ccd_state.focus.ccd->height * scale;
        sprintf(str, "1:%d", scale);
    }
    else
    {
        if (ccd_state.focus_scale == 0) ccd_state.focus_scale = FOCUS_SCALE_1_1;
        if ((scale = (FOCUS_SCALE_1_1 - ccd_state.focus_scale) * 2) == 0) scale = 1;
        ccd_state.focus_width   = ccd_state.focus.ccd->width  / scale;
        ccd_state.focus_height  = ccd_state.focus.ccd->height / scale;
        sprintf(str, "%d:1", scale);
    }
    if (!ccd_state.focus.image)
    {
        ccd_state.focus.image = (struct ccd_image *)malloc(sizeof(struct ccd_image));
        memset(ccd_state.focus.image, 0, sizeof(struct ccd_image));
    }
    if (ccd_state.focus.image->pixmap)
        gdk_pixmap_unref(ccd_state.focus.image->pixmap);
    ccd_state.focus.image->pixmap = gdk_pixmap_new(gui_state.window->window, ccd_state.focus_width, ccd_state.focus_height, -1);
    gdk_draw_rectangle(ccd_state.focus.image->pixmap, gui_state.window->style->fg_gc[GTK_STATE_NORMAL], TRUE, 0, 0, ccd_state.focus_width, ccd_state.focus_height);
    gtk_label_set_text(GTK_LABEL((GtkWidget *)data), str);
    gtk_drawing_area_size(GTK_DRAWING_AREA(gui_state.view_focus), ccd_state.focus_width, ccd_state.focus_height);
    focus_recalc_viewport();
    gdk_flush();
    focus_start(save_state);
}
/*
 * Guide.
 */
static void cbGuideExpose(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    gdk_window_set_back_pixmap(widget->window, NULL, FALSE);
    if (ccd_state.guide.image && ccd_state.guide.image->pixmap)
    {
        gdk_window_copy_area(widget->window, widget->style->fg_gc[GTK_STATE_NORMAL],
                             event->area.x, event->area.y,
                             ccd_state.guide.image->pixmap,
                             event->area.x, event->area.y,
                             event->area.width, event->area.height);
        /*
         * Paint any areas outside the image with default color.
         */
        if (event->area.x + event->area.width > GUIDE_WIDTH*GUIDE_SCALE)
            gdk_draw_rectangle(widget->window, widget->style->bg_gc[GTK_STATE_NORMAL], TRUE,
                               GUIDE_WIDTH*GUIDE_SCALE, event->area.y,
                               event->area.x + event->area.width - GUIDE_WIDTH*GUIDE_SCALE, event->area.height);
        if (event->area.y + event->area.height > GUIDE_HEIGHT*GUIDE_SCALE)
            gdk_draw_rectangle(widget->window, widget->style->bg_gc[GTK_STATE_NORMAL], TRUE,
                               event->area.x, GUIDE_HEIGHT*GUIDE_SCALE,
                               event->area.width, event->area.y + event->area.height - GUIDE_HEIGHT*GUIDE_SCALE);
    }
    else
        gdk_draw_rectangle(widget->window, widget->style->bg_gc[GTK_STATE_NORMAL], TRUE,
                           event->area.x, event->area.y,
                           event->area.width, event->area.height);
}
static void cbGuideDevChanged(GtkWidget *widget, gpointer data)
{
    struct ccd_dev *old_ccd = ccd_state.guide.ccd;

    if (old_ccd != (ccd_state.guide.ccd = ccd_find_by_name(gtk_entry_get_text(GTK_ENTRY(widget)))))
    {
        ccd_state.guide_x_centroid = 0.0;
        ccd_state.guide_y_centroid = 0.0;
        ccd_state.guide.dac_bits   = ccd_state.guide.ccd->dac_bits;
    }
}
static void cbSelfGuideMode(GtkWidget *widget, gpointer data)
{
    if ((guint)data == FIELD_DARK)
        ccd_state.guide_dark = GTK_TOGGLE_BUTTON(widget)->active;
    else if (GTK_TOGGLE_BUTTON(widget)->active)
        ccd_state.guide_fields = (guint)data;
}
static void cbGuideClear(GtkWidget *widget, gpointer data)
{
    ccd_state.guide_x_centroid = 0.0;
    ccd_state.guide_y_centroid = 0.0;
}
/*
 * Select a guide object.
 */
static struct
{
    gfloat   x, y;
    GdkGC   *gc;
    GdkColor color;
} select_state;
static gint eventGuideSelectDie(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    /*
     * Reset exposure options.
     */
    if (ccd_state.guide.downloading)
        ccd_release(ccd_state.guide.ccd);
    if (ccd_state.guide.input_tag)
        gdk_input_remove(ccd_state.guide.input_tag);
    if (ccd_state.guide.image->pixmap)
        gdk_pixmap_unref(ccd_state.guide.image->pixmap);
    if (ccd_state.guide.image->pixels);
        free(ccd_state.guide.image->pixels);
    ccd_state.guide.downloading   = FALSE;
    ccd_state.guide.input_tag     = 0;
    ccd_state.guide.image->pixmap = NULL;
    ccd_state.guide.image->pixels = NULL;
    ccd_state.guide.dac_bits      = ccd_state.guide.ccd->dac_bits;
    ccd_state.guide.width         = GUIDE_WIDTH;
    ccd_state.guide.height        = GUIDE_HEIGHT;
    ccd_state.guide.xbin          = 1;
    ccd_state.guide.ybin          = 1;
    ccd_state.guide.msec          = ccd_state.guide_msec;
    ccd_state.guide.image->width  = ccd_state.guide.width  / ccd_state.guide.xbin;
    ccd_state.guide.image->height = ccd_state.guide.height / ccd_state.guide.ybin;
    ccd_state.guide.image->depth  = ccd_state.guide.ccd->depth;
    if (select_state.gc)
    {
        gdk_gc_unref(select_state.gc);
        select_state.gc = NULL;
    }
    return (0);
}
static void cbGuideSelectCancel(GtkWidget *widget, gpointer data)
{
    gtk_widget_destroy((GtkWidget *)data);
}
static void cbGuideSelectOK(GtkWidget *widget, gpointer data)
{
    int x_radius, y_radius;

    if (select_state.x >= 0.0 && select_state.y >= 0.0)
    {
        ccd_state.guide_x_centroid = select_state.x;
        ccd_state.guide_y_centroid = select_state.y;
        x_radius                   = GUIDE_WIDTH  / 2 / ccd_state.guide.xbin;
        y_radius                   = GUIDE_HEIGHT / 2 / ccd_state.guide.ybin;
        if (!ccd_image_find_best_centroid(ccd_state.guide.image, ccd_state.guide.image->pixels, &ccd_state.guide_x_centroid, &ccd_state.guide_y_centroid, x_radius, y_radius, &x_radius, &y_radius, ccd_state.reg_noise_sigs))
        {
            gnome_warning_dialog_parented(_("Unable to find suitable centroid."), GTK_WINDOW(gui_state.window));
            ccd_state.guide_x_centroid = 0.0;
            ccd_state.guide_y_centroid = 0.0;
        }
        else
        {
            ccd_state.guide_x_centroid *= ccd_state.guide.xbin;
            ccd_state.guide_y_centroid *= ccd_state.guide.ybin;
            ccd_state.guide.xoffset    += ccd_state.guide_x_centroid - GUIDE_WIDTH  / 2;
            ccd_state.guide.yoffset    += ccd_state.guide_y_centroid - GUIDE_HEIGHT / 2;
            ccd_state.guide_x_centroid  = GUIDE_WIDTH  / 2;
            ccd_state.guide_y_centroid  = GUIDE_HEIGHT / 2;
        }
    }
    gtk_widget_destroy((GtkWidget *)data);
}
static gboolean cbSelectEvent(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    GdkEventButton *eventButton;
    gfloat          x_centroid, y_centroid;
    gint            x_radius, y_radius;

    if (event->type == GDK_BUTTON_PRESS)
    {
        eventButton = (GdkEventButton *)event;
        x_centroid  = eventButton->x;
        y_centroid  = eventButton->y;
        x_radius    = GUIDE_WIDTH  / 2 / ccd_state.guide.xbin;
        y_radius    = GUIDE_HEIGHT / 2 / ccd_state.guide.ybin;
        if (!ccd_image_find_best_centroid(ccd_state.guide.image, ccd_state.guide.image->pixels, &x_centroid, &y_centroid, x_radius, y_radius, &x_radius, &y_radius, ccd_state.reg_noise_sigs))
        {
            select_state.x = eventButton->x;
            select_state.y = eventButton->y;
        }
        else
        {
            select_state.x = x_centroid;
            select_state.y = y_centroid;
        }
        gtk_widget_queue_draw(widget);
    }
    return (TRUE);
}
static void cbGuideSelectExpose(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    gdk_window_set_back_pixmap(widget->window, NULL, FALSE);
    if (ccd_state.guide.image && ccd_state.guide.image->pixmap)
    {
        gdk_window_copy_area(widget->window, widget->style->fg_gc[GTK_STATE_NORMAL],
                             event->area.x, event->area.y,
                             ccd_state.guide.image->pixmap,
                             event->area.x, event->area.y,
                             event->area.width, event->area.height);
        /*
         * Draw guide object selection crosshairs.
         */
        if (select_state.x >= 0 && select_state.y >= 0)
        {
            gdk_draw_rectangle(widget->window, select_state.gc, TRUE, select_state.x, select_state.y - GUIDE_HEIGHT/2/ccd_state.guide.ybin, 1, GUIDE_HEIGHT/ccd_state.guide.ybin);
            gdk_draw_rectangle(widget->window, select_state.gc, TRUE, select_state.x - GUIDE_WIDTH/2/ccd_state.guide.xbin, select_state.y, GUIDE_WIDTH/ccd_state.guide.xbin, 1);
        }
        /*
         * Paint any areas outside the image with default color.
         */
        if (event->area.x + event->area.width > ccd_state.guide.width / ccd_state.guide.xbin)
            gdk_draw_rectangle(widget->window, widget->style->bg_gc[GTK_STATE_NORMAL], TRUE,
                               ccd_state.guide.width / ccd_state.guide.xbin, event->area.y,
                               event->area.x + event->area.width - ccd_state.guide.width / ccd_state.guide.xbin, event->area.height);
        if (event->area.y + event->area.height > ccd_state.guide.height / ccd_state.guide.ybin)
            gdk_draw_rectangle(widget->window, widget->style->bg_gc[GTK_STATE_NORMAL], TRUE,
                               event->area.x, ccd_state.guide.height / ccd_state.guide.ybin,
                               event->area.width, event->area.y + event->area.height - ccd_state.guide.height / ccd_state.guide.ybin);
    }
    else
        gdk_draw_rectangle(widget->window, widget->style->bg_gc[GTK_STATE_NORMAL], TRUE,
                           event->area.x, event->area.y,
                           event->area.width, event->area.height);
}
static void cbGuideSelect(GtkWidget *widget, gpointer data)
{
    GtkWidget *vbox          = gtk_vbox_new(FALSE, 0);
    GtkWidget *hbox          = gtk_hbox_new(FALSE, 0);
    GtkWidget *button_ok     = gtk_button_new_with_label(_("OK"));
    GtkWidget *button_cancel = gtk_button_new_with_label(_("Cancel"));
    GtkWidget *frame         = gtk_frame_new(NULL);
    GtkWidget *eventbox      = gtk_event_box_new();
    GtkWidget *view          = gtk_drawing_area_new();
    GtkWidget *window        = gtk_window_new(GTK_WINDOW_DIALOG);
    if (ccd_state.guide_x_centroid >= 0.0 && ccd_state.guide_y_centroid >= 0.0)
    {
        select_state.x = (ccd_state.guide_x_centroid + ccd_state.guide.xoffset) / GUIDE_SELECT_BIN;
        select_state.y = (ccd_state.guide_y_centroid + ccd_state.guide.yoffset) / GUIDE_SELECT_BIN;
    }
    else
    {
        select_state.x = -1.0;
        select_state.y = -1.0;
    }
    ccd_state.guide.dac_bits = ccd_state.guide.ccd->dac_bits / 2;
    ccd_state.guide.width    = ccd_state.guide.ccd->width  - 2 * GUIDE_WIDTH;
    ccd_state.guide.height   = ccd_state.guide.ccd->height - 2 * GUIDE_HEIGHT;
    ccd_state.guide.xoffset  = GUIDE_WIDTH;
    ccd_state.guide.yoffset  = GUIDE_HEIGHT;
    ccd_state.guide.xbin     = GUIDE_SELECT_BIN;
    ccd_state.guide.ybin     = GUIDE_SELECT_BIN;
    ccd_state.guide.msec     = ccd_state.guide_msec / 2;
    if (ccd_state.guide.image)
    {
        if (ccd_state.guide.image->pixels)
            free(ccd_state.guide.image->pixels);
        ccd_state.guide.image->pixels = NULL;
    }
    else
    {
        ccd_state.guide.image = malloc(sizeof(struct ccd_image));
        memset(ccd_state.guide.image, 0, sizeof(struct ccd_image));
    }
    ccd_state.guide.image->width  = ccd_state.guide.width  / ccd_state.guide.xbin;
    ccd_state.guide.image->height = ccd_state.guide.height / ccd_state.guide.ybin;
    ccd_state.guide.image->depth  = ccd_state.guide.ccd->depth;
    if (ccd_state.guide.image->pixmap)
        gdk_pixmap_unref(ccd_state.guide.image->pixmap);
    ccd_state.guide.image->pixmap = gdk_pixmap_new(gui_state.window->window, ccd_state.guide.image->width, ccd_state.guide.image->height, -1);
    gdk_draw_rectangle(ccd_state.guide.image->pixmap, gui_state.window->style->fg_gc[GTK_STATE_NORMAL], TRUE, 0, 0, ccd_state.guide.image->width, ccd_state.guide.image->height);
    if (ccd_connect(ccd_state.guide.ccd))
    {
        static GtkWidget *list[4];
        ccd_expose_frame(&ccd_state.guide);
        list[0]                     = window;
        list[1]                     = button_ok;
        list[2]                     = button_cancel;
        list[3]                     = view;
        ccd_state.guide.input_tag   = gdk_input_add(ccd_state.guide.ccd->fd, GDK_INPUT_READ, (GdkInputFunction)cbReadGuideSelect, (gpointer)list);
        ccd_state.guide.downloading = TRUE;
        ccd_state.guide.msec        = ccd_state.guide_msec;
    }
    else
    {
        gnome_warning_dialog_parented(_("Unable to connect to guide camera."), GTK_WINDOW(gui_state.window));
        gdk_pixmap_unref(ccd_state.guide.image->pixmap);
        if (ccd_state.guide.image->pixels);
            free(ccd_state.guide.image->pixels);
        ccd_state.guide.image->pixmap = NULL;
        ccd_state.guide.image->pixels = NULL;
        ccd_state.guide.dac_bits      = ccd_state.guide.ccd->dac_bits;
        ccd_state.guide.width         = GUIDE_WIDTH;
        ccd_state.guide.height        = GUIDE_HEIGHT;
        ccd_state.guide.xbin          = 1;
        ccd_state.guide.ybin          = 1;
        ccd_state.guide.msec          = ccd_state.guide_msec;
        ccd_state.guide.image->width  = ccd_state.guide.width  / ccd_state.guide.xbin;
        ccd_state.guide.image->height = ccd_state.guide.height / ccd_state.guide.ybin;
        return;
    }
    gtk_signal_connect(GTK_OBJECT(window),        "delete_event",       GTK_SIGNAL_FUNC(eventGuideSelectDie),  NULL);
    gtk_signal_connect(GTK_OBJECT(window),        "destroy",            GTK_SIGNAL_FUNC(eventGuideSelectDie),  NULL);
    gtk_signal_connect(GTK_OBJECT(button_ok),     "clicked",            GTK_SIGNAL_FUNC(cbGuideSelectOK),     (gpointer)window);
    gtk_signal_connect(GTK_OBJECT(button_cancel), "clicked",            GTK_SIGNAL_FUNC(cbGuideSelectCancel), (gpointer)window);
    gtk_signal_connect(GTK_OBJECT(eventbox),      "button_press_event", GTK_SIGNAL_FUNC(cbSelectEvent),         NULL);
    gtk_signal_connect(GTK_OBJECT(view),          "expose_event",       GTK_SIGNAL_FUNC(cbGuideSelectExpose),   NULL);
    gtk_drawing_area_size(GTK_DRAWING_AREA(view), ccd_state.guide.width / ccd_state.guide.xbin, ccd_state.guide.height / ccd_state.guide.ybin);
    gtk_container_add(GTK_CONTAINER(eventbox), view);
    gtk_container_add(GTK_CONTAINER(frame), eventbox);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), button_ok, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), button_cancel, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_widget_set_sensitive(button_ok, FALSE);
    gtk_widget_set_sensitive(button_cancel, FALSE);
    gtk_window_set_title(GTK_WINDOW(window), _("Loading Guide Field..."));
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(gui_state.window));
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    gtk_widget_show_all(window);
    select_state.gc          = gdk_gc_new(view->window);
    select_state.color.red   = 0xFFFF;
    select_state.color.green = 0x0000;
    select_state.color.blue  = 0x0000;
    gdk_color_alloc(gdk_colormap_get_system(), &select_state.color);
    gdk_gc_set_foreground(select_state.gc, &select_state.color);
    gdk_window_set_cursor(window->window, cursorWait);
    gdk_flush();
}
static void cbGuide(GtkWidget *widget, gpointer data)
{
    ccd_state.guiding = GTK_TOGGLE_BUTTON(widget)->active;
}
static void cbTrackRateChanged(GtkWidget *widget, gpointer data)
{
    float rate = gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(widget));
    switch ((guint)data)
    {
        case SCOPE_UP:
            ccd_state.guide_track_up = rate;
            break;
        case SCOPE_DOWN:
            ccd_state.guide_track_down = rate;
            break;
        case SCOPE_LEFT:
            ccd_state.guide_track_left = rate;
            break;
        case SCOPE_RIGHT:
            ccd_state.guide_track_right = rate;
            break;
    }
}
/*
 * Train guide tracking.
 */
static struct
{
    GtkWidget *dialog;
    GtkWidget *label;
    int        dir;
    float      x_centroid, y_centroid;
} train_state;
static void cbGuideTrainExit(GtkWidget *widget, gint button, gpointer data)
{
    if (ccd_state.guide.timeout_id)
    {
        gtk_timeout_remove(ccd_state.guide.timeout_id);
        ccd_state.guide.timeout_id = 0;
    }
    gtk_widget_destroy(widget);
    ccd_release(ccd_state.guide.ccd);
    scope_release(&scope_state);
    ccd_state.training       = FALSE;
    ccd_state.guide.msec     = ccd_state.guide_msec;
    ccd_state.guide.width    = GUIDE_WIDTH;
    ccd_state.guide.height   = GUIDE_HEIGHT;
    ccd_state.guide.xoffset += GUIDE_WIDTH;
    ccd_state.guide.yoffset += GUIDE_HEIGHT;
    if (ccd_state.guide.image)
    {
        if (ccd_state.guide.image->pixels)
            free(ccd_state.guide.image->pixels);
        ccd_state.guide.image->pixels = NULL;
    }
}
static void GuideTrainFailed()
{
    cbGuideTrainExit(train_state.dialog, 0, NULL);
    gnome_warning_dialog_parented(_("Training lost the guide star.  Try reducing the training time."), GTK_WINDOW(gui_state.window));
}
static gint cbGuideTrainBegin(gpointer data);
static gint cbGuideTrainEnd(gpointer data)
{
    float x_centroid, y_centroid, rate;
    int   x_radius,   y_radius;

    ccd_state.guide.timeout_id = 0;
    /*
     * Expose current position.
     */
    ccd_expose_frame(&ccd_state.guide);
    /*
     * Stop telescope slewing.
     */
    scope_update(train_state.dir, FALSE);
    /*
     * Load frame and measure centroid distance from original location.
     */
    while (ccd_load_frame(&ccd_state.guide));
    x_centroid = ccd_state.guide.image->width  / 2;
    y_centroid = ccd_state.guide.image->height / 2;
    x_radius   = ccd_state.reg_x_max_radius;
    y_radius   = ccd_state.reg_y_max_radius;
    if (!ccd_image_find_best_centroid(ccd_state.guide.image, ccd_state.guide.image->pixels, &x_centroid, &y_centroid, ccd_state.guide.image->width/2, ccd_state.guide.image->height/2, &x_radius, &y_radius, ccd_state.reg_noise_sigs))
    {
        /*
         * Lost star.
         */
        GuideTrainFailed();
    }
    else
    {
        switch (train_state.dir)
        {
            case SCOPE_LEFT:
                ccd_state.guide_track_left = ((x_centroid - train_state.x_centroid + 2 * x_radius) * 1000.0) / ccd_state.guide_train_msec;
                break;
            case SCOPE_RIGHT:
                ccd_state.guide_track_right = ((train_state.x_centroid - x_centroid + 2 * x_radius) * 1000.0) / ccd_state.guide_train_msec;
                /*
                 * Preload hysterisis on up/down training.
                 */
                scope_update(SCOPE_DOWN, TRUE);
                usleep(100000);
                scope_update(SCOPE_DOWN, FALSE);
                break;
            case SCOPE_UP:
                ccd_state.guide_track_up = ((y_centroid - train_state.y_centroid + 2 * y_radius) * 1000.0) / ccd_state.guide_train_msec;
                break;
            case SCOPE_DOWN:
                ccd_state.guide_track_down = ((train_state.y_centroid - y_centroid + 2 * y_radius) * 1000.0) / ccd_state.guide_train_msec;
                break;
        }
        if (train_state.dir++ != SCOPE_DOWN)
            /*
             * Train next direction.
             */
            gtk_idle_add(cbGuideTrainBegin, NULL);
        else
        {
            /*
             * Training complete.
             */
            if (ccd_state.guide_track_left < 0 && ccd_state.guide_track_right < 0)
            {
                gnome_warning_dialog_parented(_("Scope control may need to be reversed in RA."), GTK_WINDOW(gui_state.window));
                rate                        = ccd_state.guide_track_left;
                ccd_state.guide_track_left  = ccd_state.guide_track_right;
                ccd_state.guide_track_right = rate;
            }
            if (ccd_state.guide_track_left  < GUIDE_MIN_RATE)
                ccd_state.guide_track_left  = GUIDE_MIN_RATE;
            if (ccd_state.guide_track_right < GUIDE_MIN_RATE)
                ccd_state.guide_track_right = GUIDE_MIN_RATE;
            if (ccd_state.guide_track_up < 0 && ccd_state.guide_track_down < 0)
            {
                gnome_warning_dialog_parented(_("Scope control may need to be reversed in Dec."), GTK_WINDOW(gui_state.window));
                rate                       = ccd_state.guide_track_up;
                ccd_state.guide_track_up   = ccd_state.guide_track_down;
                ccd_state.guide_track_down = rate;
            }
            if (ccd_state.guide_track_up   < GUIDE_MIN_RATE)
                ccd_state.guide_track_up   = GUIDE_MIN_RATE;
            if (ccd_state.guide_track_down < GUIDE_MIN_RATE)
                ccd_state.guide_track_down = GUIDE_MIN_RATE;
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(gui_state.spin_track_left),  ccd_state.guide_track_left);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(gui_state.spin_track_right), ccd_state.guide_track_right);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(gui_state.spin_track_up),    ccd_state.guide_track_up);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(gui_state.spin_track_down),  ccd_state.guide_track_down);
            cbGuideTrainExit(train_state.dialog, 0, NULL);
        }
    }
    return (FALSE);
}
static gint cbGuideTrainBegin(gpointer data)
{
    static gchar *dirstr[4] = {N_("Left"), N_("Right"), N_("Up"), N_("Down")};
    gchar        str[80];
    int          x_radius, y_radius;

    ccd_state.training = TRUE;
    /*
     * Start telescope guide speed slewing in current direction.
     */
    scope_update(train_state.dir, TRUE);
    sprintf(str, _("Measure %s rate..."), dirstr[train_state.dir - 1]);
    gtk_label_set_text(GTK_LABEL(train_state.label), str);
    usleep(100000);
    /*
     * Expose current position.
     */
    ccd_expose_frame(&ccd_state.guide);
    /*
     * Set delay for training duration.
     */
    ccd_state.guide.timeout_id = gtk_timeout_add(ccd_state.guide_train_msec, (GtkFunction)cbGuideTrainEnd, NULL);
    /*
     * Load frame.
     */
    while (ccd_load_frame(&ccd_state.guide));
    train_state.x_centroid = ccd_state.guide.image->width  / 2;
    train_state.y_centroid = ccd_state.guide.image->height / 2;
    x_radius               = ccd_state.reg_x_max_radius;
    y_radius               = ccd_state.reg_y_max_radius;
    if (!ccd_image_find_best_centroid(ccd_state.guide.image, ccd_state.guide.image->pixels, &train_state.x_centroid, &train_state.y_centroid, ccd_state.guide.image->width/2, ccd_state.guide.image->height/2, &x_radius, &y_radius, ccd_state.reg_noise_sigs))
    {
        /*
         * Lost star.
         */
        GuideTrainFailed();
    }
    return (FALSE);
}
static void cbGuideTrain(GtkWidget *widget, gpointer data)
{
    if (ccd_state.guide_x_centroid <= 0.0 || ccd_state.guide_y_centroid <= 0.0)
    {
        gnome_warning_dialog_parented(_("First select a very bright guide star before training."), GTK_WINDOW(gui_state.window));
    }
    else
    {
        train_state.dialog = gnome_dialog_new(_("Training Guide Tracking Rate"), GNOME_STOCK_BUTTON_CANCEL, NULL);
        train_state.label  = gtk_label_new(NULL);
        gtk_window_set_transient_for(GTK_WINDOW(train_state.dialog), GTK_WINDOW(gui_state.window));
        gtk_window_set_modal(GTK_WINDOW(train_state.dialog), TRUE);
        gnome_dialog_set_default(GNOME_DIALOG(train_state.dialog), 0);
        gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(train_state.dialog)->vbox), train_state.label, TRUE, TRUE, 0);
        gtk_signal_connect(GTK_OBJECT(train_state.dialog), "clicked", cbGuideTrainExit, NULL);
        gtk_widget_show_all(train_state.dialog);
        /*
         * Set training image area to twice tracking area.
         */
        ccd_state.guide.width    = 2 * GUIDE_WIDTH;
        ccd_state.guide.height   = 2 * GUIDE_HEIGHT;
        ccd_state.guide.xoffset -= GUIDE_WIDTH;
        ccd_state.guide.yoffset -= GUIDE_HEIGHT;
        if (ccd_state.guide.image)
        {
            if (ccd_state.guide.image->pixels)
                free(ccd_state.guide.image->pixels);
            ccd_state.guide.image->pixels = NULL;
        }
        /*
         * Init guide image.
         */
        if (!ccd_state.guide.image)
        {
            ccd_state.guide.image = (struct ccd_image *)malloc(sizeof(struct ccd_image));
            memset(ccd_state.guide.image, 0, sizeof(struct ccd_image));
            ccd_state.guide.image->width  = ccd_state.guide.width  / ccd_state.guide.xbin;
            ccd_state.guide.image->height = ccd_state.guide.height / ccd_state.guide.ybin;
            ccd_state.guide.image->depth  = ccd_state.guide.ccd->depth;
        }
        if (!ccd_state.guide.image->pixels)
            ccd_state.guide.image->pixels = malloc(ccd_state.guide.image->width * ccd_state.guide.image->height * ((ccd_state.guide.image->depth + 7)/8));
        if (!ccd_state.guide.ccd->fd)
            ccd_connect(ccd_state.guide.ccd);
        if (!scope_state.fd)
            while (scope_connect(&scope_state) < 0)
                gnome_warning_dialog_parented(_("Telescope busy.  Wait for GOTO to complete."), GTK_WINDOW(gui_state.window));
        ccd_state.guide.msec = 10; // This has to be very short to make a clean centroid image - the scope is currently slewing.
        train_state.dir      = SCOPE_LEFT;
        gtk_idle_add(cbGuideTrainBegin, NULL);
        /*
         * Preload hysterisis on left/right training.
         */
        scope_update(SCOPE_RIGHT, TRUE);
        usleep(100000);
        scope_update(SCOPE_RIGHT, FALSE);
    }
}
/*
 * Filter Wheel.
 */
static void cbWheelReset(GtkWidget *widget, gpointer data)
{
    if (wheel_state.wheel.status != WHEEL_IDLE)
    {
        gdk_beep();
    }
    else
    {
        wheel_reset(&wheel_state.wheel);
        gtk_label_set_text(GTK_LABEL(gui_state.wheel_status), _("Busy"));
    }
}
static void cbWheelGoto(GtkWidget *widget, gpointer data)
{
    if (wheel_state.wheel.status != WHEEL_IDLE)
    {
        if (GTK_TOGGLE_BUTTON(widget)->active)
        {
            gdk_beep();
            GTK_TOGGLE_BUTTON(widget)->active = 0;
            GTK_TOGGLE_BUTTON(gui_state.radio_filter[wheel_state.wheel.current])->active = 1;
        }
    }
    else if (GTK_TOGGLE_BUTTON(widget)->active)
    {
        wheel_goto(&wheel_state.wheel, (guint)data + 1);
        gtk_label_set_text(GTK_LABEL(gui_state.wheel_status), _("Busy"));
    }
}
static void cbFilterTypeChanged(GtkWidget *widget, gpointer data)
{
    wheel_state.filter[(guint)data] = filter_find_by_name(gtk_entry_get_text(GTK_ENTRY(widget)));
}
static void cbReadWheel(gpointer data, gint fd, GdkInputCondition in)
{
    int prev_filter = wheel_state.wheel.current;
    wheel_read(&wheel_state.wheel);
    if (wheel_state.wheel.status == WHEEL_IDLE)
        gtk_label_set_text(GTK_LABEL(gui_state.wheel_status), _("Idle"));
    if (prev_filter != wheel_state.wheel.current)
    {
        GTK_TOGGLE_BUTTON(gui_state.radio_filter[prev_filter - 1])->active               = 0;
        GTK_TOGGLE_BUTTON(gui_state.radio_filter[wheel_state.wheel.current - 1])->active = 1;
        gtk_widget_queue_draw(gui_state.radio_filter[prev_filter - 1]);
        gtk_widget_queue_draw(gui_state.radio_filter[wheel_state.wheel.current - 1]);
    }
    //wheel_state.input_tag = 0;
}
static void cbSequenceMode(GtkWidget *widget, gpointer data)
{
    wheel_state.sequence[(guint)data] = GTK_TOGGLE_BUTTON(widget)->active;
}
static void cbExpPercentChanged(GtkWidget *widget, gpointer data)
{
    wheel_state.exp_percent[(guint)data] = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}
static void cbFilterFlatChanged(GtkWidget *widget, gpointer data)
{
    wheel_state.flat_frame[(guint)data] = ccd_image_find_by_name(gtk_entry_get_text(GTK_ENTRY(widget)));
}
static void cbSequence(GtkWidget *widget, gpointer data)
{
    ccd_state.sequencing = GTK_TOGGLE_BUTTON(widget)->active;
}
/*
 * Miscellaneous settings.
 */
static void cbMiscChanged(GtkWidget *widget, gpointer data)
{
    switch ((gint)data)
    {
        case 0:
            ccd_state.reg_noise_sigs = gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(widget));
            break;
        case 1:
            ccd_state.reg_x_max_radius = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
            break;
        case 2:
            ccd_state.reg_x_max_radius = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
            break;
        case 3:
            ccd_state.reg_x_range = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
            break;
        case 4:
            ccd_state.reg_y_range = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
            break;
        case 5:
            ccd_state.guide_interleave_offset = gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(widget));
            break;
        case 6:
            ccd_state.guide_min_offset = gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(widget));
            break;
        case 7:
            ccd_state.guide_msec = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
            break;
        case 8:
            ccd_state.guide_train_msec = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
            break;
        case 9:
            scope_state.init_delay = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
            break;
    }
}

/***************************************************************************\
*                                                                           *
*                                 Events                                    *
*                                                                           *
\***************************************************************************/

static void savePrefs(void)
{
    int i;

    if (gui_state.window)
    {
        /*
         * Save preferences.
         */
        prefs.ScopeIFace       = scope_state.iface;
        prefs.ScopeRA          = scope_state.flags & SCOPE_REV_RA;
        prefs.ScopeDec         = scope_state.flags & SCOPE_REV_DEC;
        prefs.ScopeSwap        = scope_state.flags & SCOPE_SWAP_XY;
        prefs.ScopeInitDelay   = scope_state.init_delay;
        prefs.RegSig           = ccd_state.reg_noise_sigs;
        prefs.RegXRad          = ccd_state.reg_x_max_radius;
        prefs.RegYRad          = ccd_state.reg_y_max_radius;
        prefs.RegXRange        = ccd_state.reg_x_range;
        prefs.RegYRange        = ccd_state.reg_y_range;
        prefs.TrackTrain       = ccd_state.guide_train_msec;
        prefs.TrackMsec        = ccd_state.guide_msec;
        prefs.TrackMin         = ccd_state.guide_min_offset;
        prefs.TrackFieldOffset = ccd_state.guide_interleave_offset;
        prefs.TrackSelf        = ccd_state.guide_fields;
        prefs.TrackUp          = ccd_state.guide_track_up;
        prefs.TrackDown        = ccd_state.guide_track_down;
        prefs.TrackLeft        = ccd_state.guide_track_left;
        prefs.TrackRight       = ccd_state.guide_track_right;
        for (i = 0; i < 7; i++)
        {
            prefs.Filter[i]    = wheel_state.filter[i];
            prefs.FilterExp[i] = wheel_state.exp_percent[i];
        }
    }
}
static gint eventAIDestroy(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    return (0);
}
static gint eventAIDelete(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    int i;

    /*
     * Don't die if in the middle of an exposure.
     */
    if (ccd_state.exposing)
        return(1);
    /*
     * Make sure prefences are saved.
     */
    savePrefs();
    /*
     * Window deleted.
     */
    if (ccd_state.focus.image)
        if (ccd_state.focus.image->pixmap)
            gdk_pixmap_unref(ccd_state.focus.image->pixmap);
    memset(&gui_state, 0, sizeof(gui_state));
    /*
     * Close filter wheel device.
     */
    if (wheel_state.input_tag)
        gdk_input_remove(wheel_state.input_tag);
    wheel_state.input_tag = 0;
    wheel_release(&wheel_state.wheel);
    /*
     * Close telescope device.
     */
    scope_release(&scope_state);
    /*
     * Close all CCD camera devices.
     */
    for (i = 0; i < ccd_num_devices; i++)
    {
        if (ccd_devices[i][0].fd) ccd_release(&ccd_devices[i][0]);
        if (ccd_devices[i][1].fd) ccd_release(&ccd_devices[i][1]);
        if (ccd_devices[i][2].fd) ccd_release(&ccd_devices[i][2]);
    }
    return (0);
}

/***************************************************************************\
*                                                                           *
*                  Entrypoints to CCD control window                        *
*                                                                           *
\***************************************************************************/

void imageUpdateList(void)
{
    int               i;
    struct ccd_image *image, *image_bias, *image_dark, *image_flat;

    image_bias = image_dark = image_flat = NULL;
    if (gui_state.list_images)
        g_list_free(gui_state.list_images);
    gui_state.list_images = g_list_append(NULL, _("(None)"));
    for (image = ccd_image_first(); image; image = ccd_image_next(image))
    {
        gui_state.list_images = g_list_append(gui_state.list_images, image->name);
        if (ccd_state.bias_frame && image == ccd_state.bias_frame)
            image_bias = image;
        if (ccd_state.dark_frame && image == ccd_state.dark_frame)
            image_dark = image;
        if (ccd_state.flat_frame && image == ccd_state.flat_frame)
            image_flat = image;
    }
    if (gui_state.combo_bias)
    {
        gtk_combo_set_popdown_strings(GTK_COMBO(gui_state.combo_bias), gui_state.list_images);
        gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(gui_state.combo_bias)->entry), image_bias ? image_bias->name : _("(None)"));
    }
    if (gui_state.combo_dark)
    {
        gtk_combo_set_popdown_strings(GTK_COMBO(gui_state.combo_dark), gui_state.list_images);
        gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(gui_state.combo_dark)->entry), image_dark ? image_dark->name : _("(None)"));
    }
    if (gui_state.combo_flat)
    {
        gtk_combo_set_popdown_strings(GTK_COMBO(gui_state.combo_flat), gui_state.list_images);
        gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(gui_state.combo_flat)->entry), image_flat ? image_flat->name : _("(None)"));
    }
    ccd_state.bias_frame = image_bias;
    ccd_state.dark_frame = image_dark;
    ccd_state.flat_frame = image_flat;
    for (i = 0; i < MAX_FILTERS; i++)
    {
        image_flat = NULL;
        for (image = ccd_image_first(); image; image = ccd_image_next(image))
            if (wheel_state.flat_frame[i] && image == wheel_state.flat_frame[i])
                image_flat = image;
        if (gui_state.combo_filterflat[i])
        {
            gtk_combo_set_popdown_strings(GTK_COMBO(gui_state.combo_filterflat[i]), gui_state.list_images);
            gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(gui_state.combo_filterflat[i])->entry), image_flat ? image_flat->name : _("(None)"));
        }
    }
}
void deactivateAcquireImage(int reactivate)
{
    if (reactivate && gui_state.window)
    {
        gtk_widget_unref(gui_state.window);
        activateAcquireImage(imageAdd);
    }
    else
    {
        savePrefs();
    }
}
void activateAcquireImage(void (*new_image_child)(struct ccd_image *))
{
    int           i;
    gchar         str[80];
    GtkWidget    *vbox_panels, *hbox_panels;
    static GList *list_ccds = NULL;

    if (gui_state.window == NULL)
    {
        /*
         * Find all attached CCD cameras.
         */
        if (list_ccds != NULL)
        {
            g_list_free(list_ccds);
            list_ccds = NULL;
        }
        for (ccd_num_devices = i = 0; i < MAX_CCD_DEVICES; i++)
        {
            sprintf(ccd_devices[ccd_num_devices][0].filename, "/dev/ccd%c", 'a' + i);
            if (ccd_connect(&ccd_devices[ccd_num_devices][0]))
            {
                list_ccds = g_list_append(list_ccds, ccd_devices[ccd_num_devices][0].camera);
                ccd_release(&ccd_devices[ccd_num_devices][0]);
                /*
                 * Set up odd/even frame capable devices.
                 */
                ccd_devices[ccd_num_devices][0].base = &ccd_devices[ccd_num_devices][0];
                if (ccd_devices[ccd_num_devices][0].fields > 1)
                {
                    memcpy(&ccd_devices[ccd_num_devices][1], &ccd_devices[ccd_num_devices][0], sizeof(struct ccd_dev));
                    memcpy(&ccd_devices[ccd_num_devices][2], &ccd_devices[ccd_num_devices][0], sizeof(struct ccd_dev));
                    strcat(ccd_devices[ccd_num_devices][1].filename, "1");
                    strcat(ccd_devices[ccd_num_devices][2].filename, "2");
                    ccd_devices[ccd_num_devices][0].odd_field  = &ccd_devices[ccd_num_devices][1];
                    ccd_devices[ccd_num_devices][0].even_field = &ccd_devices[ccd_num_devices][2];
                    ccd_devices[ccd_num_devices][1].base       = &ccd_devices[ccd_num_devices][0];
                    ccd_devices[ccd_num_devices][1].odd_field  = &ccd_devices[ccd_num_devices][1];
                    ccd_devices[ccd_num_devices][1].even_field = &ccd_devices[ccd_num_devices][2];
                    ccd_devices[ccd_num_devices][2].base       = &ccd_devices[ccd_num_devices][0];
                    ccd_devices[ccd_num_devices][2].odd_field  = &ccd_devices[ccd_num_devices][1];
                    ccd_devices[ccd_num_devices][2].even_field = &ccd_devices[ccd_num_devices][2];
                }
                else
                {
                    ccd_devices[ccd_num_devices][0].odd_field  = NULL;
                    ccd_devices[ccd_num_devices][0].even_field = NULL;
                }
                ccd_num_devices++;
            }
        }
        if (ccd_num_devices == 0)
        {
            gnome_warning_dialog_parented(_("No CCD cameras found!"), GTK_WINDOW(gnome_mdi_get_active_window(GNOME_MDI(mdi))));
            return;
        }
        /*
         * Create toplevel window for image acquisition.
         */
        imageAdd = new_image_child;
        gtk_widget_push_visual(gdk_rgb_get_visual());
        gtk_widget_push_colormap(gdk_rgb_get_cmap());
        gui_state.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_widget_pop_colormap();
        gtk_widget_pop_visual();
        gtk_window_set_title(GTK_WINDOW(gui_state.window), _("CCD Camera Controller"));
        gtk_signal_connect(GTK_OBJECT(gui_state.window), "delete_event", GTK_SIGNAL_FUNC(eventAIDelete), NULL);
        gtk_signal_connect(GTK_OBJECT(gui_state.window), "destroy",      GTK_SIGNAL_FUNC(eventAIDestroy), NULL);
        vbox_panels = gtk_vbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(gui_state.window), vbox_panels);
        hbox_panels = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_panels), hbox_panels, TRUE, TRUE, 0);
        {
            GtkWidget    *hbox1, *hbox2, *combo_ccd, *spin, *combo_time, *button_beep, *button_hide, *frame;
            static GList *list_time_scale = NULL;

            /*
             * Create common widgets.
             */
            hbox1     = gtk_hbox_new(FALSE, 0);
            hbox2     = gtk_hbox_new(FALSE, 0);
            combo_ccd = gtk_combo_new();
            gtk_combo_set_popdown_strings(GTK_COMBO(combo_ccd), list_ccds);
            gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(combo_ccd)->entry), FALSE);
            gtk_signal_connect(GTK_OBJECT(GTK_COMBO(combo_ccd)->entry), "changed", GTK_SIGNAL_FUNC(cbDeviceChanged), (gpointer)&ccd_state.exposure);
            if (ccd_state.exposure.ccd == NULL)
            {
                ccd_state.exposure.ccd      = &ccd_devices[0][0];
                ccd_state.exposure.dac_bits = ccd_state.exposure.ccd->dac_bits;
                ccd_state.exposure.width    = ccd_state.exposure.ccd->width;
                ccd_state.exposure.height   = ccd_state.exposure.ccd->height;
                ccd_state.exposure.xoffset  = 0;
                ccd_state.exposure.yoffset  = 0;
                ccd_state.exposure.xbin     = 1;
                ccd_state.exposure.ybin     = 1;
                ccd_state.focus.ccd         = &ccd_devices[0][0];
                ccd_state.focus.dac_bits    = ccd_state.focus.ccd->dac_bits / 2;
                ccd_state.focus.width       = ccd_state.focus.ccd->width;
                ccd_state.focus.height      = ccd_state.focus.ccd->height;
                ccd_state.focus.xoffset     = 0;
                ccd_state.focus.yoffset     = 0;
                ccd_state.focus.xbin        = 1;
                ccd_state.focus.ybin        = 1;
            }
            gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo_ccd)->entry), ccd_state.exposure.ccd->camera);
            if (ccd_state.exposure.time_count == 0)
                ccd_state.exposure.time_count = 1;
            spin = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)ccd_state.exposure.time_count, 0.0, 10000.0, 1.0, 0.0, 0.0)), 1.0, 0);
            gtk_signal_connect(GTK_OBJECT(spin), "changed", GTK_SIGNAL_FUNC(cbTimeChanged), (gpointer)&ccd_state.exposure);
            if (!list_time_scale)
            {
                list_time_scale = g_list_append(list_time_scale, _("msec"));
                list_time_scale = g_list_append(list_time_scale, _("second"));
                list_time_scale = g_list_append(list_time_scale, _("minute"));
            }
            combo_time = gtk_combo_new();
            gtk_combo_set_popdown_strings(GTK_COMBO(combo_time), list_time_scale);
            gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(combo_time)->entry), FALSE);
            gtk_signal_connect(GTK_OBJECT(GTK_COMBO(combo_time)->entry), "changed", GTK_SIGNAL_FUNC(cbTimeScaleChanged), (gpointer)&ccd_state.exposure);
            switch (ccd_state.exposure.time_scale)
            {
                case 60000:
                    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo_time)->entry), (gchar *)g_list_nth(list_time_scale, 2)->data);
                    break;
                case 1000:
                    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo_time)->entry), (gchar *)g_list_nth(list_time_scale, 1)->data);
                    break;
                case 1:
                default:
                    ccd_state.exposure.time_scale = 1;
                    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo_time)->entry), (gchar *)g_list_nth(list_time_scale, 0)->data);
                    break;
            }
            button_beep            = gtk_check_button_new_with_label(_("Beep when done"));
            gui_state.progress_bar = gtk_progress_bar_new_with_adjustment(GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 100.0, 1.0, 0.0, 0.0)));
            frame                  = gtk_frame_new(NULL);
            gui_state.status       = gtk_label_new("0/1");
            gui_state.button_exp   = gtk_button_new_with_label(_("Begin"));
            button_hide            = gtk_button_new_with_label(_("Hide"));
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_beep), gui_state.beep_when_done);
            gtk_signal_connect(GTK_OBJECT(button_beep), "toggled", GTK_SIGNAL_FUNC(cbBeepDone), NULL);
            gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(gui_state.progress_bar), GTK_PROGRESS_CONTINUOUS);
            gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(gui_state.progress_bar), GTK_PROGRESS_LEFT_TO_RIGHT);
            gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
            gtk_object_set(GTK_OBJECT(gui_state.status), "width", 60, NULL);
            gtk_signal_connect(GTK_OBJECT(gui_state.button_exp), "clicked", GTK_SIGNAL_FUNC(cbAcquireImage), NULL);
            gtk_signal_connect(GTK_OBJECT(button_hide), "clicked", GTK_SIGNAL_FUNC(cbHide), NULL);
            gtk_box_pack_end(GTK_BOX(vbox_panels), hbox2, FALSE, FALSE, 0);
            gtk_box_pack_end(GTK_BOX(vbox_panels), hbox1, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(hbox1), combo_ccd, TRUE, TRUE, 0);
            gtk_box_pack_start(GTK_BOX(hbox1), spin, TRUE, TRUE, 0);
            gtk_box_pack_start(GTK_BOX(hbox1), combo_time, TRUE, TRUE, 0);
            gtk_box_pack_end(GTK_BOX(hbox1), button_beep, TRUE, TRUE, 0);
            gtk_box_pack_start(GTK_BOX(hbox2), gui_state.progress_bar, TRUE, TRUE, 0);
            gtk_container_add(GTK_CONTAINER(frame), gui_state.status);
            gtk_box_pack_start(GTK_BOX(hbox2), frame, TRUE, TRUE, 0);
            gtk_box_pack_end(GTK_BOX(hbox2), button_hide, TRUE, TRUE, 0);
            gtk_box_pack_end(GTK_BOX(hbox2), gui_state.button_exp,  TRUE, TRUE, 0);
        }
        /*
         * Create telescope control panel
         */
        {
            GtkWidget *frame, *table, *toggle, *eventbox, *combo_iface;
            static GList *list_iface = NULL;

            scope_state.flags      = prefs.ScopeRA | prefs.ScopeDec | prefs.ScopeSwap;
            scope_state.init_delay = prefs.ScopeInitDelay;
            frame = gtk_frame_new(_("Telescope Control"));
            table = gtk_table_new(6, 3, FALSE);
            toggle = gtk_toggle_button_new_with_label(_("Rev X"));
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), scope_state.flags & SCOPE_REV_RA);
            gtk_signal_connect(GTK_OBJECT(toggle), "toggled", GTK_SIGNAL_FUNC(cbScopeOptions), (gpointer)SCOPE_REV_RA);
            gtk_table_attach(GTK_TABLE(table), toggle, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, /*GTK_EXPAND | */GTK_FILL, 0, 0);
            toggle = gtk_toggle_button_new_with_label(_("Rev Y"));
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), scope_state.flags & SCOPE_REV_DEC);
            gtk_signal_connect(GTK_OBJECT(toggle), "toggled", GTK_SIGNAL_FUNC(cbScopeOptions), (gpointer)SCOPE_REV_DEC);
            gtk_table_attach(GTK_TABLE(table), toggle, 2, 3, 0, 1, GTK_EXPAND | GTK_FILL, /*GTK_EXPAND | */GTK_FILL, 0, 0);
            toggle = gtk_toggle_button_new_with_label(_("Swap XY"));
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), scope_state.flags & SCOPE_SWAP_XY);
            gtk_signal_connect(GTK_OBJECT(toggle), "toggled", GTK_SIGNAL_FUNC(cbScopeOptions), (gpointer)SCOPE_SWAP_XY);
            gtk_table_attach(GTK_TABLE(table), toggle, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, /*GTK_EXPAND | */GTK_FILL, 0, 0);
            toggle = gtk_toggle_button_new_with_label(_("Slew"));
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), scope_state.flags & SCOPE_SLEW_TOGGLE);
            gtk_signal_connect(GTK_OBJECT(toggle), "toggled", GTK_SIGNAL_FUNC(cbScopeOptions), (gpointer)SCOPE_SLEW_TOGGLE);
            gtk_table_attach(GTK_TABLE(table), toggle, 0, 3, 4, 5, GTK_EXPAND | GTK_FILL, /*GTK_EXPAND | */GTK_FILL, 0, 0);
            eventbox = gtk_event_box_new();
            gtk_signal_connect(GTK_OBJECT(eventbox), "button_press_event",   GTK_SIGNAL_FUNC(cbScopeButton), (gpointer)SCOPE_UP);
            gtk_signal_connect(GTK_OBJECT(eventbox), "button_release_event", GTK_SIGNAL_FUNC(cbScopeButton), (gpointer)SCOPE_UP);
            gui_state.arrow_up = gtk_arrow_new(GTK_ARROW_UP, GTK_SHADOW_OUT);
            gtk_container_add(GTK_CONTAINER(eventbox), gui_state.arrow_up);
            gtk_table_attach(GTK_TABLE(table), eventbox, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
            eventbox = gtk_event_box_new();
            gtk_signal_connect(GTK_OBJECT(eventbox), "button_press_event",   GTK_SIGNAL_FUNC(cbScopeButton), (gpointer)SCOPE_DOWN);
            gtk_signal_connect(GTK_OBJECT(eventbox), "button_release_event", GTK_SIGNAL_FUNC(cbScopeButton), (gpointer)SCOPE_DOWN);
            gui_state.arrow_down = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
            gtk_container_add(GTK_CONTAINER(eventbox), gui_state.arrow_down);
            gtk_table_attach(GTK_TABLE(table), eventbox, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
            eventbox = gtk_event_box_new();
            gtk_signal_connect(GTK_OBJECT(eventbox), "button_press_event",   GTK_SIGNAL_FUNC(cbScopeButton), (gpointer)SCOPE_LEFT);
            gtk_signal_connect(GTK_OBJECT(eventbox), "button_release_event", GTK_SIGNAL_FUNC(cbScopeButton), (gpointer)SCOPE_LEFT);
            gui_state.arrow_left = gtk_arrow_new(GTK_ARROW_LEFT, GTK_SHADOW_OUT);
            gtk_misc_set_alignment(GTK_MISC(gui_state.arrow_left), 1.0, 0.5);
            gtk_container_add(GTK_CONTAINER(eventbox), gui_state.arrow_left);
            gtk_table_attach(GTK_TABLE(table), eventbox, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
            eventbox = gtk_event_box_new();
            gtk_signal_connect(GTK_OBJECT(eventbox), "button_press_event",   GTK_SIGNAL_FUNC(cbScopeButton), (gpointer)SCOPE_RIGHT);
            gtk_signal_connect(GTK_OBJECT(eventbox), "button_release_event", GTK_SIGNAL_FUNC(cbScopeButton), (gpointer)SCOPE_RIGHT);
            gui_state.arrow_right = gtk_arrow_new(GTK_ARROW_RIGHT, GTK_SHADOW_OUT);
            gtk_misc_set_alignment(GTK_MISC(gui_state.arrow_right), 0.0, 0.5);
            gtk_container_add(GTK_CONTAINER(eventbox), gui_state.arrow_right);
            gtk_table_attach(GTK_TABLE(table), eventbox, 2, 3, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
            if (prefs.PortScope >= 0)
            {
                scope_state.iface = prefs.ScopeIFace;
                if (prefs.PortScope >= 4)
                    sprintf(scope_state.filename, "/dev/ttysx%d", prefs.PortScope - 4);
                else
                    sprintf(scope_state.filename, "/dev/ttyS%d", prefs.PortScope);
                if (scope_connect(&scope_state) < 0)
                {
                    gnome_warning_dialog(_("Unable to connect to telescope.  Setting to manual"));
                    scope_release(&scope_state);
                    scope_state.iface = SCOPE_MANUAL;
                }
                else
                    scope_release(&scope_state);
            }
            else
                scope_state.iface = SCOPE_MANUAL;
            if (!list_iface)
            {
                list_iface = g_list_append(list_iface, _("Manual"));
                list_iface = g_list_append(list_iface, _("STAR 2000"));
                list_iface = g_list_append(list_iface, _("LX 200"));
            }
            combo_iface = gtk_combo_new();
            gtk_combo_set_popdown_strings(GTK_COMBO(combo_iface), list_iface);
            gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(combo_iface)->entry), FALSE);
            gtk_signal_connect(GTK_OBJECT(GTK_COMBO(combo_iface)->entry), "changed", GTK_SIGNAL_FUNC(cbScopeMode), NULL);
            switch (scope_state.iface)
            {
                case SCOPE_LX200:
                    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo_iface)->entry), (gchar *)g_list_nth(list_iface, 2)->data);
                    break;
                case SCOPE_STAR2K:
                    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo_iface)->entry), (gchar *)g_list_nth(list_iface, 1)->data);
                    break;
                case SCOPE_MANUAL:
                default:
                    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo_iface)->entry), (gchar *)g_list_nth(list_iface, 0)->data);
                    break;
            }
            gtk_table_attach(GTK_TABLE(table), combo_iface, 0, 3, 5, 6, GTK_EXPAND | GTK_FILL, /*GTK_EXPAND | */GTK_FILL, 0, 0);
            gtk_container_add(GTK_CONTAINER(frame), table);
            gtk_box_pack_end(GTK_BOX(hbox_panels), frame, TRUE, TRUE, 0);
        }
        /*
         * Create notebook to hold different control panes.
         */
        {
            gui_state.notebook = gtk_notebook_new();
            gtk_notebook_set_tab_pos(GTK_NOTEBOOK(gui_state.notebook), GTK_POS_TOP);
            gtk_signal_connect(GTK_OBJECT(gui_state.notebook), "switch_page", GTK_SIGNAL_FUNC(cbSwitchPage), NULL);
            gtk_box_pack_start(GTK_BOX(hbox_panels), gui_state.notebook, TRUE, TRUE, 0);
            /*
             * Build exposure option pane.
             */
            {
                GtkWidget *hbox_opt, *frame, *vbox, *hbox, *vbox_bin, *radiobutton;
                GtkWidget *vbox_cal, *label, *vbox_mul, *entry, *spin, *button;

                hbox_opt = gtk_hbox_new(FALSE, 0);
                vbox     = gtk_vbox_new(FALSE, 0);
                frame    = gtk_frame_new(_("X Bin"));
                vbox_bin = gtk_vbox_new(FALSE, 0);
                if (ccd_state.exposure.xbin == 0)
                    ccd_state.exposure.xbin = 1;
                radiobutton = gtk_radio_button_new_with_label(NULL, "1X");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbXBinMode), (gpointer)1);
                gtk_box_pack_start(GTK_BOX(vbox_bin), radiobutton, TRUE, TRUE, 0);
                radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "2X");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbXBinMode), (gpointer)2);
                if (ccd_state.exposure.xbin == 2)
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
                gtk_box_pack_start(GTK_BOX(vbox_bin), radiobutton, TRUE, TRUE, 0);
                radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "3X");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbXBinMode), (gpointer)3);
                if (ccd_state.exposure.xbin == 3)
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
                gtk_box_pack_start(GTK_BOX(vbox_bin), radiobutton, TRUE, TRUE, 0);
                radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "4X");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbXBinMode), (gpointer)4);
                if (ccd_state.exposure.xbin == 4)
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
                gtk_box_pack_start(GTK_BOX(vbox_bin), radiobutton, TRUE, TRUE, 0);
                gtk_container_add(GTK_CONTAINER(frame), vbox_bin);
                gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
                frame    = gtk_frame_new(_("Y Bin"));
                vbox_bin = gtk_vbox_new(FALSE, 0);
                if (ccd_state.exposure.ybin == 0)
                    ccd_state.exposure.ybin = 1;
                radiobutton = gtk_radio_button_new_with_label(NULL, "1/2Y");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbInterleaveBinMode), (gpointer)1);
                gtk_box_pack_start(GTK_BOX(vbox_bin), radiobutton, TRUE, TRUE, 0);
                if (ccd_state.exposure.ybin == 1 && ccd_state.bin_fields == FIELD_INTERLEAVE)
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
                radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "1Y");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbYBinMode), (gpointer)1);
                gtk_box_pack_start(GTK_BOX(vbox_bin), radiobutton, TRUE, TRUE, 0);
                if (ccd_state.exposure.ybin == 1 && ccd_state.bin_fields == FIELD_BOTH)
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
                radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "2Y");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbYBinMode), (gpointer)2);
                if (ccd_state.exposure.ybin == 2)
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
                gtk_box_pack_start(GTK_BOX(vbox_bin), radiobutton, TRUE, TRUE, 0);
                radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "3Y");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbYBinMode), (gpointer)3);
                if (ccd_state.exposure.ybin == 3)
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
                gtk_box_pack_start(GTK_BOX(vbox_bin), radiobutton, TRUE, TRUE, 0);
                radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "4Y");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbYBinMode), (gpointer)4);
                if (ccd_state.exposure.ybin == 4)
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
                gtk_box_pack_start(GTK_BOX(vbox_bin), radiobutton, TRUE, TRUE, 0);
                gtk_container_add(GTK_CONTAINER(frame), vbox_bin);
                gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
                gtk_box_pack_start(GTK_BOX(hbox_opt), vbox, TRUE, TRUE, 0);
                vbox_mul             = gtk_vbox_new(FALSE, 0);
                frame                = gtk_frame_new(_("Calibration Frames"));
                vbox_cal             = gtk_vbox_new(FALSE, 0);
                gui_state.combo_bias = gtk_combo_new();
                gui_state.combo_dark = gtk_combo_new();
                gui_state.combo_flat = gtk_combo_new();
                imageUpdateList();
                gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(gui_state.combo_bias)->entry), FALSE);
                gtk_signal_connect(GTK_OBJECT(GTK_COMBO(gui_state.combo_bias)->entry), "changed", GTK_SIGNAL_FUNC(cbCalibrationChanged), (gpointer)GCCD_SET_BIAS);
                vbox  = gtk_vbox_new(FALSE, 0);
                label = gtk_label_new(_("Bias Frame or Standard Dark Frame:"));
                gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox), gui_state.combo_bias, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox_cal), vbox, TRUE, TRUE, 0);
                gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(gui_state.combo_dark)->entry), FALSE);
                gtk_signal_connect(GTK_OBJECT(GTK_COMBO(gui_state.combo_dark)->entry), "changed", GTK_SIGNAL_FUNC(cbCalibrationChanged), (gpointer)GCCD_SET_DARK);
                vbox  = gtk_vbox_new(FALSE, 0);
                label = gtk_label_new(_("Scalable Dark Frame:"));
                gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox), gui_state.combo_dark, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox_cal), vbox, TRUE, TRUE, 0);
                gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(gui_state.combo_flat)->entry), FALSE);
                gtk_signal_connect(GTK_OBJECT(GTK_COMBO(gui_state.combo_flat)->entry), "changed", GTK_SIGNAL_FUNC(cbCalibrationChanged), (gpointer)GCCD_SET_FLAT);
                vbox  = gtk_vbox_new(FALSE, 0);
                label = gtk_label_new(_("Flat Frame:"));
                gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox), gui_state.combo_flat, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox_cal), vbox, TRUE, TRUE, 0);
                gtk_container_add(GTK_CONTAINER(frame), vbox_cal);
                gtk_box_pack_start(GTK_BOX(vbox_mul), frame, TRUE, TRUE, 0);
                if (ccd_state.exp_basename[0] == '\0')
                    strcpy(ccd_state.exp_basename, _("Image"));
                if (ccd_state.exp_basenum == 0)
                    ccd_state.exp_basenum = 1;
                frame                  = gtk_frame_new(_("Image Name Base"));
                hbox                   = gtk_hbox_new(FALSE, 0);
                entry                  = gtk_entry_new_with_max_length(NAME_STRING_LENGTH/2);
                label                  = gtk_label_new("-");
                gui_state.spin_basenum = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(ccd_state.exp_basenum, 1.0, 10000.0, 1.0, 0.0, 0.0)), 1.0, 0);
                gtk_entry_set_text(GTK_ENTRY(entry), ccd_state.exp_basename);
                gtk_object_set(GTK_OBJECT(entry), "width", 30, NULL);
                gtk_signal_connect(GTK_OBJECT(entry), "changed", GTK_SIGNAL_FUNC(cbBaseNameChanged), NULL);
                gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
                gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
                gtk_signal_connect(GTK_OBJECT(gui_state.spin_basenum), "changed", GTK_SIGNAL_FUNC(cbBaseNumChanged), NULL);
                gtk_box_pack_end(GTK_BOX(hbox), gui_state.spin_basenum, FALSE, FALSE, 0);
                gtk_container_add(GTK_CONTAINER(frame), hbox);
                gtk_box_pack_start(GTK_BOX(vbox_mul), frame, TRUE, TRUE, 0);
                button = gtk_check_button_new_with_label(_("Auto-Save"));
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), gui_state.auto_save);
                gtk_signal_connect(GTK_OBJECT(button), "toggled", GTK_SIGNAL_FUNC(cbSaveMode), NULL);
                gtk_box_pack_end(GTK_BOX(vbox_mul), button, TRUE, TRUE, 0);
                gtk_box_pack_start(GTK_BOX(hbox_opt), vbox_mul, TRUE, TRUE, 0);
                if (!ccd_state.exp_count)
                    ccd_state.exp_count = 1;
                frame    = gtk_frame_new(_("Multiple Exposures"));
                vbox_mul = gtk_vbox_new(FALSE, 0);
                hbox     = gtk_hbox_new(FALSE, 0);
                label    = gtk_label_new(_("Count:"));
                spin     = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)ccd_state.exp_count, 1.0, MAX_EXPOSURES, 1.0, 0.0, 0.0)), 1.0, 0);
                gtk_signal_connect(GTK_OBJECT(spin), "changed", GTK_SIGNAL_FUNC(cbExpCountChanged), (gpointer)&ccd_state.exposure);
                gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                gtk_object_set(GTK_OBJECT(label), "width", 40, NULL);
                gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox_mul), hbox, TRUE, TRUE, 0);
                button = gtk_check_button_new_with_label(_("Auto-Register"));
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), ccd_state.exp_tracknstack);
                gtk_signal_connect(GTK_OBJECT(button), "toggled", GTK_SIGNAL_FUNC(cbTrackMode), NULL);
                radiobutton = gtk_radio_button_new_with_label(NULL, "Seperate");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbCombMode), (gpointer)GCCD_NO_COMB);
                if (ccd_state.exp_comb == GCCD_NO_COMB)
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
                gtk_box_pack_start(GTK_BOX(vbox_mul), radiobutton, TRUE, TRUE, 0);
                radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "Sum");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbCombMode), (gpointer)CCD_IMAGE_COMBINE_SUM);
                if (ccd_state.exp_comb == CCD_IMAGE_COMBINE_MEAN)
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
                gtk_box_pack_start(GTK_BOX(vbox_mul), radiobutton, TRUE, TRUE, 0);
                radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "Average");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbCombMode), (gpointer)CCD_IMAGE_COMBINE_MEAN);
                if (ccd_state.exp_comb == CCD_IMAGE_COMBINE_SUM)
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
                gtk_box_pack_start(GTK_BOX(vbox_mul), radiobutton, TRUE, TRUE, 0);
                radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "Median");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbCombMode), (gpointer)CCD_IMAGE_COMBINE_MEDIAN);
                if (ccd_state.exp_comb == CCD_IMAGE_COMBINE_MEDIAN)
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
                gtk_box_pack_start(GTK_BOX(vbox_mul), radiobutton, TRUE, TRUE, 0);
                radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "Min");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbCombMode), (gpointer)CCD_IMAGE_COMBINE_MIN);
                if (ccd_state.exp_comb == CCD_IMAGE_COMBINE_MEDIAN)
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
                gtk_box_pack_start(GTK_BOX(vbox_mul), radiobutton, TRUE, TRUE, 0);
                radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "Max");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbCombMode), (gpointer)CCD_IMAGE_COMBINE_MAX);
                if (ccd_state.exp_comb == CCD_IMAGE_COMBINE_MEDIAN)
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
                gtk_box_pack_start(GTK_BOX(vbox_mul), radiobutton, TRUE, TRUE, 0);
                gtk_box_pack_end(GTK_BOX(vbox_mul), button, TRUE, TRUE, 0);
                gtk_container_add(GTK_CONTAINER(frame), vbox_mul);
                gtk_box_pack_start(GTK_BOX(hbox_opt), frame, TRUE, TRUE, 0);
                gtk_notebook_append_page(GTK_NOTEBOOK(gui_state.notebook), hbox_opt, gtk_label_new(_("Options")));
            }
            /*
             * Build focus pane.
             */
            {
                int          scale;
                GtkWidget   *hbox, *hbox_scale, *vbox, *vbox_scale, *vbox_hist, *vbox_focus, *hbox_focus, *frame, *frame_hist, *label, *scrolled_win, *scrollbar, *radiobutton, *eventbox;
                GtkObject   *adj;

                hbox         = gtk_hbox_new(FALSE, 0);
                hbox_scale   = gtk_hbox_new(FALSE, 0);
                scrolled_win = gtk_scrolled_window_new(NULL, NULL);
                gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
                gui_state.view_focus = gtk_drawing_area_new();
                if (ccd_state.focus_scale == 0)
                    ccd_state.focus_scale = FOCUS_SCALE_4_1;
                if (ccd_state.focus_scale > FOCUS_SCALE_1_1)
                {
                    scale                   = (ccd_state.focus_scale - FOCUS_SCALE_1_1) * 2;
                    ccd_state.focus_width   = ccd_state.focus.ccd->width  * scale;
                    ccd_state.focus_height  = ccd_state.focus.ccd->height * scale;
                    sprintf(str, "1:%d", scale);
                }
                else
                {
                    if ((scale = (FOCUS_SCALE_1_1 - ccd_state.focus_scale) * 2) == 0)
                        scale = 1;
                    ccd_state.focus_width   = ccd_state.focus.ccd->width  / scale;
                    ccd_state.focus_height  = ccd_state.focus.ccd->height / scale;
                    sprintf(str, "%d:1", scale);
                }
                gtk_drawing_area_size(GTK_DRAWING_AREA(gui_state.view_focus), ccd_state.focus_width, ccd_state.focus_height);
                gtk_signal_connect(GTK_OBJECT(gui_state.view_focus), "expose_event", GTK_SIGNAL_FUNC(cbFocusExpose), NULL);
                gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_win), gui_state.view_focus);
                gtk_box_pack_start(GTK_BOX(hbox_scale), scrolled_win, TRUE, TRUE, 0);
                vbox_scale  = gtk_vbox_new(FALSE, 0);
                adj         = gtk_adjustment_new((gfloat)ccd_state.focus_scale, 1.0, 6.0, 1.0, 1.0, 1.0);
                label       = gtk_label_new(str);
                scrollbar   = gtk_vscrollbar_new(GTK_ADJUSTMENT(adj));
                gtk_box_pack_start(GTK_BOX(vbox_scale), label, FALSE, FALSE, 0);
                gtk_signal_connect(GTK_OBJECT(adj), "value_changed", GTK_SIGNAL_FUNC(cbFocusScaleChanged), (gpointer)label);
                gtk_box_pack_start(GTK_BOX(vbox_scale), scrollbar, TRUE, TRUE, 0);
                gtk_box_pack_start(GTK_BOX(hbox_scale), vbox_scale, FALSE, FALSE, 0);
                frame = gtk_frame_new(NULL);
                gtk_container_add(GTK_CONTAINER(frame), hbox_scale);
                gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 0);
                vbox                     = gtk_vbox_new(FALSE, 0);
                frame_hist               = gtk_frame_new(_("ADU Histogram"));
                vbox_hist                = gtk_vbox_new(FALSE, 0);
                gui_state.draw_histogram = gtk_drawing_area_new();
                gtk_drawing_area_size(GTK_DRAWING_AREA(gui_state.draw_histogram), 128, 80);
                gtk_signal_connect(GTK_OBJECT(gui_state.draw_histogram), "expose_event", GTK_SIGNAL_FUNC(cbHistogramExpose), NULL);
                frame = gtk_frame_new(NULL);
                gtk_container_add(GTK_CONTAINER(frame), gui_state.draw_histogram);
                gtk_box_pack_start(GTK_BOX(vbox_hist), frame, FALSE, FALSE, 0);
                gui_state.label_minmax = gtk_label_new("Min:0\nAve:0\nMax:0");
                frame                  = gtk_frame_new(NULL);
                gtk_container_add(GTK_CONTAINER(frame), gui_state.label_minmax);
                gtk_box_pack_start(GTK_BOX(vbox_hist), frame, TRUE, TRUE, 0);
                gtk_container_add(GTK_CONTAINER(frame_hist), vbox_hist);
                gtk_box_pack_start(GTK_BOX(vbox), frame_hist, FALSE, FALSE, 0);
                gui_state.focus_control = gtk_frame_new(_("Focus Control"));
                hbox_focus              = gtk_hbox_new(FALSE, 0);
                vbox_focus              = gtk_vbox_new(FALSE, 0);
                radiobutton             = gtk_radio_button_new_with_label(NULL, "Slow");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbScopeOptions), (gpointer)SCOPE_FOCUS_SLOW);
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
                gtk_box_pack_start(GTK_BOX(vbox_focus), radiobutton, TRUE, TRUE, 0);
                radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "Med");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbScopeOptions), (gpointer)SCOPE_FOCUS_MED);
                gtk_box_pack_start(GTK_BOX(vbox_focus), radiobutton, TRUE, TRUE, 0);
                radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "Fast");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbScopeOptions), (gpointer)SCOPE_FOCUS_FAST);
                gtk_box_pack_start(GTK_BOX(vbox_focus), radiobutton, TRUE, TRUE, 0);
                gtk_box_pack_start(GTK_BOX(hbox_focus), vbox_focus, FALSE, FALSE, 0);
                vbox_focus = gtk_vbox_new(FALSE, 0);
                eventbox   = gtk_event_box_new();
                gtk_signal_connect(GTK_OBJECT(eventbox), "button_press_event",   GTK_SIGNAL_FUNC(cbScopeButton), (gpointer)SCOPE_FOCUS_IN);
                gtk_signal_connect(GTK_OBJECT(eventbox), "button_release_event", GTK_SIGNAL_FUNC(cbScopeButton), (gpointer)SCOPE_FOCUS_IN);
                gui_state.arrow_in = gtk_arrow_new(GTK_ARROW_UP, GTK_SHADOW_OUT);
                gtk_container_add(GTK_CONTAINER(eventbox), gui_state.arrow_in);
                gtk_box_pack_start(GTK_BOX(vbox_focus), eventbox, TRUE, TRUE, 0);
                gtk_box_pack_start(GTK_BOX(vbox_focus), gtk_label_new("+/-"), FALSE, FALSE, 0);
                eventbox = gtk_event_box_new();
                gtk_signal_connect(GTK_OBJECT(eventbox), "button_press_event",   GTK_SIGNAL_FUNC(cbScopeButton), (gpointer)SCOPE_FOCUS_OUT);
                gtk_signal_connect(GTK_OBJECT(eventbox), "button_release_event", GTK_SIGNAL_FUNC(cbScopeButton), (gpointer)SCOPE_FOCUS_OUT);
                gui_state.arrow_out = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
                gtk_container_add(GTK_CONTAINER(eventbox), gui_state.arrow_out);
                gtk_box_pack_start(GTK_BOX(vbox_focus), eventbox, TRUE, TRUE, 0);
                gtk_box_pack_start(GTK_BOX(hbox_focus), vbox_focus, TRUE, TRUE, 0);
                gtk_container_add(GTK_CONTAINER(gui_state.focus_control), hbox_focus);
                gtk_box_pack_start(GTK_BOX(vbox), gui_state.focus_control, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
                gtk_notebook_append_page(GTK_NOTEBOOK(gui_state.notebook), hbox, gtk_label_new(_("Find & Focus")));
                gtk_widget_set_sensitive(gui_state.focus_control, scope_state.iface == SCOPE_LX200);
            }
            /*
             * Build guiding pane.
             */
            {
                GtkWidget *hbox, *vbox, *hbox_button, *vbox_button, *frame_view, *combo, *button, *radiobutton, *label, *frame;

                hbox                 = gtk_hbox_new(FALSE, 0);
                frame_view           = gtk_frame_new(_("Reticule"));
                vbox                 = gtk_vbox_new(FALSE, 0);
                gui_state.view_guide = gtk_drawing_area_new();
                gtk_signal_connect(GTK_OBJECT(gui_state.view_guide), "expose_event", GTK_SIGNAL_FUNC(cbGuideExpose), NULL);
                gtk_drawing_area_size(GTK_DRAWING_AREA(gui_state.view_guide), GUIDE_WIDTH*GUIDE_SCALE, GUIDE_HEIGHT*GUIDE_SCALE);
                frame = gtk_frame_new(NULL);
                gtk_container_add(GTK_CONTAINER(frame), gui_state.view_guide);
                gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
                ccd_state.guide_dx_centroid = 0.0;
                ccd_state.guide_dy_centroid = 0.0;
                sprintf(str, _("Offset (%.2f, %.2f)"), ccd_state.guide_dx_centroid, ccd_state.guide_dy_centroid);
                gui_state.label_offset = gtk_label_new(str);
                frame                  = gtk_frame_new(NULL);
                gtk_container_add(GTK_CONTAINER(frame), gui_state.label_offset);
                gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
                combo = gtk_combo_new();
                gtk_combo_set_popdown_strings(GTK_COMBO(combo), list_ccds);
                gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(combo)->entry), FALSE);
                gtk_signal_connect(GTK_OBJECT(GTK_COMBO(combo)->entry), "changed", GTK_SIGNAL_FUNC(cbGuideDevChanged), (gpointer)&ccd_state.guide);
                if (ccd_state.guide.ccd == NULL)
                {
                    ccd_state.guide.ccd      = &ccd_devices[0][0];
                    ccd_state.guide.dac_bits = ccd_state.guide.ccd->dac_bits;
                    ccd_state.guide.width    = GUIDE_WIDTH;
                    ccd_state.guide.height   = GUIDE_HEIGHT;
                    ccd_state.guide.xoffset  = (ccd_state.guide.ccd->width  - GUIDE_WIDTH)  / 2;
                    ccd_state.guide.yoffset  = (ccd_state.guide.ccd->height - GUIDE_HEIGHT) / 2;
                    ccd_state.guide.xbin     = 1;
                    ccd_state.guide.ybin     = 1;
                }
                gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry), ccd_state.guide.ccd->camera);
                gtk_box_pack_end(GTK_BOX(vbox), combo, TRUE, TRUE, 0);
                gtk_container_add(GTK_CONTAINER(frame_view), vbox);
                gtk_box_pack_start(GTK_BOX(hbox), frame_view, FALSE, FALSE, 0);
                ccd_state.guide_fields = prefs.TrackSelf;
                vbox                 = gtk_vbox_new(FALSE, 0);
                frame                = gtk_frame_new(_("Self-Guide"));
                vbox_button          = gtk_vbox_new(FALSE, 0);
                radiobutton          = gtk_radio_button_new_with_label(NULL, "Odd Field");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbSelfGuideMode), (gpointer)FIELD_ODD);
                if (ccd_state.guide_fields == FIELD_ODD)
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
                gtk_box_pack_start(GTK_BOX(vbox_button), radiobutton, TRUE, TRUE, 0);
                radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "Even Field");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbSelfGuideMode), (gpointer)FIELD_EVEN);
                if (ccd_state.guide_fields == FIELD_EVEN)
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
                gtk_box_pack_start(GTK_BOX(vbox_button), radiobutton, TRUE, TRUE, 0);
                radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "Both Fields");
                gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbSelfGuideMode), (gpointer)FIELD_BOTH);
                if (ccd_state.guide_fields == FIELD_BOTH)
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
                gtk_box_pack_start(GTK_BOX(vbox_button), radiobutton, TRUE, TRUE, 0);
                button = gtk_check_button_new_with_label(_("Ignore Errors"));
                gtk_signal_connect(GTK_OBJECT(button), "toggled", GTK_SIGNAL_FUNC(cbSelfGuideMode), (gpointer)FIELD_DARK);
                if (ccd_state.guide_dark)
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
                gtk_box_pack_end(GTK_BOX(vbox_button), button, TRUE, TRUE, 0);
                gtk_container_add(GTK_CONTAINER(frame), vbox_button);
                gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
                gui_state.button_guide = gtk_check_button_new_with_label(_("Auto-Guide"));
                gtk_signal_connect(GTK_OBJECT(gui_state.button_guide), "toggled", GTK_SIGNAL_FUNC(cbGuide), NULL);
                gtk_box_pack_end(GTK_BOX(vbox), gui_state.button_guide, TRUE, TRUE, 0);
                gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
                vbox        = gtk_vbox_new(FALSE, 0);
                frame       = gtk_frame_new(_("Tracking pix/sec"));
                vbox_button = gtk_vbox_new(FALSE, 0);
                ccd_state.guide_track_up    = prefs.TrackUp;
                ccd_state.guide_track_down  = prefs.TrackDown;
                ccd_state.guide_track_left  = prefs.TrackLeft;
                ccd_state.guide_track_right = prefs.TrackRight;
                gui_state.spin_track_up     = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(ccd_state.guide_track_up, 0.01, 10.0, 0.01, 0.0, 0.0)), 1.0, 2);
                hbox_button                 = gtk_hbox_new(FALSE, 0);
                label                       = gtk_label_new(_("Up:"));
                gtk_signal_connect(GTK_OBJECT(gui_state.spin_track_up), "changed", GTK_SIGNAL_FUNC(cbTrackRateChanged), (gpointer)SCOPE_UP);
                gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                gtk_object_set(GTK_OBJECT(label), "width", 40, NULL);
                gtk_box_pack_start(GTK_BOX(hbox_button), label, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hbox_button), gui_state.spin_track_up, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox_button), hbox_button, TRUE, TRUE, 0);
                gui_state.spin_track_down = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(ccd_state.guide_track_down, 0.01, 10.0, 0.01, 0.0, 0.0)), 1.0, 2);
                hbox_button               = gtk_hbox_new(FALSE, 0);
                label                     = gtk_label_new(_("Down:"));
                gtk_signal_connect(GTK_OBJECT(gui_state.spin_track_down), "changed", GTK_SIGNAL_FUNC(cbTrackRateChanged), (gpointer)SCOPE_DOWN);
                gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                gtk_object_set(GTK_OBJECT(label), "width", 40, NULL);
                gtk_box_pack_start(GTK_BOX(hbox_button), label, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hbox_button), gui_state.spin_track_down, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox_button), hbox_button, TRUE, TRUE, 0);
                gui_state.spin_track_left = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(ccd_state.guide_track_left, 0.01, 10.0, 0.01, 0.0, 0.0)), 1.0, 2);
                hbox_button               = gtk_hbox_new(FALSE, 0);
                label                     = gtk_label_new(_("Left:"));
                gtk_signal_connect(GTK_OBJECT(gui_state.spin_track_left), "changed", GTK_SIGNAL_FUNC(cbTrackRateChanged), (gpointer)SCOPE_LEFT);
                gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                gtk_object_set(GTK_OBJECT(label), "width", 40, NULL);
                gtk_box_pack_start(GTK_BOX(hbox_button), label, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hbox_button), gui_state.spin_track_left, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox_button), hbox_button, TRUE, TRUE, 0);
                gui_state.spin_track_right = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(ccd_state.guide_track_right, 0.01, 10.0, 0.01, 0.0, 0.0)), 1.0, 2);
                hbox_button                = gtk_hbox_new(FALSE, 0);
                label                      = gtk_label_new(_("Right:"));
                gtk_signal_connect(GTK_OBJECT(gui_state.spin_track_right), "changed", GTK_SIGNAL_FUNC(cbTrackRateChanged), (gpointer)SCOPE_RIGHT);
                gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                gtk_object_set(GTK_OBJECT(label), "width", 40, NULL);
                gtk_box_pack_start(GTK_BOX(hbox_button), label, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hbox_button), gui_state.spin_track_right, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox_button), hbox_button, TRUE, TRUE, 0);
                button = gtk_button_new_with_label(_("Train"));
                gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(cbGuideTrain), NULL);
                gtk_box_pack_start(GTK_BOX(vbox_button), button, TRUE, TRUE, 0);
                gtk_container_add(GTK_CONTAINER(frame), vbox_button);
                gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
                frame       = gtk_frame_new(_("Guide Object"));
                hbox_button = gtk_hbox_new(FALSE, 0);
                button = gtk_button_new_with_label(_("Select"));
                gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(cbGuideSelect), NULL);
                gtk_box_pack_start(GTK_BOX(hbox_button), button, TRUE, TRUE, 0);
                button = gtk_button_new_with_label(_("Clear"));
                gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(cbGuideClear), NULL);
                gtk_box_pack_start(GTK_BOX(hbox_button), button, TRUE, TRUE, 0);
                gtk_container_add(GTK_CONTAINER(frame), hbox_button);
                gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
                gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
                gtk_notebook_append_page(GTK_NOTEBOOK(gui_state.notebook), hbox, gtk_label_new(_("Guide")));
            }
            /*
             * Build filter wheel pane.
             */
            {
                GtkWidget *table, *button, *combo, *spin, *label, *frame;

                if (prefs.PortWheel >= 0)
                {
                    if (prefs.PortWheel >= 4)
                        sprintf(wheel_state.wheel.filename, "/dev/ttysx%d", prefs.PortWheel - 4);
                    else
                        sprintf(wheel_state.wheel.filename, "/dev/ttyS%d", prefs.PortWheel);
                    wheel_state.wheel.current = 1;
                    if (wheel_connect(&wheel_state.wheel) >= 0)
                        wheel_state.input_tag = gdk_input_add(wheel_state.wheel.fd, GDK_INPUT_READ, (GdkInputFunction)cbReadWheel, NULL);
                    else
                        wheel_release(&wheel_state.wheel);
                }
                else
                    wheel_state.wheel.fd = 0;
                table = gtk_table_new(5, 7, FALSE);
                if (!wheel_state.list_filters)
                {
                    wheel_state.filter_mask[0] = CCD_COLOR_MONOCHROME;
                    wheel_state.list_filters   = g_list_append(wheel_state.list_filters, _("(None)"));
                    wheel_state.filter_mask[1] = 0x0000;
                    wheel_state.list_filters   = g_list_append(wheel_state.list_filters, COLOR_MASK_TO_NAME(wheel_state.filter_mask[1]));
                    wheel_state.filter_mask[2] = CCD_COLOR_MONOCHROME;
                    wheel_state.list_filters   = g_list_append(wheel_state.list_filters, _("Clear"));
                    wheel_state.filter_mask[3] = CCD_COLOR_MATRIX_RED_MASK;
                    wheel_state.list_filters   = g_list_append(wheel_state.list_filters, COLOR_MASK_TO_NAME(wheel_state.filter_mask[3]));
                    wheel_state.filter_mask[4] = CCD_COLOR_MATRIX_GREEN_MASK;
                    wheel_state.list_filters   = g_list_append(wheel_state.list_filters, COLOR_MASK_TO_NAME(wheel_state.filter_mask[4]));
                    wheel_state.filter_mask[5] = CCD_COLOR_MATRIX_BLUE_MASK;
                    wheel_state.list_filters   = g_list_append(wheel_state.list_filters, COLOR_MASK_TO_NAME(wheel_state.filter_mask[5]));
                    wheel_state.filter_mask[6] = CCD_COLOR_MATRIX_GREEN_MASK | CCD_COLOR_MATRIX_BLUE_MASK;
                    wheel_state.list_filters   = g_list_append(wheel_state.list_filters, COLOR_MASK_TO_NAME(wheel_state.filter_mask[6]));
                    wheel_state.filter_mask[7] = CCD_COLOR_MATRIX_RED_MASK   | CCD_COLOR_MATRIX_BLUE_MASK;
                    wheel_state.list_filters   = g_list_append(wheel_state.list_filters, COLOR_MASK_TO_NAME(wheel_state.filter_mask[7]));
                    wheel_state.filter_mask[8] = CCD_COLOR_MATRIX_GREEN_MASK | CCD_COLOR_MATRIX_RED_MASK;
                    wheel_state.list_filters   = g_list_append(wheel_state.list_filters, COLOR_MASK_TO_NAME(wheel_state.filter_mask[8]));
                    wheel_state.filter_mask[9] = CCD_COLOR_MONOCHROME;
                    wheel_state.list_filters   = g_list_append(wheel_state.list_filters, _("Other"));
                }
                for (i = 0; i < MAX_FILTERS; i++)
                {
                    sprintf(str, _("Filter #%d"), i + 1);
                    wheel_state.filter[i]         = prefs.Filter[i];
                    wheel_state.exp_percent[i]    = prefs.FilterExp[i];
                    gui_state.radio_filter[i]     = gtk_radio_button_new_with_label(i ? gtk_radio_button_group(GTK_RADIO_BUTTON(gui_state.radio_filter[i - 1])) : NULL, str);
                    combo                         = gtk_combo_new();
                    button                        = gtk_check_button_new_with_label(_("Seq."));
                    spin                          = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)wheel_state.exp_percent[i], 0.0, 1000.0, 1.0, 0.0, 0.0)), 25.0, 0);
                    label                         = gtk_label_new("%");
                    gui_state.combo_filterflat[i] = gtk_combo_new();
                    gtk_signal_connect(GTK_OBJECT(gui_state.radio_filter[i]), "toggled", GTK_SIGNAL_FUNC(cbWheelGoto), (gpointer)i);
                    if (wheel_state.wheel.current == i + 1)
                        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gui_state.radio_filter[i]), TRUE);
                    gtk_object_set(GTK_OBJECT(GTK_COMBO(combo)->entry), "width", 40, NULL);
                    gtk_combo_set_popdown_strings(GTK_COMBO(combo), wheel_state.list_filters);
                    gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(combo)->entry), FALSE);
                    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry), g_list_nth(wheel_state.list_filters, wheel_state.filter[i])->data);
                    gtk_signal_connect(GTK_OBJECT(GTK_COMBO(combo)->entry), "changed", GTK_SIGNAL_FUNC(cbFilterTypeChanged), (gpointer)i);
                    gtk_object_set(GTK_OBJECT(button), "width", 30, NULL);
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), wheel_state.sequence[i]);
                    gtk_signal_connect(GTK_OBJECT(button), "toggled", GTK_SIGNAL_FUNC(cbSequenceMode), (gpointer)i);
                    gtk_signal_connect(GTK_OBJECT(spin), "changed", GTK_SIGNAL_FUNC(cbExpPercentChanged), (gpointer)i);
                    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                    gtk_object_set(GTK_OBJECT(GTK_COMBO(gui_state.combo_filterflat[i])->entry), "width", 60, NULL);
                    gtk_combo_set_popdown_strings(GTK_COMBO(gui_state.combo_filterflat[i]), gui_state.list_images);
                    gtk_signal_connect(GTK_OBJECT(GTK_COMBO(gui_state.combo_filterflat[i])->entry), "changed", GTK_SIGNAL_FUNC(cbFilterFlatChanged), (gpointer)i);
                    gtk_table_attach(GTK_TABLE(table), gui_state.radio_filter[i],     0, 1, i, i + 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
                    gtk_table_attach(GTK_TABLE(table), combo,                         1, 2, i, i + 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
                    gtk_table_attach(GTK_TABLE(table), button,                        2, 3, i, i + 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
                    gtk_table_attach(GTK_TABLE(table), spin,                          3, 4, i, i + 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
                    gtk_table_attach(GTK_TABLE(table), label,                         4, 5, i, i + 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
                    gtk_table_attach(GTK_TABLE(table), gui_state.combo_filterflat[i], 5, 6, i, i + 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
                }
                button = gtk_button_new_with_label(_("Home/Reset"));
                gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(cbWheelReset), NULL);
                gtk_table_attach(GTK_TABLE(table), button, 0, 1, 6, 7, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
                frame                  = gtk_frame_new(NULL);
                gui_state.wheel_status = gtk_label_new(_("Idle"));
                gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
                gtk_container_add(GTK_CONTAINER(frame), gui_state.wheel_status);
                gtk_table_attach(GTK_TABLE(table), frame, 1, 2, 6, 7, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
                button = gtk_check_button_new_with_label(_("Auto-Sequence"));
                gtk_signal_connect(GTK_OBJECT(button), "toggled", GTK_SIGNAL_FUNC(cbSequence), NULL);
                gtk_table_attach(GTK_TABLE(table), button, 2, 6, 6, 7, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
                gtk_notebook_append_page(GTK_NOTEBOOK(gui_state.notebook), table, gtk_label_new(_("Filter Wheel")));
                if (wheel_state.wheel.fd == 0)
                {
                    wheel_state.wheel.current = 0;
                    gtk_widget_set_sensitive(gtk_notebook_get_nth_page(GTK_NOTEBOOK(gui_state.notebook), 3), FALSE);
                }
            }
#define MISC_LABEL_WIDTH    275
#define MISC_SPIN_WIDTH     60
            /*
             * Build miscellaneous values pane.
             */
            {
                GtkWidget *vbox, *hbox, *spin, *label, *scrolled_win;

                ccd_state.reg_noise_sigs          = prefs.RegSig;
                ccd_state.reg_x_max_radius        = prefs.RegXRad;
                ccd_state.reg_y_max_radius        = prefs.RegYRad;
                ccd_state.reg_x_range             = prefs.RegXRange;
                ccd_state.reg_y_range             = prefs.RegYRange;
                ccd_state.guide_interleave_offset = prefs.TrackFieldOffset;
                ccd_state.guide_min_offset        = prefs.TrackMin;
                ccd_state.guide_msec              = prefs.TrackMsec;
                ccd_state.guide_train_msec        = prefs.TrackTrain;
                vbox  = gtk_vbox_new(FALSE, 0);
                label = gtk_label_new(_("Noise threshold in standard deviations:"));
                spin  = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(ccd_state.reg_noise_sigs, 0.0, 10.0, 1.0, 0.0, 0.0)), 1.0, 2);
                hbox = gtk_hbox_new(FALSE, 0);
                gtk_signal_connect(GTK_OBJECT(spin), "changed", GTK_SIGNAL_FUNC(cbMiscChanged), (gpointer)0);
                gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                gtk_object_set(GTK_OBJECT(label), "width", MISC_LABEL_WIDTH, NULL);
                gtk_object_set(GTK_OBJECT(spin), "width", MISC_SPIN_WIDTH, NULL);
                gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
                label = gtk_label_new(_("Maximum horizontal radius for point registration:"));
                spin  = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(ccd_state.reg_x_max_radius, 1.0, 1000.0, 1.0, 0.0, 0.0)), 1.0, 0);
                hbox  = gtk_hbox_new(FALSE, 0);
                gtk_signal_connect(GTK_OBJECT(spin), "changed", GTK_SIGNAL_FUNC(cbMiscChanged), (gpointer)1);
                gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                gtk_object_set(GTK_OBJECT(label), "width", MISC_LABEL_WIDTH, NULL);
                gtk_object_set(GTK_OBJECT(spin), "width", MISC_SPIN_WIDTH, NULL);
                gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
                label = gtk_label_new(_("Maximum vertical radius for point registration:"));
                spin  = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(ccd_state.reg_y_max_radius, 1.0, 1000.0, 1.0, 0.0, 0.0)), 1.0, 0);
                hbox  = gtk_hbox_new(FALSE, 0);
                gtk_signal_connect(GTK_OBJECT(spin), "changed", GTK_SIGNAL_FUNC(cbMiscChanged), (gpointer)2);
                gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                gtk_object_set(GTK_OBJECT(label), "width", MISC_LABEL_WIDTH, NULL);
                gtk_object_set(GTK_OBJECT(spin), "width", MISC_SPIN_WIDTH, NULL);
                gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
                label = gtk_label_new(_("Horizontal search range % for next point registration:"));
                spin  = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(ccd_state.reg_x_range, 1.0, 50.0, 1.0, 0.0, 0.0)), 1.0, 0);
                hbox  = gtk_hbox_new(FALSE, 0);
                gtk_signal_connect(GTK_OBJECT(spin), "changed", GTK_SIGNAL_FUNC(cbMiscChanged), (gpointer)3);
                gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                gtk_object_set(GTK_OBJECT(label), "width", MISC_LABEL_WIDTH, NULL);
                gtk_object_set(GTK_OBJECT(spin), "width", MISC_SPIN_WIDTH, NULL);
                gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
                label = gtk_label_new(_("Veritical search range % for next point registration:"));
                spin =  gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(ccd_state.reg_y_range, 1.0, 50.0, 1.0, 0.0, 0.0)), 1.0, 0);
                hbox =  gtk_hbox_new(FALSE, 0);
                gtk_signal_connect(GTK_OBJECT(spin), "changed", GTK_SIGNAL_FUNC(cbMiscChanged), (gpointer)4);
                gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                gtk_object_set(GTK_OBJECT(label), "width", MISC_LABEL_WIDTH, NULL);
                gtk_object_set(GTK_OBJECT(spin), "width", MISC_SPIN_WIDTH, NULL);
                gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
                label = gtk_label_new(_("Vertical interleave guide adjustment:"));
                spin  = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(ccd_state.guide_interleave_offset, -2.0, 2.0, 0.25, 0.0, 0.0)), 1.0, 2);
                hbox  = gtk_hbox_new(FALSE, 0);
                gtk_signal_connect(GTK_OBJECT(spin), "changed", GTK_SIGNAL_FUNC(cbMiscChanged), (gpointer)5);
                gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                gtk_object_set(GTK_OBJECT(label), "width", MISC_LABEL_WIDTH, NULL);
                gtk_object_set(GTK_OBJECT(spin), "width", MISC_SPIN_WIDTH, NULL);
                gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
                label = gtk_label_new(_("Ignore guide offsets less than:"));
                spin  = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(ccd_state.guide_min_offset, 0.0, 2.0, 0.05, 0.0, 0.0)), 1.0, 2);
                hbox  = gtk_hbox_new(FALSE, 0);
                gtk_signal_connect(GTK_OBJECT(spin), "changed", GTK_SIGNAL_FUNC(cbMiscChanged), (gpointer)6);
                gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                gtk_object_set(GTK_OBJECT(label), "width", MISC_LABEL_WIDTH, NULL);
                gtk_object_set(GTK_OBJECT(spin), "width", MISC_SPIN_WIDTH, NULL);
                gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
                label = gtk_label_new(_("Guide exposure duration (msec):"));
                spin  = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(ccd_state.guide_msec, 10.0, 10000.0, 1.0, 0.0, 0.0)), 1.0, 0);
                hbox  = gtk_hbox_new(FALSE, 0);
                gtk_signal_connect(GTK_OBJECT(spin), "changed", GTK_SIGNAL_FUNC(cbMiscChanged), (gpointer)7);
                gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                gtk_object_set(GTK_OBJECT(label), "width", MISC_LABEL_WIDTH, NULL);
                gtk_object_set(GTK_OBJECT(spin), "width", MISC_SPIN_WIDTH, NULL);
                gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
                label = gtk_label_new(_("Guide training duration (msec):"));
                spin  = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(ccd_state.guide_train_msec, 100.0, 10000.0, 1.0, 0.0, 0.0)), 1.0, 0);
                hbox  = gtk_hbox_new(FALSE, 0);
                gtk_signal_connect(GTK_OBJECT(spin), "changed", GTK_SIGNAL_FUNC(cbMiscChanged), (gpointer)8);
                gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                gtk_object_set(GTK_OBJECT(label), "width", MISC_LABEL_WIDTH, NULL);
                gtk_object_set(GTK_OBJECT(spin), "width", MISC_SPIN_WIDTH, NULL);
                gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
                label = gtk_label_new(_("STAR2000 init delay (msec):"));
                spin  = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(scope_state.init_delay, 0.0, 10000.0, 1.0, 0.0, 0.0)), 1.0, 0);
                hbox  = gtk_hbox_new(FALSE, 0);
                gtk_signal_connect(GTK_OBJECT(spin), "changed", GTK_SIGNAL_FUNC(cbMiscChanged), (gpointer)9);
                gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                gtk_object_set(GTK_OBJECT(label), "width", MISC_LABEL_WIDTH, NULL);
                gtk_object_set(GTK_OBJECT(spin), "width", MISC_SPIN_WIDTH, NULL);
                gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
                scrolled_win = gtk_scrolled_window_new(NULL, NULL);
                gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
                gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_win), vbox);
                gtk_notebook_append_page(GTK_NOTEBOOK(gui_state.notebook), scrolled_win, gtk_label_new(_("Misc. Values")));
            }
        }
    }
    gtk_widget_show_all(gui_state.window);
}
