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
/*
 * Image scale sizes.
 */
#define SCALE_ASPECT    0
#define SCALE_HALFX     1
#define SCALE_2X        2
#define SCALE_4X        3

/***************************************************************************\
*                                                                           *
*                             Prototypes                                    *
*                                                                           *
\***************************************************************************/

static void cbAcquire(GtkObject *object, gpointer data);
static void cbOpen(GtkObject *object, gpointer data);
static void cbProp(GtkObject *object, gpointer data);
static void cbSaveAs(GtkObject *object, gpointer data);
static void cbSave(GtkObject *object, gpointer data);
static void cbSaveAll(GtkObject *object, gpointer data);
static void cbClose(GtkObject *object, gpointer data);
static void cbCloseAll(GtkObject *object, gpointer data);
static void cbExit(GtkObject *object, gpointer data);
static void cbDirPrefs(GtkObject *object, gpointer data);
static void cbClosePrefsToggle(GtkObject *object, gpointer data);
static void cbPortPrefs(GtkObject *object, gpointer data);
static void cbToolbarToggle(GtkObject *object, gpointer data);
static void cbViewHistogram(GtkObject *object, gpointer data);
static void cbViewContrastToggle(GtkObject *object, gpointer data);
static void cbViewColorToggle(GtkObject *object, gpointer data);
static void cbColorPalette(GtkObject *object, gpointer data);
static void cbViewAspectToggle(GtkObject *object, gpointer data);
static void cbViewBinToggle(GtkObject *object, gpointer data);
static void cbViewMode(GtkObject *object, gpointer data);
static void cbImageFlip(GtkObject *object, gpointer data);
static void cbImageRotate(GtkObject *object, gpointer data);
static void cbImageScale(GtkObject *object, gpointer data);
static void cbImageColorSplit(GtkObject *object, gpointer data);
static void cbImageRemoveNoise(GtkObject *object, gpointer data);
static void cbImageRemoveBackground(GtkObject *object, gpointer data);
static void cbImageRemoveVBE(GtkObject *object, gpointer data);
static void cbAbout(GtkObject *object, gpointer data);

/***************************************************************************\
*                                                                           *
*                            Global Data                                    *
*                                                                           *
\***************************************************************************/

/*
 * MDI object.
 */
GtkObject *mdi = NULL;
/*
 * Cursor images.
 */
GdkCursor *cursorWait = NULL;
/*
 * Verbosity.
 */
unsigned int verbose = 0;
/*
 * Preferences.
 */
struct _prefs prefs = {{0},0};
/*
 * Color names. In bit mask order BLUE=0x01, GREEN = 0x02, RED=0x04
 */
gchar *ColorName[8] =
{
    N_("Dark"),
    N_("Blue"),
    N_("Green"),
    N_("Cyan"),
    N_("Red"),
    N_("Magenta"),
    N_("Yellow"),
    N_("Luminance")
};
/*
 * Palettes.
 */
unsigned long view_palettes[NUM_PALETTES][256];
/*
 * Camera pixmap.
 */
/* XPM */
static char * camera_xpm[] = {
"22 22 8 1",
" 	c None",
".	c #000000",
"+	c #9B9B9B",
"@	c #FF0000",
"#	c #98F3FF",
"$	c #4FEAFF",
"%	c #00E1FF",
"&	c #494533",
"                      ",
"                      ",
"                      ",
"                      ",
"                      ",
"                      ",
"     ..............   ",
"    .++++++++++++..   ",
"   ..............+.   ",
"   .++++++++++++.+.   ",
"   .+@++.....+++.+.   ",
"   .++ ....++.++.+.   ",
"   .++..#$..+.++.+.   ",
"   .++.#$$%.+.++..    ",
"   .++.$$%%..+++..    ",
"   .....%%.......     ",
"     .&....  .&.      ",
"     .&.     .&.      ",
"    .&.       .&.     ",
"    .&.       .&.     ",
"   .&.         .&.    ",
"   .&.         .&.    "};
/*
 * Menus.
 */
