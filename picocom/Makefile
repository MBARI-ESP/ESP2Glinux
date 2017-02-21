
VERSION=1.6-mbari

# CC = gcc
# Set CC to your target's 'C' compiler to build for it
# Example:
#       make CC=/arm/bin/gcc clean picocom
DEBUG=
CPPFLAGS=-DVERSION_STR=\"$(VERSION)\" -DUUCP_LOCK_DIR=\"/var/lock\"
CFLAGS = -Wall -Os $(DEBUG)

# LD = gcc
LDFLAGS = $(DEBUG)
LDLIBS =

picocom : picocom.o term.o
#	$(LD) $(LDFLAGS) -o $@ $+ $(LDLIBS)

picocom.o : picocom.c term.h
term.o : term.c term.h

doc : picocom.8 picocom.8.html picocom.8.ps

changes :
	svn log -v . > CHANGES

picocom.8 : picocom.8.xml
	xmlmp2man < $< > $@

picocom.8.html : picocom.8.xml
	xmlmp2html < $< > $@

picocom.8.ps : picocom.8
	groff -mandoc -Tps $< > $@

clean:
	rm -f picocom.o term.o
	rm -f *~
	rm -f \#*\#

distclean: clean
	rm -f picocom

realclean: distclean
	rm -f picocom.8
	rm -f picocom.8.html
	rm -f picocom.8.ps
	rm -f CHANGES
