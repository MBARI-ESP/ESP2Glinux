
VERSION=1.0

# CC = gcc
# Set CC to your target's 'C' compiler to build for it
# Example:
#       make CC=/arm/bin/gcc clean tcflush
DEBUG=
CPPFLAGS=-DVERSION_STR=\"$(VERSION)\" -DUUCP_LOCK_DIR=\"/var/lock\"
CFLAGS = -Wall -Os $(DEBUG)

# LD = gcc
LDFLAGS = $(DEBUG)
LDLIBS =

tcflush : tcflush.o
#	$(LD) $(LDFLAGS) -o $@ $+ $(LDLIBS)

tcflush.o : tcflush.c

clean:
	rm -f tcflush.o
	rm -f *~
	rm -f \#*\#

distclean: clean
	rm -f tcflush
