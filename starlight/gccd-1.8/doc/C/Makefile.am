gccd_helpdir = $(datadir)/gnome/help/gccd/C

gccd_help_DATA =         \
        gccd.html        \
        topic.dat

SGML_FILES =                    \
        gccd.sgml

# files that aren't in a binary/data/library target have to be listed here
# to be included in the tarball when you 'make dist'
EXTRA_DIST = \
        topic.dat               \
        $(SGML_FILES)


## The - before the command means to ignore it if it fails.  that way
## people can still build the software without the docbook tools

all: 

gccd.html: gccd/gccd.html
	-cp gccd/gccd.html .

gccd/gccd.html: $(SGML_FILES)
	-db2html gccd.sgml

## when we make dist, we include the generated HTML so people don't
## have to have the docbook tools
dist-hook:
	mkdir $(distdir)/gccd
	-cp gccd/*.html gccd/*.css $(distdir)/gccd
	-cp gccd.html $(distdir)

install-data-local: gccd.html
	$(mkinstalldirs) $(gccd_helpdir)/images
	-for file in $(srcdir)/gccd/*.html $(srcdir)/gccd/*.css; do \
	basefile=`basename $$file`; \
	$(INSTALL_DATA) $(srcdir)/$$file $(gccd_helpdir)/$$basefile; \
	done

gccd.ps: gccd.sgml
	-db2ps $<

gccd.rtf: gccd.sgml       
	-db2rtf $<

