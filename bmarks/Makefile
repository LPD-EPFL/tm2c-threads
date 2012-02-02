PLATFORM = iRCCE

include ../Makefile.common

sinclude ../Makefile.$(PLATFORM)

INCLUDES += -I$(MAININCLUDE)

.PHONY: all default clean

default:
	@echo "Platform: $(PLATFORM)"
	@echo "Usage: make all       "
	@echo "       make clean     "

MB_LL := microbench/linkedlist
MB_LLPGAS := microbench/linkedlistpgas
MB_HT := microbench/hashtable
MB_HTPGAS := microbench/hashtablepgas
MR := mapreduce

# harris is not used here
LLFILES = linkedlist test #harris
HTFILES = hashtable intset test

ALL_FILES = $(BMARKS) \
			$(MR)/mr \
			$(addprefix $(MB_LL)/,$(LLFILES)) \
			$(addprefix $(MB_HT)/,$(HTFILES))
	
# if there is no PGAS
ifneq (,$(findstring PGAS,$(CFLAGS)))
ALL_FILES += \
			$(addprefix $(MB_LLPGAS)/,$(LLFILES)) \
			$(addprefix $(MB_HTPGAS)/,$(HTFILES))
endif

ALL_SRCS = $(addsuffix .c,$(ALL_FILES))
ALL_OBJS = $(addsuffix .o,$(ALL_FILES)) 

BMARKS = bank bankseq \
		 mtest \
		 readonly

# this is a list of programs that do not work, for whatever reason
NON_WORKING = 

LIBS += -lpthread -lm

ALL_PROGRAMS = $(BMARKS) mbll mbht mr

ifneq (,$(findstring PGAS,$(CFLAGS)))
BMARKS       += bankpgas
ALL_PROGRAMS += mbllpgas mbhtpgas
endif

all: $(ALL_PROGRAMS)

mbll: $(filter $(MB_LL)/%,$(ALL_OBJS)) $(DSTM_ARCHIVE)
	$(C) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LIBS)

mbht: $(filter $(MB_HT)/%,$(ALL_OBJS)) $(MB_LL)/linkedlist.o $(DSTM_ARCHIVE)
	$(C) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LIBS)

mbllpgas: $(filter $(MB_LLPGAS)/%,$(ALL_OBJS)) $(DSTM_ARCHIVE)
	$(C) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LIBS)

mbhtpgas: $(filter $(MB_HTPGAS)/%,$(ALL_OBJS)) $(MB_LLPGAS)/linkedlist.o $(DSTM_ARCHIVE)
	$(C) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LIBS)

mr: $(filter $(MR)/%,$(ALL_OBJS)) $(DSTM_ARCHIVE)
	$(C) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LIBS)

%: %.o
	$(C) $(CFLAGS) $(LDFLAGS) -o $@ $< $(DSTM_ARCHIVE) $(LIBS)

%.o: %.c
	$(C) $(CFLAGS) -o $@ -c $<

$(ALL_OBJS): $(ALL_SRCS)

clean:
	@rm -f *.o $(ALL_OBJS)
	@rm -f $(ALL_PROGRAMS)
	@rm -rf $(addsuffix .dSYM, $(ALL_PROGRAMS))

