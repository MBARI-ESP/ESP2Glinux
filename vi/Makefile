
# CC = gcc
# Set CC to your target's 'C' compiler to build for it
# Example:
#       make CC=/arm/bin/gcc clean vi
#DEBUG = -DCONFIG_FEATURE_VI_CRASHME
CPPFLAGS = -DSTANDALONE $(DEBUG)
CFLAGS = -Wall -Os -s

# LD = gcc
LDFLAGS = $(DEBUG)
LDLIBS =

all:	vi arm/vi ltib/vi

vi : vi.c Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) vi.c -o vi

ltib/vi:  Makefile vi.c
	mkdir -p ltib && /ltibarm/bin/gcc $(CPPFLAGS) $(CFLAGS) vi.c -o $@

arm/vi:  Makefile vi.c
	mkdir -p arm && /arm/bin/gcc $(CPPFLAGS) $(CFLAGS) vi.c -o $@

clean:
	rm -f vi ltib/vi arm/vi

distclean: clean

realclean: distclean