GnomeUIInfo menuFile[] =
{
    {   GNOME_APP_UI_ITEM, N_("_Acquire..."), N_("Acquire new CCD image"),
        (gpointer)cbAcquire, NULL, NULL,
        GNOME_APP_PIXMAP_DATA, camera_xpm,
        0, (GdkModifierType) 0, NULL},
    GNOMEUIINFO_MENU_OPEN_ITEM(                                               cbOpen,  NULL),
    {   GNOME_APP_UI_ITEM, N_("Properties"), N_("Image Properties"),
        (gpointer)cbProp, NULL, NULL,
        GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PROP,
        0, (GdkModifierType) 0, NULL},
    GNOMEUIINFO_MENU_SAVE_ITEM(                                               cbSave,  NULL),
    GNOMEUIINFO_MENU_SAVE_AS_ITEM(                                            cbSaveAs,NULL),
    {   GNOME_APP_UI_ITEM, N_("Save All"), N_("Save All Images"),
        (gpointer)cbSaveAll, NULL, NULL,
        GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SAVE,
        0, (GdkModifierType) 0, NULL},
    GNOMEUIINFO_MENU_CLOSE_ITEM(                                              cbClose, NULL),
    {   GNOME_APP_UI_ITEM, N_("Close All"), N_("Close All Images"),
        (gpointer)cbCloseAll, NULL, NULL,
        GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CLOSE,
        0, (GdkModifierType) 0, NULL},
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_MENU_EXIT_ITEM(                                               cbExit,  NULL),
    GNOMEUIINFO_END
};
GnomeUIInfo menuSettings[] =
{
    {   GNOME_APP_UI_ITEM, N_("Working Directory..."), N_("Set Working Directory"),
        (gpointer)cbDirPrefs, NULL, NULL,
        GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PREF,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_TOGGLEITEM, N_("Close Warning"), N_("Warn on unsaved image close"),
        (gpointer)cbClosePrefsToggle, NULL, NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_ITEM, N_("COM Ports..."), N_("Set COM ports"),
        (gpointer)cbPortPrefs, NULL, NULL,
        GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PREF,
        0, (GdkModifierType) 0, NULL},
    GNOMEUIINFO_END
};
GnomeUIInfo submenuPalettes[] =
{
    {   GNOME_APP_UI_TOGGLEITEM, N_("Linear Ramp"), N_("View image using linear grey scale"),
        (gpointer)cbColorPalette, GUINT_TO_POINTER(LINEAR_GREY), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_TOGGLEITEM, N_("GammaLog 1 Ramp"), N_("View image using Gamma Log 1.0 / 1.0 grey scale"),
        (gpointer)cbColorPalette, GUINT_TO_POINTER(GAMMALOG1_GREY), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_TOGGLEITEM, N_("GammaLog 2 Ramp"), N_("View image using Gamma Log 1.0 / 2.0 grey scale"),
        (gpointer)cbColorPalette, GUINT_TO_POINTER(GAMMALOG2_GREY), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_TOGGLEITEM, N_("GammaLog 3 Ramp"), N_("View image using Gamma Log 1.0 / 3.0 grey scale"),
        (gpointer)cbColorPalette, GUINT_TO_POINTER(GAMMALOG3_GREY), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_TOGGLEITEM, N_("GammaLog 4 Ramp"), N_("View image using Gamma Log 1.0 / 4.0 grey scale"),
        (gpointer)cbColorPalette, GUINT_TO_POINTER(GAMMALOG4_GREY), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_TOGGLEITEM, N_("Linear Red Ramp"), N_("View image using linear red scale"),
        (gpointer)cbColorPalette, GUINT_TO_POINTER(LINEAR_RED), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_TOGGLEITEM, N_("GammaLog 1 Red Ramp"), N_("View image using Gamma Log 1.0 / 1.0 red scale"),
        (gpointer)cbColorPalette, GUINT_TO_POINTER(GAMMALOG1_RED), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_TOGGLEITEM, N_("GammaLog 2 Red Ramp"), N_("View image using Gamma Log 1.0 / 2.0 red scale"),
        (gpointer)cbColorPalette, GUINT_TO_POINTER(GAMMALOG2_RED), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_TOGGLEITEM, N_("GammaLog 3 Red Ramp"), N_("View image using Gamma Log 1.0 / 3.0 red scale"),
        (gpointer)cbColorPalette, GUINT_TO_POINTER(GAMMALOG3_RED), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_TOGGLEITEM, N_("GammaLog 4 Red Ramp"), N_("View image using Gamma Log 1.0 / 4.0 red scale"),
        (gpointer)cbColorPalette, GUINT_TO_POINTER(GAMMALOG4_RED), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_TOGGLEITEM, N_("Invert"), N_("View image using inversion palette"),
        (gpointer)cbColorPalette, GUINT_TO_POINTER(INVERSION), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_TOGGLEITEM, N_("False Color 1"), N_("View image using false color palette #1"),
        (gpointer)cbColorPalette, GUINT_TO_POINTER(FALSE_COLOR1), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_TOGGLEITEM, N_("False Color 2"), N_("View image using false color palette #2"),
        (gpointer)cbColorPalette, GUINT_TO_POINTER(FALSE_COLOR2), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_TOGGLEITEM, N_("False Color 3"), N_("View image using false color palette #3"),
        (gpointer)cbColorPalette, GUINT_TO_POINTER(FALSE_COLOR3), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_TOGGLEITEM, N_("MinMax Highlite"), N_("View image using minimum, middle, & maximin highlite palette"),
        (gpointer)cbColorPalette, GUINT_TO_POINTER(MINMAX_HIGHLITE), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_TOGGLEITEM, N_("4 Level"), N_("View image using 4 level decimation palette"),
        (gpointer)cbColorPalette, GUINT_TO_POINTER(LEVEL_4), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    GNOMEUIINFO_END
};
GnomeUIInfo menuView[] =
{
    GNOMEUIINFO_SEPARATOR,
    {   GNOME_APP_UI_ITEM, N_("Show Histogram"), N_("Open histogram window"),
        (gpointer)cbViewHistogram, NULL, NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    GNOMEUIINFO_SEPARATOR,
    {   GNOME_APP_UI_TOGGLEITEM, N_("Contrast Stretch"), N_("Contrast stretch image"),
        (gpointer)cbViewContrastToggle, NULL, NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_TOGGLEITEM, N_("Aspect Corrected"), N_("Correct for aspect ratio"),
        (gpointer)cbViewAspectToggle, NULL, NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_TOGGLEITEM, N_("Bin Corrected"), N_("Correct for binning"),
        (gpointer)cbViewBinToggle, NULL, NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_TOGGLEITEM, N_("Filter Color"), N_("Show image using filter color information"),
        (gpointer)cbViewColorToggle, NULL, NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    GNOMEUIINFO_SUBTREE("_Palettes", submenuPalettes),
    GNOMEUIINFO_SEPARATOR,
    {   GNOME_APP_UI_TOGGLEITEM, N_("Toolbar"), N_("Toolbar display toggle"),
        (gpointer)cbToolbarToggle, NULL, NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_ITEM, N_("Notebook"), N_("Notebook display mode"),
        (gpointer)cbViewMode, GUINT_TO_POINTER(GNOME_MDI_NOTEBOOK), NULL,
        GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_INDEX,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_ITEM, N_("Toplevel"), N_("Toplevel display mode"),
        (gpointer)cbViewMode, GUINT_TO_POINTER(GNOME_MDI_TOPLEVEL), NULL,
        GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_TOP,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_ITEM, N_("Modal"), N_("Modal display mode"),
        (gpointer)cbViewMode, GUINT_TO_POINTER(GNOME_MDI_MODAL), NULL,
        GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_BOTTOM,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_ITEM, N_("Default"), N_("Default display mode"),
        (gpointer)cbViewMode, GUINT_TO_POINTER(GNOME_MDI_DEFAULT_MODE), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    GNOMEUIINFO_END
};
GnomeUIInfo menuWindow[] =
{
    GNOMEUIINFO_END
};
GnomeUIInfo menuImage[] =
{
    {   GNOME_APP_UI_ITEM, N_("Flip Horizontal"), N_("Flip image horizontally"),
        (gpointer)cbImageFlip, GUINT_TO_POINTER(1), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_ITEM, N_("Flip Vertical"), N_("Flip image vertically"),
        (gpointer)cbImageFlip, GUINT_TO_POINTER(0), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_ITEM, N_("Rotate 90 degrees"), N_("Rotate image 90 degrees"),
        (gpointer)cbImageRotate, GUINT_TO_POINTER(90), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_ITEM, N_("Rotate 180 degrees"), N_("Rotate image 180 degrees"),
        (gpointer)cbImageRotate, GUINT_TO_POINTER(180), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_ITEM, N_("Rotate 270 degrees"), N_("Rotate image 270 degrees"),
        (gpointer)cbImageRotate, GUINT_TO_POINTER(270), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_ITEM, N_("Scale 1:1 Aspect"), N_("Scale to 1:1 aspect ratio"),
        (gpointer)cbImageScale, GUINT_TO_POINTER(SCALE_ASPECT), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_ITEM, N_("Scale 1/2 X"), N_("Scale to half size"),
        (gpointer)cbImageScale, GUINT_TO_POINTER(SCALE_HALFX), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_ITEM, N_("Scale 2 X"), N_("Scale to twice size"),
        (gpointer)cbImageScale, GUINT_TO_POINTER(SCALE_2X), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_ITEM, N_("Scale 4 X"), N_("Scale to four times size"),
        (gpointer)cbImageScale, GUINT_TO_POINTER(SCALE_4X), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    GNOMEUIINFO_SEPARATOR,
    {   GNOME_APP_UI_ITEM, N_("Native Matrix Split"), N_("Split apart image based on camera color matrix"),
        (gpointer)cbImageColorSplit, GUINT_TO_POINTER(0), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_ITEM, N_("L/RGB Matrix Split"), N_("Split apart image based on camera color matrix"),
        (gpointer)cbImageColorSplit, GUINT_TO_POINTER(1), NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    GNOMEUIINFO_SEPARATOR,
    {   GNOME_APP_UI_ITEM, N_("Remove Noise"), N_("Remove noise from image"),
        (gpointer)cbImageRemoveNoise, NULL, NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_ITEM, N_("Remove Background"), N_("Remove background offset from image"),
        (gpointer)cbImageRemoveBackground, NULL, NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_ITEM, N_("Remove VBE"), N_("Venetian Blind Effect removal filter"),
        (gpointer)cbImageRemoveVBE, NULL, NULL,
        GNOME_APP_PIXMAP_NONE, NULL,
        0, (GdkModifierType) 0, NULL},
    GNOMEUIINFO_END
};
GnomeUIInfo menuHelp[] =
{
    GNOMEUIINFO_MENU_ABOUT_ITEM(cbAbout, NULL),
    GNOMEUIINFO_HELP(PACKAGE),
    GNOMEUIINFO_END
};
GnomeUIInfo menuMain[] =
{
    GNOMEUIINFO_MENU_FILE_TREE(menuFile),
    GNOMEUIINFO_MENU_SETTINGS_TREE(menuSettings),
    GNOMEUIINFO_MENU_VIEW_TREE(menuView),
    { GNOME_APP_UI_SUBTREE_STOCK, N_("_Image"), NULL, menuImage, NULL, NULL,
      (GnomeUIPixmapType) 0, NULL, 0, (GdkModifierType) 0, NULL },
    GNOMEUIINFO_MENU_WINDOWS_TREE(menuWindow),
    GNOMEUIINFO_MENU_HELP_TREE(menuHelp),
    GNOMEUIINFO_END
};
/*
 * Toolbar.
 */
GnomeUIInfo toolbarMain[] =
{
    {   GNOME_APP_UI_ITEM, N_("Acquire"), N_("Acquire new CCD image"),
        (gpointer)cbAcquire, NULL, NULL,
        GNOME_APP_PIXMAP_DATA, camera_xpm,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_ITEM, N_("Open"), N_("Open CCD image"),
        (gpointer)cbOpen, NULL, NULL,
        GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_OPEN,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_ITEM, N_("Properties"), N_("CCD image properties"),
        (gpointer)cbProp, NULL, NULL,
        GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_PROPERTIES,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_ITEM, N_("Save"), N_("Save CCD image"),
        (gpointer)cbSave, (gpointer) 1, NULL,
        GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_SAVE,
        0, (GdkModifierType) 0, NULL},
    {   GNOME_APP_UI_ITEM, N_("Close"), N_("Close CCD image"),
        (gpointer)cbClose, NULL, NULL,
        GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_CLOSE,
        0, (GdkModifierType) 0, NULL},
    GNOMEUIINFO_SEPARATOR,
    {   GNOME_APP_UI_ITEM, N_("Exit"), N_("Exit program"),
        (gpointer)cbExit, NULL, NULL,
        GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_EXIT,
        0, (GdkModifierType) 0, NULL},
    GNOMEUIINFO_END
};
/*
 * Create palettes used for view options.
 */
static void create_view_palette(void)
{
    int   i;
    float f, r, g, b;

#define RGB2LONG(r,g,b) (((unsigned long)(r)&0xFF)|(((unsigned long)(g)&0xFF)<<8)|(((unsigned long)(b)&0xFF)<<16))
    /*
     * Linear palettes.
     */
    for (i = 0; i < 256; i++)
    {
        view_palettes[LINEAR_GREY][i] = RGB2LONG(i, i, i);
        view_palettes[LINEAR_RED][i]  = RGB2LONG(i, 0, 0);
    }
    /*
     * GammaLog palettes.
     */
    for (i = 0; i < 256; i++)
    {
        f = log10(pow((i/255.0), 1.0)*9.0 + 1.0) * 255.0;
        view_palettes[GAMMALOG1_GREY][i] = RGB2LONG(f, f, f);
        view_palettes[GAMMALOG1_RED][i]  = RGB2LONG(f, 0, 0);
        f = log10(pow((i/255.0), 0.5)*9.0 + 1.0) * 255.0;
        view_palettes[GAMMALOG2_GREY][i] = RGB2LONG(f, f, f);
        view_palettes[GAMMALOG2_RED][i]  = RGB2LONG(f, 0, 0);
        f = log10(pow((i/255.0), 0.33333333)*9.0 + 1.0) * 255.0;
        view_palettes[GAMMALOG3_GREY][i] = RGB2LONG(f, f, f);
        view_palettes[GAMMALOG3_RED][i]  = RGB2LONG(f, 0, 0);
        f = log10(pow((i/255.0), 0.25)*9.0 + 1.0) * 255.0;
        view_palettes[GAMMALOG4_GREY][i] = RGB2LONG(f, f, f);
        view_palettes[GAMMALOG4_RED][i]  = RGB2LONG(f, 0, 0);
    }
    /*
     * Inversion palette.
     */
    for (i = 0; i < 256; i++)
    {
        view_palettes[INVERSION][i] = RGB2LONG(255 - i, 255 - i, 255 - i);
    }
#define DEG2RAD 0.01745329
    for (i = 0; i < 256; i++)
    {
        /*
         * False color palette #1.
         */
        r = (sin((i/255.0 * 360.0 + 0.0)   * DEG2RAD) * 0.5 + 0.5) * 255.0;
        g = (sin((i/255.0 * 360.0 + 120.0) * DEG2RAD) * 0.5 + 0.5) * 255.0;
        b = (sin((i/255.0 * 360.0 + 240.0) * DEG2RAD) * 0.5 + 0.5) * 255.0;
        view_palettes[FALSE_COLOR1][i] = RGB2LONG(r, g, b);
        /*
         * False color palette #2.
         */
        r = (sin((i/255.0 * 360.0 + 120.0) * DEG2RAD) * 0.5 + 0.5) * 255.0;
        g = (sin((i/255.0 * 360.0 + 240.0) * DEG2RAD) * 0.5 + 0.5) * 255.0;
        b = (sin((i/255.0 * 360.0 + 0.0)   * DEG2RAD) * 0.5 + 0.5) * 255.0;
        view_palettes[FALSE_COLOR2][i] = RGB2LONG(r, g, b);
        /*
         * False color palette #3.
         */
        r = (sin((i/255.0 * 360.0 + 240.0) * DEG2RAD) * 0.5 + 0.5) * 255.0;
        g = (sin((i/255.0 * 360.0 + 0.0)   * DEG2RAD) * 0.5 + 0.5) * 255.0;
        b = (sin((i/255.0 * 360.0 + 120.0) * DEG2RAD) * 0.5 + 0.5) * 255.0;
        view_palettes[FALSE_COLOR3][i] = RGB2LONG(r, g, b);
    }
#undef DEG2RAD
    /*
     * Min, middle, max highlite palette.
     */
    for (i = 1; i < 255; i++)
    {
        view_palettes[MINMAX_HIGHLITE][i] = RGB2LONG(0, 0, 0);
    }
    view_palettes[MINMAX_HIGHLITE][0]   = RGB2LONG(255,   0,   0);
    view_palettes[MINMAX_HIGHLITE][127] = RGB2LONG(  0, 255,   0);
    view_palettes[MINMAX_HIGHLITE][128] = RGB2LONG(  0, 255,   0);
    view_palettes[MINMAX_HIGHLITE][255] = RGB2LONG(255, 255, 255);
    /*
     * Decimate to 4 levels.
     */
    for (i = 0; i < 256; i++)
    {
        view_palettes[LEVEL_4][i]  = RGB2LONG(i | 0x3F, i | 0x3F, i | 0x3F);
    }
#undef RGB2LONG
}

/***************************************************************************\
*                                                                           *
*                                Preferences                                *
*                                                                           *
\***************************************************************************/

static void prefs_load(void)
{
    strcpy(prefs.WorkingDirectory, gnome_config_get_string("/gccd/images/dir=."));
    prefs.CloseWarning     = gnome_config_get_bool("/gccd/file/close_warning=true");
    prefs.ViewContrast     = gnome_config_get_bool("/gccd/view/contrast=true");
    prefs.ViewAspect       = gnome_config_get_bool("/gccd/view/aspect=false");
    prefs.ViewBin          = gnome_config_get_bool("/gccd/view/bin=false");
    prefs.ViewToolbar      = gnome_config_get_bool("/gccd/view/toolbar=true");
    prefs.ViewColor        = gnome_config_get_bool("/gccd/view/color=false");
    prefs.ViewPalette      = gnome_config_get_int("/gccd/view/palette=0");
    prefs.ViewMode         = (GnomeMDIMode)gnome_config_get_int("/gccd/view/mode=42");
    prefs.PortScope        = gnome_config_get_int("/gccd/port/scope=-1");
    prefs.PortWheel        = gnome_config_get_int("/gccd/port/wheel=-1");
    prefs.ScopeIFace       = gnome_config_get_int("/gccd/scope/iface=-1");
    prefs.ScopeRA          = gnome_config_get_int("/gccd/scope/ra=0");
    prefs.ScopeDec         = gnome_config_get_int("/gccd/scope/dec=0");
    prefs.ScopeSwap        = gnome_config_get_int("/gccd/scope/swap=0");
    prefs.ScopeInitDelay   = gnome_config_get_int("/gccd/scope/init_delay=0");
    prefs.RegSig           = gnome_config_get_float("/gccd/reg/sigs=2.0");
    prefs.RegXRad          = gnome_config_get_int("/gccd/reg/x_radius=5");
    prefs.RegYRad          = gnome_config_get_int("/gccd/reg/y_radius=5");
    prefs.RegXRange        = gnome_config_get_int("/gccd/reg/x_range=10");
    prefs.RegYRange        = gnome_config_get_int("/gccd/reg/y_range=10");
    prefs.TrackTrain       = gnome_config_get_int("/gccd/track/train=5000");
    prefs.TrackFieldOffset = gnome_config_get_float("/gccd/track/field_offset=0.25");
    prefs.TrackMin         = gnome_config_get_float("/gccd/track/min_offset=0.5");
    prefs.TrackMsec        = gnome_config_get_int("/gccd/track/msec=500");
    prefs.TrackSelf        = gnome_config_get_int("/gccd/track/self=0");
    prefs.TrackUp          = gnome_config_get_float("/gccd/track/up=1.0");
    prefs.TrackDown        = gnome_config_get_float("/gccd/track/down=1.0");
    prefs.TrackLeft        = gnome_config_get_float("/gccd/track/left=1.0");
    prefs.TrackRight       = gnome_config_get_float("/gccd/track/right=1.0");
    prefs.Filter[0]        = gnome_config_get_int("/gccd/filter/type0=1");
    prefs.Filter[1]        = gnome_config_get_int("/gccd/filter/type1=0");
    prefs.Filter[2]        = gnome_config_get_int("/gccd/filter/type2=0");
    prefs.Filter[3]        = gnome_config_get_int("/gccd/filter/type3=0");
    prefs.Filter[4]        = gnome_config_get_int("/gccd/filter/type4=0");
    prefs.Filter[5]        = gnome_config_get_int("/gccd/filter/type5=0");
    prefs.Filter[6]        = gnome_config_get_int("/gccd/filter/type6=0");
    prefs.FilterExp[0]     = gnome_config_get_int("/gccd/filter/exposure0=100");
    prefs.FilterExp[1]     = gnome_config_get_int("/gccd/filter/exposure1=100");
    prefs.FilterExp[2]     = gnome_config_get_int("/gccd/filter/exposure2=100");
    prefs.FilterExp[3]     = gnome_config_get_int("/gccd/filter/exposure3=100");
    prefs.FilterExp[4]     = gnome_config_get_int("/gccd/filter/exposure4=100");
    prefs.FilterExp[5]     = gnome_config_get_int("/gccd/filter/exposure5=100");
    prefs.FilterExp[6]     = gnome_config_get_int("/gccd/filter/exposure6=100");
}
static void prefs_save(void)
{
    gnome_config_set_string("/gccd/images/dir",        prefs.WorkingDirectory);
    gnome_config_set_bool("/gccd/file/close_warning",  prefs.CloseWarning);
    gnome_config_set_bool("/gccd/view/contrast",       prefs.ViewContrast);
    gnome_config_set_bool("/gccd/view/aspect",         prefs.ViewAspect);
    gnome_config_set_bool("/gccd/view/bin",            prefs.ViewBin);
    gnome_config_set_bool("/gccd/view/color",          prefs.ViewColor);
    gnome_config_set_int("/gccd/view/palette",         prefs.ViewPalette);
    gnome_config_set_bool("/gccd/view/toolbar",        prefs.ViewToolbar);
    gnome_config_set_int("/gccd/view/mode",            prefs.ViewMode);
    gnome_config_set_int("/gccd/port/scope",           prefs.PortScope);
    gnome_config_set_int("/gccd/port/wheel",           prefs.PortWheel);
    gnome_config_set_int("/gccd/scope/iface",          prefs.ScopeIFace);
    gnome_config_set_int("/gccd/scope/ra",             prefs.ScopeRA);
    gnome_config_set_int("/gccd/scope/dec",            prefs.ScopeDec);
    gnome_config_set_float("/gccd/reg/sigs",           prefs.RegSig);
    gnome_config_set_int("/gccd/reg/x_radius",         prefs.RegXRad);
    gnome_config_set_int("/gccd/reg/y_radius",         prefs.RegYRad);
    gnome_config_set_int("/gccd/reg/x_range",          prefs.RegXRange);
    gnome_config_set_int("/gccd/reg/y_range",          prefs.RegYRange);
    gnome_config_set_int("/gccd/track/train",          prefs.TrackTrain);
    gnome_config_set_float("/gccd/track/field_offset", prefs.TrackFieldOffset);
    gnome_config_set_float("/gccd/track/min_offset",   prefs.TrackMin);
    gnome_config_set_int("/gccd/track/msec",           prefs.TrackMsec);
    gnome_config_set_int("/gccd/track/self",           prefs.TrackSelf);
    gnome_config_set_float("/gccd/track/up",           prefs.TrackUp);
    gnome_config_set_float("/gccd/track/down",         prefs.TrackDown);
    gnome_config_set_float("/gccd/track/left",         prefs.TrackLeft);
    gnome_config_set_float("/gccd/track/right",        prefs.TrackRight);
    gnome_config_set_int("/gccd/filter/type0",         prefs.Filter[0]);
    gnome_config_set_int("/gccd/filter/type1",         prefs.Filter[1]);
    gnome_config_set_int("/gccd/filter/type2",         prefs.Filter[2]);
    gnome_config_set_int("/gccd/filter/type3",         prefs.Filter[3]);
    gnome_config_set_int("/gccd/filter/type4",         prefs.Filter[4]);
    gnome_config_set_int("/gccd/filter/type5",         prefs.Filter[5]);
    gnome_config_set_int("/gccd/filter/type6",         prefs.Filter[6]);
    gnome_config_set_int("/gccd/filter/exposure0",     prefs.FilterExp[0]);
    gnome_config_set_int("/gccd/filter/exposure1",     prefs.FilterExp[1]);
    gnome_config_set_int("/gccd/filter/exposure2",     prefs.FilterExp[2]);
    gnome_config_set_int("/gccd/filter/exposure3",     prefs.FilterExp[3]);
    gnome_config_set_int("/gccd/filter/exposure4",     prefs.FilterExp[4]);
    gnome_config_set_int("/gccd/filter/exposure5",     prefs.FilterExp[5]);
    gnome_config_set_int("/gccd/filter/exposure6",     prefs.FilterExp[6]);
    gnome_config_sync();
}
static void prefs_set_menu_items(GnomeMDIChild *child)
{
    int               i;
    GnomeApp         *app;
    GnomeUIInfo      *menu, *view, *pal;
    struct ccd_image *image;

    if (child)
    {
        app      = gnome_mdi_get_active_window(GNOME_MDI(mdi));
        menu     = gnome_mdi_get_menubar_info(app);
        view     = (GnomeUIInfo *)menu[2].moreinfo;
        pal      = (GnomeUIInfo *)view[7].moreinfo;
        if ((image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(child))))
        {
            GTK_CHECK_MENU_ITEM(view[3].widget)->active = image->view.ContrastStretch;
            GTK_CHECK_MENU_ITEM(view[4].widget)->active = image->view.AspectStretch;
            GTK_CHECK_MENU_ITEM(view[5].widget)->active = image->view.BinStretch;
            GTK_CHECK_MENU_ITEM(view[6].widget)->active = image->view.Color;
            for (i = 0; i < NUM_PALETTES; i++)
                GTK_CHECK_MENU_ITEM(pal[i].widget)->active = (image->view.Palette == i);
            GTK_CHECK_MENU_ITEM(view[9].widget)->active = image->view.Toolbar;
        }
        else
        {
            GTK_CHECK_MENU_ITEM(view[3].widget)->active = prefs.ViewContrast;
            GTK_CHECK_MENU_ITEM(view[4].widget)->active = prefs.ViewAspect;
            GTK_CHECK_MENU_ITEM(view[5].widget)->active = prefs.ViewBin;
            GTK_CHECK_MENU_ITEM(view[6].widget)->active = prefs.ViewColor;
            for (i = 0; i < NUM_PALETTES; i++)
                GTK_CHECK_MENU_ITEM(pal[i].widget)->active = (prefs.ViewPalette == i);
            GTK_CHECK_MENU_ITEM(view[9].widget)->active = prefs.ViewToolbar;
        }
    }
}
static void cbPrefsPortScope(GtkWidget *widget, gpointer data)
{
    if (GTK_TOGGLE_BUTTON(widget)->active)
        prefs.PortScope = (gint)data;
}
static void cbPrefsPortWheel(GtkWidget *widget, gpointer data)
{
    if (GTK_TOGGLE_BUTTON(widget)->active)
        prefs.PortWheel = (gint)data;
}
static gint eventPrefsDie(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    return (0);
}

/***************************************************************************\
*                                                                           *
*                         File selection dialog                             *
*                                                                           *
\***************************************************************************/

static void cbGetFilenameDestroy(GtkWidget *widget, gpointer data)
{
    gtk_grab_remove(widget);
}
static void dlgGetFilename(gchar *title, gchar *filename, void (*ok_callback)(GtkWidget *, gpointer))
{
    GtkWidget *select_file;

    select_file = gtk_file_selection_new(title);
    gtk_signal_connect(GTK_OBJECT(select_file), "destroy", cbGetFilenameDestroy, NULL);
    gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(select_file)->ok_button), "clicked", ok_callback, (gpointer)select_file);
    gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(select_file)->cancel_button), "clicked", gtk_widget_destroy, (gpointer)select_file);
    if (filename)
        gtk_file_selection_set_filename(GTK_FILE_SELECTION(select_file), filename);
    gtk_widget_show(select_file);
    gtk_grab_add(select_file);
}

/***************************************************************************\
*                                                                           *
*                         View Histogram Window                             *
*                                                                           *
\***************************************************************************/

static gint eventViewHistogramDie(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    struct ccd_image *image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(widget));
    if (image && image->histogram_view)
        image->histogram_view = NULL;
    return (0);
}
static void cbViewHistogramExpose(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    int   i, max, top;
    float scale;
    struct ccd_image *image = (struct ccd_image *)data;

    gdk_window_set_back_pixmap(widget->window, NULL, FALSE);
    for (i = max = 0; i < HISTOGRAM_BINS; i++)
        if (max < image->histogram[i])
            max = image->histogram[i];
    scale = 255.0 / max;
    for (i = 0; i < HISTOGRAM_BINS; i++)
    {
        top = view_palettes[GAMMALOG1_GREY][(int)(image->histogram[i] * scale)] & 0xFF;
        gdk_draw_rectangle(widget->window, widget->style->fg_gc[GTK_STATE_NORMAL], TRUE, i, 256 - top, 1, top);
        gdk_draw_rectangle(widget->window, widget->style->bg_gc[GTK_STATE_NORMAL], TRUE, i, 0, 1, 256 - top);
    }
}
static void histogramUpdate(struct ccd_image *image)
{
    if (image)
    {
        ccd_image_histogram(image);
        if (image->histogram_view)
        {
            char str[80];
            int  x, y;
            float pixel_sig = 0.0;

#define PIXEL_LOOP(pixel_type)                                                                                                                                      \
            for (y = 0; y < image->height; y++)                                                                                                                     \
                for (x = 0; x < image->width; x++)                                                                                                                  \
                    pixel_sig += (image->pixave - ((pixel_type *)(image->pixels))[y * image->width + x]) * (image->pixave - ((pixel_type *)(image->pixels))[y * image->width + x]);   \
            pixel_sig = sqrt(pixel_sig / ((image->height) * (image->width) - 1));                                                                                   \

            PIXEL_SIZE_CASE((image->depth + 7) / 8);
#undef PIXEL_LOOP

            sprintf(str, _("Min: %lu\n\nAve: %lu\n\nSigma: %.2f\n\nMax: %lu"), image->pixmin, image->pixave, pixel_sig, image->pixmax);
            gtk_label_set_text(GTK_LABEL(image->histogram_label), str);
            gtk_widget_queue_draw(image->histogram_view);
        }
    }
}

