#
# Makefile for liblabjackusb
#
#

TARGET = liblabjackusb.so.2.0.4
DESTINATION = $(DEV_IMAGE)/usr/local/lib

HEADER = labjackusb.h
HEADER_DESTINATION = $(DEV_IMAGE)/usr/local/include

CFLAGS += -fPIC -g -Wall
LIBFLAGS = -lusb-1.0 -lc

%.o: %.c
	$(CC) $(CFLAGS) -c $<

all: $(TARGET)

$(TARGET): labjackusb.o $(HEADER)
	$(CC) -shared -Wl,-soname,liblabjackusb.so -o $(TARGET) labjackusb.o $(LIBFLAGS)

install: $(TARGET)
	test -z $(DESTINATION) || mkdir -p $(DESTINATION)
	install $(TARGET) $(DESTINATION)
	test -z $(HEADER_DESTINATION) || mkdir -p $(HEADER_DESTINATION)
	install $(HEADER) $(HEADER_DESTINATION)
	ldconfig $(DESTINATION)

clean:
	rm -f $(TARGET) *.o *~
