#Make a standalone OpenWRT usbmode utility 
#usbmode is OpenWRT's much trimmer alternative to usb_modeswitch
#  revised:  6/2/18 brent@mbari.org

#assumes that following OpenWRT project's have been cloned under ~/git
#  usbmode
#  libubox
#  json-c

# json-c must have been configured for the desired target
#  export HOST=arm-none-linux-gnueabi
#  pushd ~/git/json-c
#  configure --HOST=$HOST CFLAGS="-Os -g" ac_cv_func_uselocale=no
#  make clean
#  make
#  popd
#  make

gitTop = $(HOME)/git
usbmodeTop = $(gitTop)/usbmode
libuboxTop = $(gitTop)/libubox
jsonTop = $(gitTop)/json-c
modeSwitchData = ../usb_modeswitch/usb-modeswitch-data-20170806/usb_modeswitch.d

HOST ?= arm-none-linux-gnueabi
CC ?= $(HOST)-gcc
STRIP ?= $(HOST)-strip
CFLAGS ?= -Os -g
rootfs ?= $(HOME)/ltib/rootfs
inc ?= $(rootfs)/usr/include
LDFLAGS ?= -L$(rootfs)/usr/lib


usbmode: usbmode.unstripped
	$(STRIP) -o $@ $<

usbmode.unstripped:  Makefile avl.o avl-cmp.o blob.o blobmsg.o blobmsg_json.o \
	  $(jsonTop)/.libs/libjson-c.a $(usbmodeTop)/*.h $(usbmodeTop)/*.c 
	$(CC) $(CFLAGS) -o $@ \
          -I$(usbmodeTop) -I$(libuboxTop) \
          -I$(gitTop) -isystem $(inc)/libusb-1.0 \
          avl.o avl-cmp.o blob.o blobmsg.o blobmsg_json.o \
          $(jsonTop)/.libs/libjson-c.a $(LDFLAGS) -lusb-1.0 \
          $(usbmodeTop)/switch.c $(usbmodeTop)/main.c

avl.o: $(libuboxTop)/avl.h $(libuboxTop)/list.h $(libuboxTop)/avl.c
	$(CC) $(CFLAGS) -I$(libuboxTop) -o $@ -c $(libuboxTop)/avl.c

avl-cmp.o: $(libuboxTop)/avl-cmp.h $(libuboxTop)/blob.h $(libuboxTop)/avl-cmp.c
	$(CC) $(CFLAGS) -I$(libuboxTop) -o $@ -c $(libuboxTop)/avl-cmp.c

blob.o: $(libuboxTop)/blob.h $(libuboxTop)/blob.c
	$(CC) $(CFLAGS) -I$(libuboxTop) -o $@ -c $(libuboxTop)/blob.c

blobmsg.o: $(libuboxTop)/blobmsg.h json/json.h $(libuboxTop)/blobmsg.c
	$(CC) $(CFLAGS) -I$(libuboxTop) -o $@ -c $(libuboxTop)/blobmsg.c

blobmsg_json.o: $(libuboxTop)/blobmsg.h $(libuboxTop)/blobmsg_json.h
	$(CC) $(CFLAGS) -I$(libuboxTop) -I. \
          -o $@ -c $(libuboxTop)/blobmsg_json.c

json/json.h:
	ln -s $(jsonTop) json

clean:
	rm -f *.o usbmode json

#derive the database from that maintained by the usb_modeswitch project
usb-mode.json:  $(modeSwitchData)
	convert-modeswitch.pl $</* >usb-mode.json

$(libuboxTop)/%.h $(libuboxTop)/%.c:
	cd $(gitTop); git clone https://git.openwrt.org/project/libubox.git
	cd $(libuboxTop); git checkout 6eff829d788b36939325557066f58aafd6a05321