/***************************************************************************\
*                                                                           *
*                           Child callbacks                                 *
*                                                                           *
\***************************************************************************/

static void cbViewExpose(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    struct ccd_image *image = (struct ccd_image *)data;
    gdk_window_set_back_pixmap(widget->window, NULL, FALSE);
    gdk_draw_rectangle(widget->window, widget->style->bg_gc[GTK_STATE_NORMAL], TRUE,
                       event->area.x, event->area.y,
                       event->area.width, event->area.height);
    gdk_window_copy_area(widget->window, widget->style->fg_gc[GTK_STATE_NORMAL],
                         event->area.x, event->area.y,
                         image->pixmap,
                         event->area.x, event->area.y,
                         event->area.width, event->area.height);
#if 0
    /*
     * Paint any areas outside the image with default color.
     */
    if (event->area.x + event->area.width > image->width)
        gdk_draw_rectangle(widget->window, widget->style->bg_gc[GTK_STATE_NORMAL], TRUE,
                           image->width, event->area.y,
                           event->area.x + event->area.width - image->width, event->area.height);
    if (event->area.y + event->area.height > image->height)
        gdk_draw_rectangle(widget->window, widget->style->bg_gc[GTK_STATE_NORMAL], TRUE,
                           event->area.x, image->height,
                           event->area.width, event->area.y + event->area.height - image->height);
#endif
}
static GtkWidget *cbViewCreate(GnomeMDIChild *child, gpointer data)
{
    int               width, height;
    GtkWidget        *scrolled_win;
    struct ccd_image *image = (struct ccd_image *)data;

    /*
     * Create scrolled drawing area and connect signals.
     */
    if (image->view.AspectStretch)
    {
        if (image->pixel_height > image->pixel_width)
        {
            if (image->view.BinStretch && image->xbin && image->ybin)
            {
                width  = image->width  * image->xbin;
                height = image->height * image->ybin * ((float)image->pixel_height / (float)image->ybin) / ((float)image->pixel_width / (float)image->xbin);
            }
            else
            {
                width  = image->width;
                height = image->height * (float)image->pixel_height / (float)image->pixel_width;
            }
        }
        else
        {
            if (image->view.BinStretch && image->xbin && image->ybin)
            {
                width  = image->width  * image->xbin * ((float)image->pixel_width / (float)image->xbin) / ((float)image->pixel_height / (float)image->ybin);
                height = image->height * image->ybin;
            }
            else
            {
                width  = image->width * (float)image->pixel_width / (float)image->pixel_height;
                height = image->height;
            }
        }
    }
    else
    {
        width  = image->width  * ((image->view.BinStretch && image->xbin) ? image->xbin : 1);
        height = image->height * ((image->view.BinStretch && image->ybin) ? image->ybin : 1);
    }
    image->draw_area = gtk_drawing_area_new();
    gtk_drawing_area_size(GTK_DRAWING_AREA(image->draw_area), width, height);
    gtk_signal_connect(GTK_OBJECT(image->draw_area), "expose_event", GTK_SIGNAL_FUNC(cbViewExpose), data);
    scrolled_win = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_win), image->draw_area);
    gtk_widget_show_all(scrolled_win);
    return(scrolled_win);
}

