# Main Makefile for DSTM

# For platform, choose one out of: iRCCE, SCC.SSMP, MCORE, MCORE.SSMP,CLUSTER,TILERA
PLATFORM = MCORE.SSMP
# USE_HASHTABLE_KHASH:  khash.h from <http://www.freewebs.com/attractivechaos/khash.h>
# USE_HASHTABLE_UTHASH: uthash.h from <http://uthash.sourceforge.net/>
# USE_HASHTABLE_SDD:   Sunrise Data Dictionary <>
# USE_HASHTABLE_VT:     hashtable by Vasilis
# USE_HASHTABLE_SSHT:     Super-Simple-HT (counting readers)
# USE_FIXED_HASH:    fixed hash
# USE_ARRAY:     array
HASHTABLE = USE_HASHTABLE_SSHT
.PHONY: all 

all:

TOP := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

SRCPATH := $(TOP)/src
MAININCLUDE := $(TOP)/include

INCLUDES := -I$(MAININCLUDE) -I$(TOP)/external/include
LIBS := -L$(TOP)/external/lib \
		-lm \
		-lconfig \
		$(PLATFORM_LIBS)

# EXTRA_DEFINES are passed through the command line
DEFINES := $(PLATFORM_DEFINES) $(EXTRA_DEFINES)

DEBUG_FLAGS := #-g -ggdb -fno-inline#-DDEBUG 

## Archive ##
ARCHIVE_SRCS_PURE:= pubSubTM.c tm.c log.c lock_log.c array_log.c dslock.c \
			measurements.c pgas.c config.c fakemem.c

# Include the platform specific Makefile
# This file has the modifications related to the current platform
-include Makefile.$(PLATFORM)

## Apps ##
APPS_DIR := apps
# add the non PGAS applications only if PGAS is not defined
ifeq (,$(findstring DPGAS,$(PLATFORM_DEFINES)))
APPS = #tm1 tm2 tm3 tm4 tm5 tm6 tm7 tm8 tm9 tm10
endif

# apps that need tasklib in order to compile
APPS_TASKLIB = ps_hashtable \
			   shmem_map

# this is a list of apps that do not work, for whatever reason
APPS_NON_WORKING = \
				   RCCE_locks \
				   ps_hashtable_ex \
				   simple \
				   tasklib tasklib_test \
				   recv_all recv_all_p recv_all_i \
				   pthread \
				   bm_rams global_clock \
				   madmonkey spam

## Benchmarks ##
BMARKS_DIR := bmarks

MB_LL := microbench/linkedlist
MB_LLPGAS := microbench/linkedlistpgas
MB_HT := microbench/hashtable
MB_HTPGAS := microbench/hashtablepgas
MR := mapreduce

# harris is not used here
LLFILES = linkedlist test #harris
HTFILES = hashtable intset test

# add the non PGAS applications only if PGAS is not defined
ifneq (,$(findstring UPGAS,$(PLATFORM_DEFINES)))
BMARKS = bank #bankseq \
	#	 readonly

# this is a list of programs that do not work, for whatever reason
BMARKS_NON_WORKING = mtest

# all bmarks that need to be built
ALL_BMARKS = $(BMARKS) mbll mbht mr mp
endif 

## The rest of the Makefile ##


ifneq (,$(findstring DPGAS,$(PLATFORM_DEFINES)))
ALL_BMARKS += bankpgas mbllpgas mbhtpgas mp
endif

# define the compiler now, if platform makefile exported something through
# CCOMPILE
ifneq (,$(CCOMPILE))
C := $(CCOMPILE)
else
C := gcc
endif

CDEP := $(C)
AR := ar
LD := ld

DSTM_ARCHIVE:= $(TOP)/dtm.a

### Pretty output control ###
# Set up compiler and linker commands that either is quiet (does not print 
# the command line being executed) or verbose (print the command line). 
_C := $(C)
_CDEP := $(CDEP)
_LD := $(LD)
_AR := $(AR)

ifeq ($(V),1)
 override C = $(_C)
 override LD = $(_LD)
 override CDEP = $(_CDEP)
 override AR = $(_AR)
