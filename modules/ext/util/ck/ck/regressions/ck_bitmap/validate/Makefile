.PHONY: check clean

OBJECTS=serial

all: $(OBJECTS)

serial: serial.cc ../../../include/ck_bitmap.h
	$(CC) $(CFLAGS) -o serial serial.cc

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM *.exe

check: all
	./serial

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE -Wno-long-long
