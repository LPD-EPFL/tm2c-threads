ifeq ($(P),0) #give P=0 to compile with debug info
DEBUG_CFLAGS=-ggdb -Wall -g  -fno-inline #-pg
PERF_CLFAGS=
else
DEBUG_CFLAGS=-Wall
PERF_CLFAGS=
endif


default: main

main:	libssmp.a main.o common.h
	gcc $(VER_FLAGS) -o main main.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS) 

main.o:	main.c
	gcc $(VER_FLAGS) -D_GNU_SOURCE -c main.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

ssmp.o: ssmp.c
	gcc $(VER_FLAGS) -D_GNU_SOURCE -c ssmp.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

ssmp_send.o: ssmp_send.c
	gcc $(VER_FLAGS) -D_GNU_SOURCE -c ssmp_send.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

ssmp_recv.o: ssmp_send.c
	gcc $(VER_FLAGS) -D_GNU_SOURCE -c ssmp_recv.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

ssmp_broadcast.o: ssmp_broadcast.c
	gcc $(VER_FLAGS) -D_GNU_SOURCE -c ssmp_broadcast.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

libssmp.a: ssmp.o ssmp_send.o ssmp_recv.o ssmp_broadcast.o ssmp.h
	@echo Archive name = libssmp.a
	ar -r libssmp.a ssmp.o ssmp_send.o ssmp_recv.o ssmp_broadcast.o
	rm -f *.o	

one2one: libssmp.a one2one.o common.h
	gcc $(VER_FLAGS) -o one2one one2one.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)	

one2one.o:	one2one.c ssmp.c
		gcc $(VER_FLAGS) -D_GNU_SOURCE -c one2one.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

barrier: libssmp.a barrier.o common.h
	gcc $(VER_FLAGS) -o barrier barrier.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)	

barrier.o:	barrier.c ssmp.c
		gcc $(VER_FLAGS) -D_GNU_SOURCE -c barrier.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)

color_buf: libssmp.a color_buf.o common.h
	gcc $(VER_FLAGS) -o color_buf color_buf.o libssmp.a -lrt $(DEBUG_CFLAGS) $(PERF_CLFAGS)	

color_buf.o:	color_buf.c ssmp.c
		gcc $(VER_FLAGS) -D_GNU_SOURCE -c color_buf.c $(DEBUG_CFLAGS) $(PERF_CLFAGS)



clean:
	rm -f *.o *.a