else
 override C    = @echo "    COMPILE         $<"; $(_C)
 override LD   = @echo "    LINKING         $@"; $(_LD)
 override CDEP = @echo "    DEPGEN          $@"; $(_CDEP)
 override AR   = @echo "    ARCHIVE         $@ ($^)"; $(_AR)
endif

# dependency tracking stuff, works with gcc / does not work with tile-gcc
ifeq ($(PLATFORM),CLUSTER)
EXTRA_CFLAGS = -MMD -MG
endif


EXTRA_CFLAGS += -D$(HASHTABLE)

LDFLAGS := $(LDFLAGS) $(PLATFORM_LDFLAGS)
LIBS    := $(LIBS) $(PLATFORM_LIBS)
CFLAGS  := $(CFLAGS) $(DEFINES) $(PLATFORM_DEFINES) \
		   $(PLATFORM_CFLAGS) \
		   $(DEBUG_FLAGS) $(INCLUDES) $(EXTRA_CFLAGS)

.PHONY: all libs clean install dist

all: archive $(HELPER_APPS) applications benchmarks

## Archive specific stuff ##
ARCHIVE_SRCS := $(addprefix $(SRCPATH)/, $(ARCHIVE_SRCS_PURE))
ARCHIVE_OBJS = $(ARCHIVE_SRCS:.c=.o)
ARCHIVE_DEPS = $(ARCHIVE_SRCS:.c=.d)

$(SRCPATH)/%.o:: $(SRCPATH)/%.c
	$(C) $(CFLAGS) -o $@ -c $<

# We do this only on iRCCE
ifeq ($(PLATFORM),iRCCE)
$(ARCHIVE):
	@(cd $(RCCEPATH) ; make)
endif

# We do this only on the SCC with ssmp
ifeq ($(PLATFORM),SCC.SSMP)
$(ARCHIVE):
	@(cd $(RCCEPATH) ; make API=gory)
endif


$(DSTM_ARCHIVE): $(ARCHIVE) $(ARCHIVE_OBJS)
	@echo Archive name = $(DSTM_ARCHIVE) 
