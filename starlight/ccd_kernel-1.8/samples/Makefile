.PHONY:	all clean

INCDIR=	     -I..
CFLAGS=      -O2 $(INCDIR)
LIBS=        -lvga -lm

all:	testguide tdi tdi_cal

clean:
	rm -f testguide tdi tdi_cal
	rm -f *.o

testguide: testguide.o ui.o
	$(CC) $(CFLAGS) testguide.o ui.o $(LIBS) -o testguide

tdi: tdi.o tdi_util.o
	$(CC) $(CFLAGS) tdi.o tdi_util.o $(LIBS) -o tdi

tdi_cal: tdi_cal.o tdi_util.o
	$(CC) $(CFLAGS) tdi_cal.o tdi_util.o $(LIBS) -o tdi_cal