/***************************************************************************\
*                                                                           *
*                             Image utilities                               *
*                                                                           *
\***************************************************************************/

static void imageChanged(GnomeMDIChild *child, struct ccd_image *image)
{
    gchar str[NAME_STRING_LENGTH + 2];

    image->changed = TRUE;
    strcpy(str, image->name);
    strcat(str, " *");
    gnome_mdi_child_set_name(child, str);
}
static void imageUpdate(struct ccd_image *image)
{
    int                   dst_rowstride, src_depthbytes, x, y;
    unsigned int          src_pixel, src_offset, mask, width, height;
    unsigned char         *dst_pixels, filter[4][2][3];
    float                 contrast_scale;
    GdkPixbuf            *pixbuf, *scale_pixbuf;

    /*
     * Load image into displayable pixmap.
     */
    pixbuf         = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, image->width, image->height);
    dst_pixels     = gdk_pixbuf_get_pixels(pixbuf);
    dst_rowstride  = gdk_pixbuf_get_rowstride(pixbuf);
    src_depthbytes = (image->depth + 7) / 8;
    if (image->view.ContrastStretch)
    {
        ccd_image_histogram(image);
        contrast_scale = 255.5 / (image->pixmax - image->pixmin);
        src_offset = image->pixmin;
    }
    else
    {
        switch (src_depthbytes)
        {
            case 1:
                contrast_scale = 1.0;
                break;
            case 2:
                contrast_scale = 1.0 / 256.0;
                break;
            case 4:
            default:
                contrast_scale = 1.0 / 16777216.0;
                break;
        }
        src_offset = 0;
    }
    if (image->view.Color && (mask = (image->color & image->filter)))
    {
        filter[0][0][0] = mask & 0x100 ? 0xFF : 0x00;
        filter[0][0][1] = mask & 0x010 ? 0xFF : 0x00;
        filter[0][0][2] = mask & 0x001 ? 0xFF : 0x00;
        filter[0][1][0] = mask & 0x200 ? 0xFF : 0x00;
        filter[0][1][1] = mask & 0x020 ? 0xFF : 0x00;
        filter[0][1][2] = mask & 0x002 ? 0xFF : 0x00;
        filter[1][0][0] = mask & 0x400 ? 0xFF : 0x00;
        filter[1][0][1] = mask & 0x040 ? 0xFF : 0x00;
        filter[1][0][2] = mask & 0x004 ? 0xFF : 0x00;
        filter[1][1][0] = mask & 0x800 ? 0xFF : 0x00;
        filter[1][1][1] = mask & 0x080 ? 0xFF : 0x00;
        filter[1][1][2] = mask & 0x008 ? 0xFF : 0x00;
        if (image->color & CCD_COLOR_MATRIX_ALT_EVEN)
        {
            filter[2][1][0] = mask & 0x100 ? 0xFF : 0x00;
            filter[2][1][1] = mask & 0x010 ? 0xFF : 0x00;
            filter[2][1][2] = mask & 0x001 ? 0xFF : 0x00;
            filter[2][0][0] = mask & 0x200 ? 0xFF : 0x00;
            filter[2][0][1] = mask & 0x020 ? 0xFF : 0x00;
            filter[2][0][2] = mask & 0x002 ? 0xFF : 0x00;
        }
        else
        {
            filter[2][0][0] = mask & 0x100 ? 0xFF : 0x00;
            filter[2][0][1] = mask & 0x010 ? 0xFF : 0x00;
            filter[2][0][2] = mask & 0x001 ? 0xFF : 0x00;
            filter[2][1][0] = mask & 0x200 ? 0xFF : 0x00;
            filter[2][1][1] = mask & 0x020 ? 0xFF : 0x00;
            filter[2][1][2] = mask & 0x002 ? 0xFF : 0x00;
        }
        if (image->color & CCD_COLOR_MATRIX_ALT_ODD)
        {
            filter[3][1][0] = mask & 0x400 ? 0xFF : 0x00;
            filter[3][1][1] = mask & 0x040 ? 0xFF : 0x00;
            filter[3][1][2] = mask & 0x004 ? 0xFF : 0x00;
            filter[3][0][0] = mask & 0x800 ? 0xFF : 0x00;
            filter[3][0][1] = mask & 0x080 ? 0xFF : 0x00;
            filter[3][0][2] = mask & 0x008 ? 0xFF : 0x00;
        }
        else
        {
            filter[3][0][0] = mask & 0x400 ? 0xFF : 0x00;
            filter[3][0][1] = mask & 0x040 ? 0xFF : 0x00;
            filter[3][0][2] = mask & 0x004 ? 0xFF : 0x00;
            filter[3][1][0] = mask & 0x800 ? 0xFF : 0x00;
            filter[3][1][1] = mask & 0x080 ? 0xFF : 0x00;
            filter[3][1][2] = mask & 0x008 ? 0xFF : 0x00;
        }
    }
    else
    {
        memset(filter, 0xFF, 4*2*3);
    }

#define PALETTE_RED(p)      ((view_palettes[image->view.Palette][p])&0xFF)
#define PALETTE_GREEN(p)    (((view_palettes[image->view.Palette][p])>>8)&0xFF)
#define PALETTE_BLUE(p)     (((view_palettes[image->view.Palette][p])>>16)&0xFF)
#define PIXEL_LOOP(pixel_type)                                                                                      \
    for (y = 0; y < image->height; y++)                                                                             \
        for (x = 0; x < image->width; x++)                                                                          \
        {                                                                                                           \
            src_pixel = (((pixel_type *)image->pixels)[y * image->width + x] - src_offset) * contrast_scale;        \
            dst_pixels[y * dst_rowstride + x * 3 + 0] = PALETTE_RED(src_pixel)   & filter[y & 3][x & 1][0];         \
            dst_pixels[y * dst_rowstride + x * 3 + 1] = PALETTE_GREEN(src_pixel) & filter[y & 3][x & 1][1];         \
            dst_pixels[y * dst_rowstride + x * 3 + 2] = PALETTE_BLUE(src_pixel)  & filter[y & 3][x & 1][2];         \
        }

    PIXEL_SIZE_CASE(src_depthbytes);
