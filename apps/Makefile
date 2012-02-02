PLATFORM = iRCCE

include ../Makefile.common

sinclude ../Makefile.$(PLATFORM)

INCLUDES += -I$(MAININCLUDE)

.PHONY: all default clean 

default:
	@echo "Platform: $(PLATFORM)"
	@echo "Usage: make all       "
	@echo "       make clean     "

PROGRAMS = tm1 tm2 tm3 tm4 tm5 tm6 tm7 tm8 tm9 tm10 \
		   toall all2all \
		   pingpong async_pingpong \
		   ring ring_token ring_simple \
		   pthread \
		   bm_rams global_clock \
		   madmonkey spam

# programs that need tasklib in order to compile
TASKLIB_PROGRAMS = ps_hashtable \
				   shmem_map

# this is a list of programs that do not work, for whatever reason
NON_WORKING = \
			  RCCE_locks \
			  ps_hashtable_ex \
			  simple \
			  recv_all \
			  tasklib tasklib_test \
			  recv_all recv_all_p recv_all_i

ALL_PROGRAMS = $(PROGRAMS) $(TASKLIB_PROGRAMS)
ALL_OBJS = $(addsuffix .o,$(ALL_PROGRAMS))
ALL_SRCS = $(addsuffix .c,$(ALL_PROGRAMS))

LIBS += -lpthread

all: $(ALL_PROGRAMS)

%.o: %.c
	$(C) $(CFLAGS) -o $@ -c $<

%: %.c
	$(C) $(CFLAGS) $(LDFLAGS) -o $@ $< $(DSTM_ARCHIVE) $(LIBS)

$(TASKLIB_PROGRAMS):
	$(C) $(CFLAGS) $(LDFLAGS) -o $@ $(addsuffix .c,$@) $(DSTM_ARCHIVE) $(LIBS) -ltask

$(ALL_PROGRAMS): $(ALL_SRCS) $(DSTM_ARCHIVE)

clean:
	@rm -f *.o $(ALL_OBJS)
	@rm -f $(ALL_PROGRAMS)
	@rm -rf $(addsuffix .dSYM, $(ALL_PROGRAMS))

