VERSION_MAJOR =1
VERSION_MINOR =8
KERNEL        = /usr/src/linux
CFLAGS        =-I$(KERNEL)/include -include $(KERNEL)/include/linux/modversions.h -D__KERNEL__ -DMODULE -DVERSION_MAJOR=$(VERSION_MAJOR) -DVERSION_MINOR=$(VERSION_MINOR) -O2 -Wall
MISCDIR       =/lib/modules/$(shell uname -r)/misc
CC            =gcc  #-2.95

all: ccd.o sx_parport.o sx_usb.o audinegccd.o cookbook.o qc.o tester.o

clean:
	-rm *.o

install:
	rm -f /dev/ccd*
	mknod /dev/ccda  c 127 0
	mknod /dev/ccda1 c 127 16
	mknod /dev/ccda2 c 127 32
	mknod /dev/ccdA  c 127 128
	mknod /dev/ccdA1 c 127 144
	mknod /dev/ccdA2 c 127 160
	mknod /dev/ccdb  c 127 1
	mknod /dev/ccdb1 c 127 17
	mknod /dev/ccdb2 c 127 33
	mknod /dev/ccdB  c 127 129
	mknod /dev/ccdB1 c 127 145
	mknod /dev/ccdB2 c 127 161
	chmod 0666 /dev/ccda
	chmod 0666 /dev/ccda1
	chmod 0666 /dev/ccda2
	chmod 0666 /dev/ccdA
	chmod 0666 /dev/ccdA1
	chmod 0666 /dev/ccdA2
	chmod 0666 /dev/ccdb
	chmod 0666 /dev/ccdb1
	chmod 0666 /dev/ccdb2
	chmod 0666 /dev/ccdB
	chmod 0666 /dev/ccdB1
	chmod 0666 /dev/ccdB2
#
# If you have made ther serial port mod for the SX USB module or have the
# FX camera, uncomment the following to gain access to the ports.
#	
#	rm -f /dev/ttysx*
#	mknod /dev/ttysx0  c 4 128
#	mknod /dev/ttysx1  c 4 129
#	mknod /dev/ttysx2  c 4 130
#	mknod /dev/ttysx3  c 4 131
#	chmod 0666 /dev/ttysx0
#	chmod 0666 /dev/ttysx1
#	chmod 0666 /dev/ttysx2
#	chmod 0666 /dev/ttysx3
	if [ ! -d $(MISCDIR) ]; then mkdir $(MISCDIR); fi;
	cp *.o /lib/modules/`uname -r`/misc
	depmod -a
	modprobe ccd
#
# Uncomment whichever camera(s) you want installed.
#	
	modprobe sx_usb
#	modprobe sx_parport
#	modprobe audinegccd
#	modprobe cookbook
#	modprobe qc

dist:
	$(MAKE) -C ezusb
	mkdir ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)
	cp *.c ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)
	cp *.h ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)
	cp makefile ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)
	mkdir ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)/samples
	cp samples/*.c ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)/samples
	cp samples/*.h ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)/samples
	cp samples/*.pm ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)/samples
	cp samples/README.* ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)/samples
	cp samples/Makefile ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)/samples
	tar czf ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR).tgz ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)
	rm -rf ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)
	
dist-ezusb:
	$(MAKE) -C ezusb
	mkdir ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)
	cp *.c ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)
	cp *.h ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)
	cp makefile ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)
	mkdir ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)/samples
	cp samples/*.c ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)/samples
	cp samples/*.h ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)/samples
	cp samples/*.pm ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)/samples
	cp samples/README.* ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)/samples
	cp samples/Makefile ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)/samples
	$(MAKE) -C ezusb clean
	mkdir ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)/ezusb
	cp ezusb/makefile ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)/ezusb
	cp ezusb/*.c ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)/ezusb
	cp ezusb/*.h ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)/ezusb
	tar czf ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR).tgz ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)
	rm -rf ccd_kernel-$(VERSION_MAJOR).$(VERSION_MINOR)
	
ccd.o : ccd.c ccd.h ccd_msg.h

qc.o : qc.c ccd.h ccd_msg.h

cookbook.o : cookbook.c cookbook.h ccd.h ccd_msg.h

tester.o : tester.c ccd.h ccd_msg.h

sx_parport.o : sx_parport.c ccd.h ccd_msg.h

sx_usb.o : sx_usb.c sx_usb.h ccd.h ccd_msg.h sx_ezusb_code.h sx_ezusb2_code.h
	@if [ -d ezusb ]; then $(MAKE) -C ezusb; fi;
	$(CC) $(CFLAGS) -c $< -o $@

audinegccd.o :	audinegccd.c audine_io.c ccd.h ccd_msg.h