#undef PIXEL_LOOP
#undef PALETTE_BLUE
#undef PALETTE_GREEN
#undef PALETTE_RED

    if (image->view.AspectStretch)
    {
        if (image->pixel_height > image->pixel_width)
        {
            if (image->view.BinStretch && image->xbin && image->ybin)
            {
                width  = image->width  * image->xbin;
                height = image->height * image->ybin * ((float)image->pixel_height / (float)image->ybin) / ((float)image->pixel_width / (float)image->xbin);
            }
            else
            {
                width  = image->width;
                height = image->height * (float)image->pixel_height / (float)image->pixel_width;
            }
        }
        else
        {
            if (image->view.BinStretch && image->xbin && image->ybin)
            {
                width  = image->width  * image->xbin * ((float)image->pixel_width / (float)image->xbin) / ((float)image->pixel_height / (float)image->ybin);
                height = image->height * image->ybin;
            }
            else
            {
                width  = image->width * (float)image->pixel_width / (float)image->pixel_height;
                height = image->height;
            }
        }
        scale_pixbuf = gdk_pixbuf_scale_simple(pixbuf, width, height, GDK_INTERP_BILINEAR);
        gdk_pixbuf_unref(pixbuf);
        pixbuf = scale_pixbuf;
    }
    else if (image->view.BinStretch)
    {
        width  = image->width  * (image->xbin ? image->xbin : 1);
        height = image->height * (image->ybin ? image->ybin : 1);
        if (width != image->width || height != image->height)
        {
            scale_pixbuf = gdk_pixbuf_scale_simple(pixbuf, width, height, GDK_INTERP_BILINEAR);
            gdk_pixbuf_unref(pixbuf);
            pixbuf = scale_pixbuf;
        }
    }
    else
    {
        width  = image->width;
        height = image->height;
    }
    if (image->pixmap)
    {
        gdk_pixmap_unref(image->pixmap);
        image->pixmap = NULL;
    }
    gtk_widget_push_visual(gdk_rgb_get_visual());
    gtk_widget_push_colormap(gdk_rgb_get_cmap());
    image->pixmap = gdk_pixmap_new(GTK_WIDGET(gnome_mdi_get_active_window(GNOME_MDI(mdi)))->window, width, height, -1);
    gtk_widget_pop_colormap();
    gtk_widget_pop_visual();
    gdk_pixbuf_render_to_drawable(pixbuf, image->pixmap, GTK_WIDGET(gnome_mdi_get_active_window(GNOME_MDI(mdi)))->style->fg_gc[GTK_STATE_NORMAL],
                                  0, 0, 0, 0, width, height,
                                  GDK_RGB_DITHER_MAX, 0, 0);
    gdk_pixbuf_unref(pixbuf);
    if (image->draw_area)
        gtk_drawing_area_size(GTK_DRAWING_AREA(image->draw_area), width, height);
    histogramUpdate(image);
}
static void imageRemoveNoise(struct ccd_image *image)
{
    unsigned int    i, j, k, l, x, y;
    unsigned long  sorted_pixels[9], sig, noise_hi, noise_lo;

#define PIXEL_LOOP(pixel_type)                                                                                      \
    for (y = 1; y < image->height - 1; y++)                                                                         \
        for (x = 1; x < image->width - 1; x++)                                                                      \
        {                                                                                                           \
            for (k = 0; k < 9; k++)                                                                                 \
                sorted_pixels[k] = 0xFFFFFFFF;                                                                      \
            for (j = y - 1; j <= y + 1; j++)                                                                        \
                for (i = x - 1; i <= x + 1; i++)                                                                    \
                    for (k = 0; k < 9; k++)                                                                         \
                        if (((pixel_type *)image->pixels)[j * image->width + i] < sorted_pixels[k])                 \
                        {                                                                                           \
                            for (l = 8; l > k; l--)                                                                 \
                                sorted_pixels[l] = sorted_pixels[l - 1];                                            \
                            sorted_pixels[k] = ((pixel_type *)image->pixels)[j * image->width + i];                 \
                            break;                                                                                  \
                        }                                                                                           \
            sig = sorted_pixels[7] - sorted_pixels[1];                                                              \
            noise_hi = (long)sorted_pixels[4] + sig > image->datamax ? image->datamax : sorted_pixels[4] + sig;     \
            noise_lo = (long)sorted_pixels[4] - sig < 0     ? 0      : sorted_pixels[4] - sig;                      \
            if (((pixel_type *)image->pixels)[y * image->width + x] > noise_hi)                                     \
                ((pixel_type *)image->pixels)[y * image->width + x] = sorted_pixels[4];                             \
            if (((pixel_type *)image->pixels)[y * image->width + x] < noise_lo)                                     \
                ((pixel_type *)image->pixels)[y * image->width + x] = sorted_pixels[4];                             \
        }

    PIXEL_SIZE_CASE((image->depth + 7) / 8);
#undef PIXEL_LOOP

    image->pixmax = image->pixmin = 0;
    ccd_image_histogram(image);
}
static void imageRemoveBackground(struct ccd_image *image)
{
    unsigned int          x, y, src_offset, max_count, max_index, histogram[256];
    float                 histogram_scale;

    /*
     * Get histogram of image values
     */
    ccd_image_histogram(image);
    histogram_scale = 255.0 / (image->pixmax - image->pixmin);
    src_offset = image->pixmin;
    for (y = 0; y < 256; y++)
        histogram[y] = 0;
    max_count = 0;
    max_index = 0;

#define PIXEL_LOOP(pixel_type)                                                                                          \
    for (y = 0; y < image->height; y++)                                                                                 \
        for (x = 0; x < image->width; x++)                                                                              \
            histogram[(int)((((pixel_type *)image->pixels)[y * image->width + x] - src_offset) * histogram_scale)]++;   \
        for (y = 0; y < 256; y++)                                                                                       \
            if (histogram[y] > max_count)                                                                               \
            {                                                                                                           \
                max_count = histogram[y];                                                                               \
                max_index = y;                                                                                          \
            }                                                                                                           \
        src_offset = (max_index / histogram_scale + image->pixmin) * 0.9;                                               \

    PIXEL_SIZE_CASE((image->depth + 7) / 8);
#undef PIXEL_LOOP

    ccd_image_sub(image, src_offset);
}
static void imageNewChild(struct ccd_image *image)
{
    gchar str[NAME_STRING_LENGTH + 2];

    image->view.Toolbar         = prefs.ViewToolbar;
    image->view.ContrastStretch = prefs.ViewContrast;
    image->view.AspectStretch   = prefs.ViewAspect;
    image->view.BinStretch      = prefs.ViewBin;
    image->view.Color           = prefs.ViewColor;
    image->view.Palette         = prefs.ViewPalette;
    image->pixmap               = NULL;
    image->draw_area            = NULL;
    imageUpdate(image);
    strcpy(str, image->name);
    if (image->changed)
        strcat(str, " *");
    image->child = gnome_mdi_generic_child_new(str);
    gtk_object_set_user_data(GTK_OBJECT(image->child), (gpointer)image);
    gnome_mdi_add_child(GNOME_MDI(mdi), GNOME_MDI_CHILD(image->child));
    gnome_mdi_generic_child_set_view_creator(image->child, cbViewCreate, image);
    gnome_mdi_add_view(GNOME_MDI(mdi), GNOME_MDI_CHILD(image->child));
    imageUpdateList();
}

/***************************************************************************\
*                                                                           *
*                          Menu/toolbar callbacks                           *
*                                                                           *
\***************************************************************************/

static void cbAcquire(GtkObject *object, gpointer data)
{
    /*
     * Open/show acquire image dialog.
     */
    activateAcquireImage(imageNewChild);
}
static void cbOpenOK(GtkWidget *widget, gpointer data)
{
    char                  filename[PATH_MAX];
    struct ccd_image     *image;

    /*
     * Get path from selection dialog.
     */
    strcpy(filename, gtk_file_selection_get_filename(GTK_FILE_SELECTION(data)));
    gtk_widget_destroy(GTK_WIDGET(data));
    /*
     * Load image.
     */
    if ((image = ccd_image_new_from_file(filename)))
    {
        image->changed = FALSE;
        imageNewChild(image);
        chdir(image->dir);
    }
    else
    {
        gnome_warning_dialog_parented(_("Unable to load file.  Probably invalid FITS format."), GTK_WINDOW(gnome_mdi_get_active_window(GNOME_MDI(mdi))));
    }
}
static void cbOpen(GtkObject *object, gpointer data)
{
    dlgGetFilename(_("Open File"), NULL, cbOpenOK);
}
static struct
{
    struct ccd_image *image;
    GtkWidget        *dialog, *entry[100];
} prop_edit;
static void cbPropExit(GtkWidget *widget, gint button, gpointer data)
{
    gchar *string;
    gint   i, j;

    if (button == 0)
    {
        if (strcmp(prop_edit.image->name, (string = gtk_entry_get_text(GTK_ENTRY(prop_edit.entry[0])))))
        {
            /*
             * Name changed.
             */
            if (ccd_image_find_by_name(string))
            {
                gnome_warning_dialog_parented(_("Already an image with that name.  Select a unique name."), GTK_WINDOW(gnome_mdi_get_active_window(GNOME_MDI(mdi))));
            }
            else if (string[0] == '\0')
            {
                gnome_warning_dialog_parented(_("Invalid name.  Must be non-empty."), GTK_WINDOW(gnome_mdi_get_active_window(GNOME_MDI(mdi))));
            }
            else
            {
                strcpy(prop_edit.image->name, string);
                imageUpdateList();
            }
        }
        i = 11;
        strcpy(prop_edit.image->object,    gtk_entry_get_text(GTK_ENTRY(prop_edit.entry[i++])));
        strcpy(prop_edit.image->observer,  gtk_entry_get_text(GTK_ENTRY(prop_edit.entry[i++])));
        strcpy(prop_edit.image->location,  gtk_entry_get_text(GTK_ENTRY(prop_edit.entry[i++])));
        i++;
        strcpy(prop_edit.image->telescope, gtk_entry_get_text(GTK_ENTRY(prop_edit.entry[i++])));
        for (j = 0; j < MAX_PROCESS_HISTORY; j++)
            strcpy(prop_edit.image->history[j], gtk_entry_get_text(GTK_ENTRY(prop_edit.entry[i++])));
        for (j = 0; j < MAX_COMMENTS; j++)
            strcpy(prop_edit.image->comments[j], gtk_entry_get_text(GTK_ENTRY(prop_edit.entry[i++])));
        imageChanged(gnome_mdi_get_active_child(GNOME_MDI(mdi)), prop_edit.image);
    }
    gtk_widget_destroy(widget);
}
#define PROP_LABEL_WIDTH 100
static void cbProp(GtkObject *object, gpointer data)
{
    char              str[80];
    GtkWidget        *scrolled_win, *vbox, *hbox, *label;
    struct ccd_image *image;
    int               j, i = 0;

    /*
     * Edit current image properties.
     */
    if (gnome_mdi_get_active_child(GNOME_MDI(mdi)) && (image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(gnome_mdi_get_active_child(GNOME_MDI(mdi))))))
    {
        prop_edit.image  = image;
        prop_edit.dialog = gnome_dialog_new(_("Image Header"), GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
        gtk_window_set_modal(GTK_WINDOW(prop_edit.dialog), TRUE);
        gnome_dialog_set_default(GNOME_DIALOG(prop_edit.dialog), 0);
        gtk_window_set_transient_for(GTK_WINDOW(prop_edit.dialog), GTK_WINDOW(gnome_mdi_get_active_window(GNOME_MDI(mdi))));
        scrolled_win = gtk_scrolled_window_new(NULL, NULL);
        vbox         = gtk_vbox_new(FALSE, 0);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_win), vbox);
        gtk_object_set(GTK_OBJECT(scrolled_win), "height", 300, NULL);
        gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(prop_edit.dialog)->vbox), scrolled_win, TRUE, TRUE, 0);

