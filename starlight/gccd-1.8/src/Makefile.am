
INCLUDES =                                                               \
	-I$(top_srcdir)                                                  \
	-I$(includedir)                                                  \
	-I$(top_srcdir)/intl                                             \
	$(GNOME_INCLUDEDIR)                                              \
	$(GDK_PIXBUF_CFLAGS)                                             \
	-DG_LOG_DOMAIN=\"GCCD\"                                          \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"

bin_PROGRAMS = gccd

gccd_SOURCES =                                                           \
	gccd.h		                                                 \
	ccd_msg.h                                                        \
	gccd.c								 \
	image_obj.c							 \
	ccd_gui.c							 \
	dev_ctl.c

gccd_LDADD =                                                             \
	$(GNOMEUI_LIBS)                                                  \
	$(GNOME_LIBDIR)                                                  \
	$(GDK_PIXBUF_LIBS)                                               \
	$(INTLLIBS)