ifeq ($(PLATFORM),iRCCE)
	@rm -rf .ar-lib
	@mkdir .ar-lib
	@(cd .ar-lib && ar x $(ARCHIVE))
	@$(AR) cr $(DSTM_ARCHIVE) $(ARCHIVE_OBJS) .ar-lib/*.o
	@rm -rf .ar-lib
else
ifeq ($(PLATFORM),SCC.SSMP)
	@rm -rf .ar-lib
	@mkdir .ar-lib
	@(cd .ar-lib && ar x $(ARCHIVE))
	@$(AR) cr $(DSTM_ARCHIVE) $(ARCHIVE_OBJS) .ar-lib/*.o
	@rm -rf .ar-lib
else
	$(AR) cr $(DSTM_ARCHIVE) $(ARCHIVE_OBJS)
endif
endif
	ranlib $(DSTM_ARCHIVE)

archive: $(DSTM_ARCHIVE)

## Apps specific stuff ##

ifeq ($(HAS_TASKLIB),yes)
APPS += $(APPS_TASKLIB)
endif

APPS := $(addprefix $(APPS_DIR)/,$(APPS))
APP_OBJS = $(addsuffix .o,$(APPS))
APP_SRCS = $(addsuffix .c,$(APPS))
APP_DEPS = $(addsuffix .d,$(APPS))

$(APPS): $(DSTM_ARCHIVE) $(APP_SRCS)

# an exception
$(APPS_DIR)/pthread: $(APPS_DIR)/pthread.c
	$(C) $(CFLAGS) $(LDFLAGS) -o $@ $< $(DSTM_ARCHIVE) $(LIBS) -lpthread

$(APPS_DIR)/%: $(APPS_DIR)/%.c
	$(C) $(CFLAGS) $(LDFLAGS) -o $@ $< $(DSTM_ARCHIVE) $(LIBS)

$(TASKLIB_PROGRAMS):
	$(C) $(CFLAGS) $(LDFLAGS) -o $@ $(addsuffix .c,$@) $(DSTM_ARCHIVE) $(LIBS) -ltask

applications: $(DSTM_ARCHIVE) $(APPS)

## Benchmarks specific stuff ##
ALL_BMARK_FILES = $(BMARKS) \
				  $(MR)/mr \
				  $(addprefix $(MB_LL)/,$(LLFILES)) \
				  $(addprefix $(MB_HT)/,$(HTFILES))

# if there is no PGAS
ifneq (,$(findstring DPGAS,$(CFLAGS)))
ALL_BMARK_FILES += \
					$(addprefix $(MB_LLPGAS)/,$(LLFILES)) \
					$(addprefix $(MB_HTPGAS)/,$(HTFILES))
endif

ALL_BMARK_FILES := $(addprefix $(BMARKS_DIR)/, $(ALL_BMARK_FILES))
BMARKS_OBJS = $(addsuffix .o,$(ALL_BMARK_FILES)) 
BMARKS_SRCS = $(addsuffix .c,$(ALL_BMARK_FILES))
BMARKS_DEPS = $(addsuffix .d,$(ALL_BMARK_FILES))

# set the proper directory
ALL_BMARKS := $(addprefix $(BMARKS_DIR)/, $(ALL_BMARKS))
BMARKS_DEPS += $(addsuffix .d,$(ALL_BMARKS))

$(ALL_BMARKS): $(DSTM_ARCHIVE)

$(BMARKS_DIR)/%.o:: $(BMARKS_DIR)/%.c
	$(C) $(CFLAGS) -o $@ -c $<

$(BMARKS_DIR)/%: $(BMARKS_DIR)/%.c
	$(C) $(CFLAGS) $(LDFLAGS) -o $@ $< $(DSTM_ARCHIVE) $(LIBS)

$(BMARKS_DIR)/mbll: $(filter $(BMARKS_DIR)/$(MB_LL)/%,$(BMARKS_OBJS)) $(DSTM_ARCHIVE)
	$(C) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LIBS)

$(BMARKS_DIR)/mbht: $(filter $(BMARKS_DIR)/$(MB_HT)/%,$(BMARKS_OBJS)) $(BMARKS_DIR)/$(MB_LL)/linkedlist.o $(DSTM_ARCHIVE)
	$(C) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LIBS)

$(BMARKS_DIR)/mbllpgas: $(filter $(BMARKS_DIR)/$(MB_LLPGAS)/%,$(BMARKS_OBJS)) $(DSTM_ARCHIVE)
	$(C) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LIBS)

$(BMARKS_DIR)/mbhtpgas: $(filter $(BMARKS_DIR)/$(MB_HTPGAS)/%,$(BMARKS_OBJS)) $(BMARKS_DIR)/$(MB_LLPGAS)/linkedlist.o $(DSTM_ARCHIVE)
	$(C) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LIBS)

$(BMARKS_DIR)/mr: $(filter $(BMARKS_DIR)/$(MR)/%,$(BMARKS_OBJS)) $(DSTM_ARCHIVE)
	$(C) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LIBS)

benchmarks: $(ALL_BMARKS)

clean_archive:
	@echo "Cleaning archives (libraries)..."
	rm -f $(ARCHIVE_OBJS)
	rm -f $(DSTM_ARCHIVE)

clean_apps:
	@echo "Cleaning apps dir..."
	rm -f $(APP_OBJS) $(APPS)
	rm -rf $(addsuffix .dSYM, $(HELPER_APPS))
	rm -rf $(addsuffix .dSYM, $(APPS))
	rm -f $(HELPER_APPS)

clean_bmarks:
	@echo "Cleaning benchmarks dir..."
	rm -f $(BMARKS_OBJS) $(ALL_BMARKS)
	rm -rf $(addsuffix .dSYM, $(ALL_BMARKS))

clean: clean_archive clean_apps clean_bmarks

realclean: clean
	rm -f $(ARCHIVE_DEPS)
	rm -f $(APP_DEPS)
	rm -f $(BMARKS_DEPS)

.PHONY: clean clean_bmarks clean_apps clean_archive realclean

depend: $(ARCHIVE_DEPS) $(APP_DEPS) $(BMARKS_DEPS)

ifeq (,$(filter %clean,$(MAKECMDGOALS)))
-include $(ARCHIVE_DEPS)
-include $(APP_DEPS)
-include $(BMARKS_DEPS)
endif