#define EDIT_ENTRY(name, editable, str)                                             \
        hbox               = gtk_hbox_new(FALSE, 0);                                \
        label              = gtk_label_new(_(name));                                \
        prop_edit.entry[i] = gtk_entry_new_with_max_length(DEFAULT_STRING_LENGTH);  \
        gtk_object_set(GTK_OBJECT(label), "width", PROP_LABEL_WIDTH, NULL);         \
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);                          \
        gtk_entry_set_editable(GTK_ENTRY(prop_edit.entry[i]), editable);            \
        gtk_entry_set_text(GTK_ENTRY(prop_edit.entry[i]), str);                     \
        gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);                    \
        gtk_box_pack_start(GTK_BOX(hbox), prop_edit.entry[i], TRUE, TRUE, 0);       \
        gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);                     \
        i++;

        EDIT_ENTRY("NAME", TRUE, image->name);
        sprintf(str, "%d", image->depth);
        EDIT_ENTRY("BITPIX", FALSE, str);
        sprintf(str, "%d", image->width);
        EDIT_ENTRY("WIDTH", FALSE, str);
        sprintf(str, "%d", image->height);
        EDIT_ENTRY("HEIGHT", FALSE, str);
        sprintf(str, "%f", image->pixel_width);
        EDIT_ENTRY("PIXWIDTH", FALSE, str);
        sprintf(str, "%f", image->pixel_height);
        EDIT_ENTRY("PIXHEIGHT", FALSE, str);
        sprintf(str, "0x%04X", image->color);
        EDIT_ENTRY("COLOR FORMAT", FALSE, str);
        sprintf(str, "0x%04X", image->filter);
        EDIT_ENTRY("FILTER", FALSE, str);
        EDIT_ENTRY("DATE", FALSE, image->date);
        EDIT_ENTRY("TIME", FALSE, image->time);
        sprintf(str, "%f", (float)image->exposure/1000.0);
        EDIT_ENTRY("EXPOSURE", FALSE, str);
        EDIT_ENTRY("OBJECT", TRUE, image->object);
        EDIT_ENTRY("OBSERVER", TRUE, image->observer);
        EDIT_ENTRY("LOCATION", TRUE, image->location);
        EDIT_ENTRY("INSTRUMENT", FALSE, image->camera);
        EDIT_ENTRY("TELESCOPE", TRUE, image->telescope);
        for (j = 0; j < MAX_PROCESS_HISTORY; j++)
        {
            EDIT_ENTRY("HISTORY", TRUE, image->history[j]);
        }
        for (j = 0; j < MAX_COMMENTS; j++)
        {
            EDIT_ENTRY("COMMENT", TRUE, image->comments[j]);
        }
