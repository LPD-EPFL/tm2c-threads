# Main Makefile for DSTM
# For platform, choose one out of: iRCCE,MCORE,CLUSTER

PLATFORM = iRCCE

include Makefile.common

DEPENDFILE:= .depends

SRCS_PURE:= pubSubTM.c tm.c rwhash.c log.c dslock.c \
			measurements.c pgas.c

SRCS:=$(addprefix $(SRCPATH)/, $(SRCS_PURE))
ARCHIVEOBJS= $(addprefix $(SRCPATH)/, $(SRCS_PURE:.c=.o))

# Platform specific Makefile
sinclude Makefile.$(PLATFORM)

.PHONY: all libs clean install dist

all: archive $(HELPER_APPS) applications benchmarks

$(DSTM_ARCHIVE): $(ARCHIVEOBJS)
	@echo Archive name = $(DSTM_ARCHIVE) 
ifeq ($(PLATFORM),iRCCE)
	@(cd $(RCCEPATH) ; make)
	# because we somehow need to fetch rcce's .a ARCHIVE
	mv $(ARCHIVE) $(DSTM_ARCHIVE)
endif
	ar -r $(DSTM_ARCHIVE) $(ARCHIVEOBJS)

archive: $(DSTM_ARCHIVE)

$(SRCPATH)/%.o:: $(SRCPATH)/%.c
	$(C) $(CFLAGS) -o $@ -c $<

applications:
	cd apps && $(MAKE) clean all

benchmarks:
	cd bmarks && $(MAKE) clean all

clean:
	cd apps && $(MAKE) clean
	cd bmarks && $(MAKE) clean
	rm -f $(ARCHIVE) $(ARCHIVEOBJS)
	rm -f $(DSTM_ARCHIVE)
	rm -f $(HELPER_APPS)
	rm -f $(DEPENDFILE)
	rm -rf $(addsuffix .dSYM, $(HELPER_APPS))

# depend: $(SRCS) $(DEPENDFILE)

# $(DEPENDFILE):
# 	$(C) -MM $(CFLAGS) $(SRCPATH)/*.c >$(DEPENDFILE)

# sinclude $(DEPENDFILE)


