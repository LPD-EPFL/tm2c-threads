ifeq ($(P),1)
DEBUG_CFLAGS=-Wall
PERF_CLFAGS=
else
DEBUG_CFLAGS=-ggdb -Wall -g  -fno-inline #-pg
PERF_CLFAGS=
endif

ifeq ($(V),2) 
VER_FLAGS=-DV2
else 
VER_FLAGS=-DV1
endif

default: main

main:	ssmp.a main.o common.h
	gcc $(VER_FLAGS) -o main main.o ssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS) 

main.o:	main.c
	gcc $(VER_FLAGS) -D_GNU_SOURCE -c main.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

ssmp.o: ssmp.c
	gcc $(VER_FLAGS) -D_GNU_SOURCE -c ssmp.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

ssmp_send.o: ssmp_send.c
	gcc $(VER_FLAGS) -D_GNU_SOURCE -c ssmp_send.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

ssmp_recv.o: ssmp_send.c
	gcc $(VER_FLAGS) -D_GNU_SOURCE -c ssmp_recv.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

ssmp.a: ssmp.o ssmp_send.o ssmp_recv.o ssmp.h
	@echo Archive name = ssmp.a
	ar -r ssmp.a ssmp.o ssmp_send.o ssmp_recv.o
	rm -f *.o	

one2one: ssmp.a one2one.o common.h
	gcc $(VER_FLAGS) -o one2one one2one.o ssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)	

one2one.o:	one2one.c ssmp.c
		gcc $(VER_FLAGS) -D_GNU_SOURCE -c one2one.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

barrier: ssmp.a barrier.o common.h
	gcc $(VER_FLAGS) -o barrier barrier.o ssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)	

barrier.o:	barrier.c ssmp.c
		gcc $(VER_FLAGS) -D_GNU_SOURCE -c barrier.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

color_buf: ssmp.a color_buf.o common.h
	gcc $(VER_FLAGS) -o color_buf color_buf.o ssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)	

color_buf.o:	color_buf.c ssmp.c
		gcc $(VER_FLAGS) -D_GNU_SOURCE -c color_buf.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)



clean:
	rm -f *.o *.a