#undef EDIT_ENTRY

        gtk_signal_connect(GTK_OBJECT(prop_edit.dialog), "clicked", cbPropExit, NULL);
        gtk_widget_show_all(prop_edit.dialog);
    }
}
static void cbSaveAsOK(GtkWidget *widget, gpointer data)
{
    char             filename[PATH_MAX];
    struct ccd_image *image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(gnome_mdi_get_active_child(GNOME_MDI(mdi))));
    /*
     * Get path from selection dialog.
     */
    strcpy(filename, gtk_file_selection_get_filename(GTK_FILE_SELECTION(data)));
    gtk_widget_destroy(GTK_WIDGET(data));
    /*
     * Update name and save.
     */
    ccd_image_set_dir_name_ext(image, filename);
    if (ccd_image_save_fits(image) == 0)
    {
        gnome_mdi_child_set_name(gnome_mdi_get_active_child(GNOME_MDI(mdi)), image->name);
        imageUpdateList();
        image->changed = FALSE;
    }
}
static void cbSaveAs(GtkObject *object, gpointer data)
{
    struct ccd_image *image;
    GnomeMDIChild    *child = gnome_mdi_get_active_child(GNOME_MDI(mdi));

    if (child && (image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(child))))
    {
        gchar filename[PATH_MAX];
        filename[0] = '\0';
        strcat(filename, image->dir);
        strcat(filename, "/");
        strcat(filename, image->name);
        if (image->ext[0])
        {
            strcat(filename, ".");
            strcat(filename, image->ext);
        }
        dlgGetFilename(_("Save File"), filename, cbSaveAsOK);
    }
}
static void cbSave(GtkObject *object, gpointer data)
{
    struct ccd_image *image;
    GnomeMDIChild    *child = gnome_mdi_get_active_child(GNOME_MDI(mdi));

    if (child && (image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(child))))
    {
       if (image->changed && (ccd_image_save_fits(image) == 0))
       {
           gnome_mdi_child_set_name(child, image->name);
           image->changed = FALSE;
       }
    }
}
static void cbSaveAll(GtkObject *object, gpointer data)
{
    struct ccd_image *image;

    for (image = ccd_image_first(); image; image = ccd_image_next(image))
    {
        if (image->changed && (ccd_image_save_fits(image) == 0))
        {
            gnome_mdi_child_set_name(GNOME_MDI_CHILD(image->child), image->name);
            image->changed = FALSE;
        }
    }
}
static void cbClose(GtkObject *object, gpointer data)
{
    struct ccd_image *image;
    GtkWidget        *messagebox;
    GnomeMDIChild    *child = gnome_mdi_get_active_child(GNOME_MDI(mdi));

    if (child && (image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(child))))
    {
       if (image->changed && prefs.CloseWarning)
       {
           messagebox = gnome_message_box_new(_("Image not saved"),
                                              GNOME_MESSAGE_BOX_WARNING,
                                              GNOME_STOCK_BUTTON_CLOSE, GNOME_STOCK_BUTTON_CANCEL, NULL);
           if (gnome_dialog_run(GNOME_DIALOG(messagebox)) == 1)
               return;
       }
       if (image->histogram_view)
           gtk_widget_destroy(image->histogram_view);
       image->changed = FALSE;
       gnome_mdi_remove_child(GNOME_MDI(mdi), child, FALSE);
    }
}
static void cbCloseAll(GtkObject *object, gpointer data)
{
    struct ccd_image *image;
    GtkWidget        *messagebox;
    GnomeMDIChild    *child;
    gint              close_ok = FALSE;

    while ((child = gnome_mdi_get_active_child(GNOME_MDI(mdi))) && (image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(child))))
    {
       if (image->changed && !close_ok && prefs.CloseWarning)
       {
           messagebox = gnome_message_box_new(_("Images not saved"),
                                              GNOME_MESSAGE_BOX_WARNING,
                                              GNOME_STOCK_BUTTON_CLOSE, GNOME_STOCK_BUTTON_CANCEL, NULL);
           if (gnome_dialog_run(GNOME_DIALOG(messagebox)) == 1)
               return;
           close_ok = TRUE;
       }
       if (image->histogram_view)
           gtk_widget_destroy(image->histogram_view);
       image->changed = FALSE;
       gnome_mdi_remove_child(GNOME_MDI(mdi), child, FALSE);
    }
}
static void cbExit(GtkObject *object, gpointer data)
{
    struct ccd_image *image;
    GtkWidget        *messagebox;
    gint              close_ok = FALSE;

    /*
     * Make sure all the images have been saved.
     */
    for (image = ccd_image_first(); image; image = ccd_image_next(image))
    {
        if (image->changed && !close_ok && prefs.CloseWarning)
        {
            messagebox = gnome_message_box_new(_("Images not saved"),
                                               GNOME_MESSAGE_BOX_WARNING,
                                               GNOME_STOCK_BUTTON_CLOSE, GNOME_STOCK_BUTTON_CANCEL, NULL);
            if (gnome_dialog_run(GNOME_DIALOG(messagebox)) == 1)
                return;
            close_ok = TRUE;
        }
        if (image->histogram_view)
            gtk_widget_destroy(image->histogram_view);
        image->changed = FALSE;
    }
    gtk_main_quit();
}
static void cbDirPrefsOK(GtkWidget *widget, gpointer data)
{
    /*
     * Get path from selection dialog.
     */
    strcpy(prefs.WorkingDirectory, gtk_file_selection_get_filename(GTK_FILE_SELECTION(data)));
    chdir(prefs.WorkingDirectory);
    getcwd(prefs.WorkingDirectory, DIR_STRING_LENGTH);
    gtk_widget_destroy(GTK_WIDGET(data));
}
static void cbDirPrefs(GtkObject *object, gpointer data)
{
    gchar path[DIR_STRING_LENGTH + 1];
    GtkWidget *select_dir;

    /*
     * This is a very cheesy way to get a directory entry.  It was just too hard to do it by hand.
     */
    strcpy(path, prefs.WorkingDirectory);
    strcat(path, "/.");
    select_dir = gtk_file_selection_new(_("Select Working Directory"));
    gtk_signal_connect(GTK_OBJECT(select_dir), "destroy", cbGetFilenameDestroy, NULL);
    gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(select_dir)->ok_button), "clicked", cbDirPrefsOK, (gpointer)select_dir);
    gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(select_dir)->cancel_button), "clicked", gtk_widget_destroy, (gpointer)select_dir);
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(select_dir), path);
    gtk_widget_show(select_dir);
    gtk_widget_hide(GTK_FILE_SELECTION(select_dir)->file_list->parent);
    gtk_grab_add(select_dir);
}
static void cbClosePrefsToggle(GtkObject *object, gpointer data)
{
    prefs.CloseWarning = GTK_CHECK_MENU_ITEM(object)->active;
}
static void cbPortPrefsClose(GtkWidget *widget, gpointer data)
{
    gtk_widget_destroy((GtkWidget *)data);
}
static void cbPortPrefs(GtkObject *object, gpointer data)
{
    int        fd;
    GtkWidget *radiobutton;
    GtkWidget *vbox        = gtk_vbox_new(FALSE, 0);
    GtkWidget *hbox        = gtk_hbox_new(FALSE, 0);
    GtkWidget *frame_scope = gtk_frame_new(_("Telescope"));
    GtkWidget *vbox_scope  = gtk_vbox_new(FALSE, 0);
    GtkWidget *frame_wheel = gtk_frame_new(_("Filter Wheel"));
    GtkWidget *vbox_wheel  = gtk_vbox_new(FALSE, 0);
    GtkWidget *button      = gtk_button_new_with_label(_("Close"));
    GtkWidget *window      = gtk_window_new(GTK_WINDOW_DIALOG);
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(gnome_mdi_get_active_window(GNOME_MDI(mdi))));
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    gtk_window_set_title(GTK_WINDOW(window), _("COM Ports"));
    gtk_signal_connect(GTK_OBJECT(window), "delete_event", GTK_SIGNAL_FUNC(eventPrefsDie),    NULL);
    gtk_signal_connect(GTK_OBJECT(window), "destroy",      GTK_SIGNAL_FUNC(eventPrefsDie),    NULL);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",      GTK_SIGNAL_FUNC(cbPortPrefsClose), (gpointer)window);
    radiobutton = gtk_radio_button_new_with_label(NULL, _("(None)"));
    gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbPrefsPortScope), (gpointer)-1);
    if (prefs.PortScope == -1)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox_scope), radiobutton, TRUE, TRUE, 0);
    radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "COM: 1");
    gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbPrefsPortScope), (gpointer)0);
    if (prefs.PortScope == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox_scope), radiobutton, TRUE, TRUE, 0);
    radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "COM: 2");
    gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbPrefsPortScope), (gpointer)1);
    if (prefs.PortScope == 1)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox_scope), radiobutton, TRUE, TRUE, 0);
    radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "COM: 3");
    gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbPrefsPortScope), (gpointer)2);
    if (prefs.PortScope == 2)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox_scope), radiobutton, TRUE, TRUE, 0);
    radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "COM: 4");
    gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbPrefsPortScope), (gpointer)3);
    if (prefs.PortScope == 3)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox_scope), radiobutton, TRUE, TRUE, 0);
    if ((fd = open("/dev/ttysx3", O_RDWR, 0)) > 0)
    {
        close(fd);
        radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "SXUSB1: S2K");
        gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbPrefsPortScope), (gpointer)7);
        if (prefs.PortScope == 7)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
        gtk_box_pack_start(GTK_BOX(vbox_scope), radiobutton, TRUE, TRUE, 0);
    }
    gtk_container_add(GTK_CONTAINER(frame_scope), vbox_scope);
    gtk_box_pack_start(GTK_BOX(hbox), frame_scope, TRUE, TRUE, 0);
    radiobutton = gtk_radio_button_new_with_label(NULL, _("(None)"));
    gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbPrefsPortWheel), (gpointer)-1);
    if (prefs.PortWheel == -1)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox_wheel), radiobutton, TRUE, TRUE, 0);
    radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "COM: 1");
    gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbPrefsPortWheel), (gpointer)0);
    if (prefs.PortWheel == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox_wheel), radiobutton, TRUE, TRUE, 0);
    radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "COM: 2");
    gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbPrefsPortWheel), (gpointer)1);
    if (prefs.PortWheel == 1)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox_wheel), radiobutton, TRUE, TRUE, 0);
    radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "COM: 3");
    gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbPrefsPortWheel), (gpointer)2);
    if (prefs.PortWheel == 2)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox_wheel), radiobutton, TRUE, TRUE, 0);
    radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "COM: 4");
    gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbPrefsPortWheel), (gpointer)3);
    if (prefs.PortWheel == 3)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox_wheel), radiobutton, TRUE, TRUE, 0);
    if ((fd = open("/dev/ttysx0", O_RDWR, 0)) > 0)
    {
        close(fd);
        radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "SXUSB1: SER 1");
        gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbPrefsPortWheel), (gpointer)4);
        if (prefs.PortWheel == 4)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
        gtk_box_pack_start(GTK_BOX(vbox_wheel), radiobutton, TRUE, TRUE, 0);
    }
    if ((fd = open("/dev/ttysx1", O_RDWR, 0)) > 0)
    {
        close(fd);
        radiobutton = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(radiobutton)), "SXUSB1: SER 2");
        gtk_signal_connect(GTK_OBJECT(radiobutton), "toggled", GTK_SIGNAL_FUNC(cbPrefsPortWheel), (gpointer)5);
        if (prefs.PortWheel == 5)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton), TRUE);
        gtk_box_pack_start(GTK_BOX(vbox_wheel), radiobutton, TRUE, TRUE, 0);
    }
    gtk_container_add(GTK_CONTAINER(frame_wheel), vbox_wheel);
    gtk_box_pack_start(GTK_BOX(hbox), frame_wheel, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(vbox), button, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_widget_show_all(window);
}
static void cbToolbarToggle(GtkObject *object, gpointer data)
{
    GnomeDockItem    *dockToolbar;
    struct ccd_image *image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(gnome_mdi_get_active_child(GNOME_MDI(mdi))));
    GnomeApp         *app   = gnome_mdi_get_active_window(GNOME_MDI(mdi));
    if ((dockToolbar = gnome_dock_get_item_by_name(GNOME_DOCK(app->dock), GNOME_APP_TOOLBAR_NAME, NULL, NULL, NULL, NULL)))
    {
        if ((prefs.ViewToolbar = GTK_CHECK_MENU_ITEM(object)->active))
            gtk_widget_show(GTK_WIDGET(dockToolbar));
        else
            gtk_widget_hide(GTK_WIDGET(dockToolbar));
        if (image)
            image->view.Toolbar = prefs.ViewToolbar;
    }
    gtk_widget_queue_resize(GTK_WIDGET(app->dock));
}
static void cbViewHistogram(GtkObject *object, gpointer data)
{
    struct ccd_image *image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(gnome_mdi_get_active_child(GNOME_MDI(mdi))));
    if (image && !image->histogram_view)
    {
        char       str[80];
        GtkWidget *hbox, *hframe, *sframe, *view;

        hbox                   = gtk_hbox_new(FALSE, 0);
        hframe                 = gtk_frame_new(_("Log Graph"));
        sframe                 = gtk_frame_new(_("ADUs"));
        image->histogram_label = gtk_label_new(NULL);
        view                   = gtk_drawing_area_new();
        image->histogram_view  = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_object_set_user_data(GTK_OBJECT(image->histogram_view), (gpointer)image);
        gtk_signal_connect(GTK_OBJECT(image->histogram_view), "delete_event", GTK_SIGNAL_FUNC(eventViewHistogramDie), NULL);
        gtk_signal_connect(GTK_OBJECT(image->histogram_view), "destroy",      GTK_SIGNAL_FUNC(eventViewHistogramDie), NULL);
        gtk_signal_connect(GTK_OBJECT(view),                  "expose_event", GTK_SIGNAL_FUNC(cbViewHistogramExpose), (gpointer)image);
        gtk_box_pack_start(GTK_BOX(hbox), hframe, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), sframe, TRUE, TRUE, 0);
        gtk_drawing_area_size(GTK_DRAWING_AREA(view), HISTOGRAM_BINS, 256);
        gtk_container_add(GTK_CONTAINER(hframe), view);
        gtk_container_add(GTK_CONTAINER(sframe), image->histogram_label);
        gtk_container_add(GTK_CONTAINER(image->histogram_view), hbox);
        sprintf(str, "%s: %s", _("Histogram"), image->name);
        gtk_window_set_title(GTK_WINDOW(image->histogram_view), str);
        gtk_widget_show_all(image->histogram_view);
        histogramUpdate(image);
    }
}
static void cbViewContrastToggle(GtkObject *object, gpointer data)
{
    struct ccd_image *image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(gnome_mdi_get_active_child(GNOME_MDI(mdi))));
    prefs.ViewContrast = GTK_CHECK_MENU_ITEM(object)->active;
    if (image)
    {
        image->view.ContrastStretch = prefs.ViewContrast;
        imageUpdate(image);
        gtk_widget_queue_draw(gnome_mdi_get_active_view(GNOME_MDI(mdi)));
    }
}
static void cbViewAspectToggle(GtkObject *object, gpointer data)
{
    struct ccd_image *image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(gnome_mdi_get_active_child(GNOME_MDI(mdi))));
    prefs.ViewAspect = GTK_CHECK_MENU_ITEM(object)->active;
    if (image)
    {
        image->view.AspectStretch = prefs.ViewAspect;
        imageUpdate(image);
        gtk_widget_queue_draw(gnome_mdi_get_active_view(GNOME_MDI(mdi)));
    }
}
static void cbViewColorToggle(GtkObject *object, gpointer data)
{
    struct ccd_image *image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(gnome_mdi_get_active_child(GNOME_MDI(mdi))));
    prefs.ViewColor = GTK_CHECK_MENU_ITEM(object)->active;
    if (image)
    {
        image->view.Color = prefs.ViewColor;
        imageUpdate(image);
        gtk_widget_queue_draw(gnome_mdi_get_active_view(GNOME_MDI(mdi)));
    }
}
static void cbViewBinToggle(GtkObject *object, gpointer data)
{
    struct ccd_image *image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(gnome_mdi_get_active_child(GNOME_MDI(mdi))));
    prefs.ViewBin = GTK_CHECK_MENU_ITEM(object)->active;
    if (image)
    {
        image->view.BinStretch = prefs.ViewBin;
        imageUpdate(image);
        gtk_widget_queue_draw(gnome_mdi_get_active_view(GNOME_MDI(mdi)));
    }
}
static void cbColorPalette(GtkObject *object, gpointer data)
{
    int               i;
    GnomeApp         *app   = gnome_mdi_get_active_window(GNOME_MDI(mdi));
    GnomeUIInfo      *menu  = gnome_mdi_get_menubar_info(app);
    GnomeUIInfo      *view  = (GnomeUIInfo *)menu[2].moreinfo;
    GnomeUIInfo      *pal   = (GnomeUIInfo *)view[7].moreinfo;
    struct ccd_image *image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(gnome_mdi_get_active_child(GNOME_MDI(mdi))));
    prefs.ViewPalette       = GPOINTER_TO_UINT(gtk_object_get_data(object, GNOMEUIINFO_KEY_UIDATA));
    for (i = 0; i < NUM_PALETTES; i++)
        GTK_CHECK_MENU_ITEM(pal[i].widget)->active = (prefs.ViewPalette == i);
    if (image)
    {
        image->view.Palette = prefs.ViewPalette;
        imageUpdate(image);
        gtk_widget_queue_draw(gnome_mdi_get_active_view(GNOME_MDI(mdi)));
    }
}
static void cbViewMode(GtkObject *object, gpointer data)
{
    GnomeMDIMode mode = (GnomeMDIMode)(GPOINTER_TO_UINT(gtk_object_get_data(object, GNOMEUIINFO_KEY_UIDATA)));
    gnome_mdi_set_mode(GNOME_MDI(mdi), mode);
    prefs.ViewMode = mode;
}
static void cbImageFlip(GtkObject *object, gpointer data)
{
    struct ccd_image *image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(gnome_mdi_get_active_child(GNOME_MDI(mdi))));
    int               dir   = GPOINTER_TO_UINT(gtk_object_get_data(object, GNOMEUIINFO_KEY_UIDATA));
    if (image)
    {
        gdk_window_set_cursor(gnome_mdi_get_active_view(GNOME_MDI(mdi))->window, cursorWait);
        gdk_flush();
        if (dir)
            ccd_image_flip_horiz(image);
        else
            ccd_image_flip_vert(image);
        imageUpdate(image);
        imageChanged(gnome_mdi_get_active_child(GNOME_MDI(mdi)), image);
        gtk_widget_queue_draw(gnome_mdi_get_active_view(GNOME_MDI(mdi)));
        gdk_window_set_cursor(gnome_mdi_get_active_view(GNOME_MDI(mdi))->window, NULL);
    }
}
static void cbImageRotate(GtkObject *object, gpointer data)
{
    int               angle = GPOINTER_TO_UINT(gtk_object_get_data(object, GNOMEUIINFO_KEY_UIDATA));
    struct ccd_image *image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(gnome_mdi_get_active_child(GNOME_MDI(mdi))));
    if (image)
    {
        gdk_window_set_cursor(gnome_mdi_get_active_view(GNOME_MDI(mdi))->window, cursorWait);
        gdk_flush();
        ccd_image_rotate(image, angle);
        imageUpdate(image);
        imageChanged(gnome_mdi_get_active_child(GNOME_MDI(mdi)), image);
        gtk_widget_queue_draw(gnome_mdi_get_active_view(GNOME_MDI(mdi)));
        gdk_window_set_cursor(gnome_mdi_get_active_view(GNOME_MDI(mdi))->window, NULL);
    }
}
static void cbImageScale(GtkObject *object, gpointer data)
{
    unsigned int      scale_width, scale_height;
    int               scale = GPOINTER_TO_UINT(gtk_object_get_data(object, GNOMEUIINFO_KEY_UIDATA));
    struct ccd_image *image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(gnome_mdi_get_active_child(GNOME_MDI(mdi))));
    if (image)
    {
        gdk_window_set_cursor(gnome_mdi_get_active_view(GNOME_MDI(mdi))->window, cursorWait);
        gdk_flush();
        switch (scale)
        {
            case SCALE_ASPECT:
                if (image->pixel_height == image->pixel_width)
                    return;
                if (image->pixel_height > image->pixel_width)
                {
                    scale_width         = image->width;
                    scale_height        = image->height * image->pixel_height / image->pixel_width;
                    image->pixel_height = image->pixel_width;
                }
                else
                {
                    scale_width        = image->width * image->pixel_width / image->pixel_height;
                    scale_height       = image->height;
                    image->pixel_width = image->pixel_height;
                }
                break;
            case SCALE_HALFX:
                scale_width  = image->width  / 2;
                scale_height = image->height / 2;
                break;
            case SCALE_2X:
                scale_width  = image->width  * 2;
                scale_height = image->height * 2;
                break;
            case SCALE_4X:
                scale_width  = image->width  * 4;
                scale_height = image->height * 4;
                break;
            default:
                return;
        }
        ccd_image_scale(image, scale_width, scale_height);
        imageUpdate(image);
        imageChanged(gnome_mdi_get_active_child(GNOME_MDI(mdi)), image);
        gtk_widget_queue_draw(gnome_mdi_get_active_view(GNOME_MDI(mdi)));
        gdk_window_set_cursor(gnome_mdi_get_active_view(GNOME_MDI(mdi))->window, NULL);
    }
}
static void cbImageColorSplit(GtkObject *object, gpointer data)
{
    unsigned int      num_frames, i, colors[5];
    unsigned char    *pixels[5];
    struct ccd_image *new_image;
    struct ccd_image *image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(gnome_mdi_get_active_child(GNOME_MDI(mdi))));
    if (image && image->color != CCD_COLOR_MONOCHROME && image->color != 0)
    {
        gdk_window_set_cursor(gnome_mdi_get_active_view(GNOME_MDI(mdi))->window, cursorWait);
        gdk_flush();
        num_frames = ccd_image_split_frames(image, pixels, colors, GPOINTER_TO_UINT(gtk_object_get_data(object, GNOMEUIINFO_KEY_UIDATA)));
        for (i = 0; i < num_frames; i++)
            if (pixels[i])
            {
                new_image          = ccd_image_dup(image);
                new_image->pixmin = 0;
                new_image->pixmax = 0;
                new_image->color   = colors[i];
                new_image->pixels  = pixels[i];
                new_image->pixmap  = NULL;
                new_image->changed = TRUE;
                sprintf(new_image->name, "%s-%s", image->name, COLOR_MASK_TO_NAME(colors[i]));
                imageNewChild(new_image);
            }
        gdk_window_set_cursor(gnome_mdi_get_active_view(GNOME_MDI(mdi))->window, NULL);
    }
}
static void cbImageRemoveNoise(GtkObject *object, gpointer data)
{
    struct ccd_image *image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(gnome_mdi_get_active_child(GNOME_MDI(mdi))));
    if (image)
    {
        gdk_window_set_cursor(gnome_mdi_get_active_view(GNOME_MDI(mdi))->window, cursorWait);
        gdk_flush();
        imageRemoveNoise(image);
        imageUpdate(image);
        imageChanged(gnome_mdi_get_active_child(GNOME_MDI(mdi)), image);
        gtk_widget_queue_draw(gnome_mdi_get_active_view(GNOME_MDI(mdi)));
        gdk_window_set_cursor(gnome_mdi_get_active_view(GNOME_MDI(mdi))->window, NULL);
    }
}
static void cbImageRemoveBackground(GtkObject *object, gpointer data)
{
    struct ccd_image *image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(gnome_mdi_get_active_child(GNOME_MDI(mdi))));
    if (image)
    {
        gdk_window_set_cursor(gnome_mdi_get_active_view(GNOME_MDI(mdi))->window, cursorWait);
        gdk_flush();
        imageRemoveBackground(image);
        imageUpdate(image);
        imageChanged(gnome_mdi_get_active_child(GNOME_MDI(mdi)), image);
        gtk_widget_queue_draw(gnome_mdi_get_active_view(GNOME_MDI(mdi)));
        gdk_window_set_cursor(gnome_mdi_get_active_view(GNOME_MDI(mdi))->window, NULL);
    }
}
static void cbImageRemoveVBE(GtkObject *object, gpointer data)
{
    float             kernel[3];
    unsigned char    *pixels, *filt_pixels;
    struct ccd_image *image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(gnome_mdi_get_active_child(GNOME_MDI(mdi))));
    if (image)
    {
        gdk_window_set_cursor(gnome_mdi_get_active_view(GNOME_MDI(mdi))->window, cursorWait);
        gdk_flush();
        kernel[0]     =
        kernel[2]     = 1.0;
        kernel[1]     = 2.0;
        pixels        = image->pixels;
        filt_pixels   = ccd_image_convolve(image, NULL, 0, 1, (float *)kernel);
        image->pixels = filt_pixels;
        kernel[0]     =
        kernel[2]     = -1.0;
        kernel[1]     = 6.0;
        image->pixels = ccd_image_convolve(image, pixels, 0, 1, (float *)kernel);
        image->pixmin = image->pixmax = 0;
        free(filt_pixels);
        imageUpdate(image);
        imageChanged(gnome_mdi_get_active_child(GNOME_MDI(mdi)), image);
        gtk_widget_queue_draw(gnome_mdi_get_active_view(GNOME_MDI(mdi)));
        gdk_window_set_cursor(gnome_mdi_get_active_view(GNOME_MDI(mdi))->window, NULL);
    }
}
static void cbAbout(GtkObject *object, gpointer data)
{
    const gchar *authors[] = {"David Schmenk", NULL};
    GtkWidget   *about     = gnome_about_new(_("About GNOME CCD"), VERSION,
                                             "(C) 2001", authors,
                                             _("GNOME CCD is a CCD camera control and imaging "
                                               "program.  It is designed with astronimical applications "
                                               "in mind, but surely can be used elsewhere.\n"
                                               "http://home.earthlink.net/~dschmenk"), NULL);
    gtk_widget_show(about);
}

