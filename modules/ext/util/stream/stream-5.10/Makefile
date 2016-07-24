CC = gcc
CFLAGS = -O2
RW_CFLAGS = -O -fopenmp -DSTREAM_ARRAY_SIZE=4000000 -DNTIMES=10000

FF = g77
FFLAGS = -O2

all: stream_f.exe stream_c.exe

stream_f.exe: stream.f mysecond.o
	$(CC) $(CFLAGS) -c mysecond.c
	$(FF) $(FFLAGS) -c stream.f
	$(FF) $(FFLAGS) stream.o mysecond.o -o stream_f.exe

stream_c.exe: stream.c
	$(CC) $(CFLAGS) stream.c -o stream_c.exe

rw_stream: stream.c
	export MP_NUM_THREADS=10
	$(CC) $(RW_CFLAGS) stream.c -o rw_stream

clean:
	rm -f stream_f.exe stream_c.exe rw_stream *.o