/***************************************************************************\
*                                                                           *
*                                Events                                     *
*                                                                           *
\***************************************************************************/

static void eventAppCreated(GnomeMDI *mdi, GnomeApp *app, gpointer data)
{
    GnomeUIInfo   *menu, *settings;
    GnomeDockItem *dockToolbar;
    menu     = gnome_mdi_get_menubar_info(app);
    settings = (GnomeUIInfo *)menu[1].moreinfo;
    GTK_CHECK_MENU_ITEM(settings[1].widget)->active = prefs.CloseWarning;
    prefs_set_menu_items(gnome_mdi_get_active_child(mdi));
    if ((dockToolbar = gnome_dock_get_item_by_name(GNOME_DOCK(app->dock), GNOME_APP_TOOLBAR_NAME, NULL, NULL, NULL, NULL)))
    {
        if (prefs.ViewToolbar)
            gtk_widget_show(GTK_WIDGET(dockToolbar));
        else
            gtk_widget_hide(GTK_WIDGET(dockToolbar));
        gtk_widget_queue_resize(GTK_WIDGET(app->dock));
    }
}
static gint eventChildRemove(GnomeMDI *mdi, GnomeMDIChild *child)
{
    GtkWidget        *messagebox;
    gint              bye   = TRUE;
    struct ccd_image *image = (struct ccd_image *)gtk_object_get_user_data(GTK_OBJECT(child));
    if (image->changed && prefs.CloseWarning)
    {
        messagebox = gnome_message_box_new(_("Image not saved"),
                                           GNOME_MESSAGE_BOX_WARNING,
                                           GNOME_STOCK_BUTTON_CLOSE, GNOME_STOCK_BUTTON_CANCEL, NULL);
        bye = (gnome_dialog_run(GNOME_DIALOG(messagebox)) == 0);
    }
    if (bye)
    {
        gdk_pixmap_unref(image->pixmap);
        ccd_image_delete(image);
        imageUpdateList();
    }
    return (bye);
}
static gint eventChildChange(GnomeMDI *mdi, GnomeMDIChild *old_child)
{
    prefs_set_menu_items(gnome_mdi_get_active_child(mdi));
    return (TRUE);
}
static gint eventDestroy(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    struct ccd_image *image;
    GtkWidget        *messagebox;
    gint              cancel = FALSE;

    /*
     * Make sure all the images have been saved.
     */
    for (image = ccd_image_first(); image; image = ccd_image_next(image))
    {
        if (image->changed && prefs.CloseWarning)
        {
            messagebox = gnome_message_box_new(_("Images not saved"),
                                               GNOME_MESSAGE_BOX_WARNING,
                                               GNOME_STOCK_BUTTON_CLOSE, GNOME_STOCK_BUTTON_CANCEL, NULL);
            cancel = (gnome_dialog_run(GNOME_DIALOG(messagebox)) == 1);
            break;
        }
    }
    if (!cancel)
       gtk_main_quit();
    return (cancel);
}

/***************************************************************************\
*                                                                           *
*                                 Main                                      *
*                                                                           *
\***************************************************************************/

int main(int argc, char **argv)
{
    poptContext ctx;
    int v = 0,
        g = 0;
    struct poptOption options[] =
    {
        {
            "verbose",
            'v',
            POPT_ARG_NONE,
            &v,
            0,
            N_("Output status bar text to stdout"),
            NULL
        },
        {
            "guide",
            'g',
            POPT_ARG_NONE,
            &g,
            0,
            N_("Output guiding errors to stdout"),
            NULL
        },
        {
            NULL,
            '\0',
            0,
            NULL,
            0,
            NULL,
            NULL
        }
    };
    bindtextdomain(PACKAGE, GNOMELOCALEDIR);
    textdomain(PACKAGE);
    gnome_init_with_popt_table(PACKAGE, VERSION, argc, argv, options, 0, &ctx);
    poptFreeContext(ctx);
    verbose =  (v ? 1 : 0) | (g ? 2 : 0);
    prefs_load();
    chdir(prefs.WorkingDirectory);
    getcwd(prefs.WorkingDirectory, DIR_STRING_LENGTH);
    create_view_palette();
    cursorWait = gdk_cursor_new(GDK_WATCH);
    mdi = gnome_mdi_new(PACKAGE, "CCD Camera Images");
    gtk_signal_connect(mdi, "app-created",  GTK_SIGNAL_FUNC(eventAppCreated),  NULL);
    gtk_signal_connect(mdi, "remove-child", GTK_SIGNAL_FUNC(eventChildRemove), NULL);
    gtk_signal_connect(mdi, "child-changed",GTK_SIGNAL_FUNC(eventChildChange), NULL);
    gtk_signal_connect(mdi, "destroy",      GTK_SIGNAL_FUNC(eventDestroy),     NULL);
    gnome_mdi_set_menubar_template(GNOME_MDI(mdi), menuMain);
    gnome_mdi_set_toolbar_template(GNOME_MDI(mdi), toolbarMain);
    gnome_mdi_set_child_list_path(GNOME_MDI(mdi), GNOME_MENU_WINDOWS_PATH);
    gnome_mdi_set_mode(GNOME_MDI(mdi), prefs.ViewMode);
    gnome_mdi_open_toplevel(GNOME_MDI(mdi));
    gtk_main();
    deactivateAcquireImage(FALSE);
    prefs_save();
    return (0);
}
