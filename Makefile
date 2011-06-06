RCCEPATH=/home/trigonak/rcce/latest
SYMBOLSPATH=/home/trigonak/rcce/latest/common/symbols
API          := nongory
iRCCESRC     := ./src
iRCCEINCLUDE := ./include
#libtask
LIBTASKSRC := $(iRCCESRC)/libtask
LIBTASK := $(iRCCESRC)/libtask/libtask.a
LIBTASKOBJS= $(LIBTASKSRC)/asm.o $(LIBTASKSRC)/channel.o $(LIBTASKSRC)/context.o $(LIBTASKSRC)/fd.o $(LIBTASKSRC)/net.o \
	$(LIBTASKSRC)/print.o $(LIBTASKSRC)/qlock.o $(LIBTASKSRC)/rendez.o $(LIBTASKSRC)/task.o

include $(SYMBOLSPATH)

CFLAGS := $(CFLAGS) -I$(iRCCEINCLUDE)

ARCHIVEOBJS= iRCCE_isend.o iRCCE_irecv.o iRCCE_isend32.o iRCCE_irecv32.o iRCCE_admin.o iRCCE_synch.o iRCCE_send.o iRCCE_recv.o iRCCE_get.o iRCCE_put.o iRCCE_waitlist.o \
	 pubSubTM.o RCCE_barrier_nb.o rwhash.o log.o

.PHONY: all libs clean install dist

$(ARCHIVE): $(ARCHIVEOBJS)
	@echo Archive name = $(ARCHIVE) 
	@(cd $(RCCEPATH) ; make)
	ar -r $(ARCHIVE) $(ARCHIVEOBJS) 
	rm -f *.o

# $ s (LIBTASKOBJS)

iRCCE_isend.o: $(iRCCESRC)/iRCCE_isend.c $(RCCEINCLUDE)/RCCE.h $(RCCEINCLUDE)/RCCE_lib.h $(iRCCEINCLUDE)/iRCCE.h $(iRCCEINCLUDE)/iRCCE_lib.h $(iRCCEINCLUDE)/pubSubTM.h 
	$(CCOMPILE) -c $(CFLAGS) $(iRCCESRC)/iRCCE_isend.c  $(RCCE_FLAGS)

iRCCE_irecv.o: $(iRCCESRC)/iRCCE_irecv.c $(RCCEINCLUDE)/RCCE.h $(RCCEINCLUDE)/RCCE_lib.h $(iRCCEINCLUDE)/iRCCE.h $(iRCCEINCLUDE)/iRCCE_lib.h $(iRCCEINCLUDE)/pubSubTM.h
	$(CCOMPILE) -c $(CFLAGS) $(iRCCESRC)/iRCCE_irecv.c  $(RCCE_FLAGS)

iRCCE_isend32.o: $(iRCCESRC)/iRCCE_isend32.c $(RCCEINCLUDE)/RCCE.h $(RCCEINCLUDE)/RCCE_lib.h $(iRCCEINCLUDE)/iRCCE.h $(iRCCEINCLUDE)/iRCCE_lib.h $(iRCCEINCLUDE)/pubSubTM.h
	$(CCOMPILE) -c $(CFLAGS) $(iRCCESRC)/iRCCE_isend32.c  $(RCCE_FLAGS)

iRCCE_irecv32.o: $(iRCCESRC)/iRCCE_irecv32.c $(RCCEINCLUDE)/RCCE.h $(RCCEINCLUDE)/RCCE_lib.h $(iRCCEINCLUDE)/iRCCE.h $(iRCCEINCLUDE)/iRCCE_lib.h $(iRCCEINCLUDE)/pubSubTM.h
	$(CCOMPILE) -c $(CFLAGS) $(iRCCESRC)/iRCCE_irecv32.c  $(RCCE_FLAGS)

iRCCE_admin.o: $(iRCCESRC)/iRCCE_admin.c $(RCCEINCLUDE)/RCCE.h $(RCCEINCLUDE)/RCCE_lib.h $(iRCCEINCLUDE)/iRCCE.h $(iRCCEINCLUDE)/iRCCE_lib.h $(iRCCEINCLUDE)/pubSubTM.h
	$(CCOMPILE) -c $(CFLAGS) $(iRCCESRC)/iRCCE_admin.c  $(RCCE_FLAGS)

iRCCE_synch.o: $(iRCCESRC)/iRCCE_synch.c $(RCCEINCLUDE)/RCCE.h $(RCCEINCLUDE)/RCCE_lib.h $(iRCCEINCLUDE)/iRCCE.h $(iRCCEINCLUDE)/iRCCE_lib.h $(iRCCEINCLUDE)/pubSubTM.h
	$(CCOMPILE) -c $(CFLAGS) $(iRCCESRC)/iRCCE_synch.c  $(RCCE_FLAGS)

iRCCE_send.o: $(iRCCESRC)/iRCCE_send.c $(RCCEINCLUDE)/RCCE.h $(RCCEINCLUDE)/RCCE_lib.h $(iRCCEINCLUDE)/iRCCE.h $(iRCCEINCLUDE)/iRCCE_lib.h $(iRCCEINCLUDE)/pubSubTM.h
	$(CCOMPILE) -c $(CFLAGS) $(iRCCESRC)/iRCCE_send.c  $(RCCE_FLAGS)

iRCCE_recv.o: $(iRCCESRC)/iRCCE_recv.c $(RCCEINCLUDE)/RCCE.h $(RCCEINCLUDE)/RCCE_lib.h $(iRCCEINCLUDE)/iRCCE.h $(iRCCEINCLUDE)/iRCCE_lib.h $(iRCCEINCLUDE)/pubSubTM.h
	$(CCOMPILE) -c $(CFLAGS) $(iRCCESRC)/iRCCE_recv.c  $(RCCE_FLAGS)

iRCCE_get.o: $(iRCCESRC)/iRCCE_get.c $(RCCEINCLUDE)/RCCE.h $(RCCEINCLUDE)/RCCE_lib.h $(iRCCEINCLUDE)/iRCCE.h $(iRCCEINCLUDE)/iRCCE_lib.h $(iRCCEINCLUDE)/pubSubTM.h
	$(CCOMPILE) -c $(CFLAGS) $(iRCCESRC)/iRCCE_get.c  $(RCCE_FLAGS)

iRCCE_put.o: $(iRCCESRC)/iRCCE_put.c $(RCCEINCLUDE)/RCCE.h $(RCCEINCLUDE)/RCCE_lib.h $(iRCCEINCLUDE)/iRCCE.h $(iRCCEINCLUDE)/iRCCE_lib.h $(iRCCEINCLUDE)/pubSubTM.h
	$(CCOMPILE) -c $(CFLAGS) $(iRCCESRC)/iRCCE_put.c  $(RCCE_FLAGS)

iRCCE_waitlist.o: $(iRCCESRC)/iRCCE_waitlist.c $(RCCEINCLUDE)/RCCE.h $(RCCEINCLUDE)/RCCE_lib.h $(iRCCEINCLUDE)/iRCCE.h $(iRCCEINCLUDE)/iRCCE_lib.h $(iRCCEINCLUDE)/pubSubTM.h
	$(CCOMPILE) -c $(CFLAGS) $(iRCCESRC)/iRCCE_waitlist.c  $(RCCE_FLAGS)


RCCE_barrier_nb.o: $(iRCCESRC)/RCCE_barrier_nb.c $(RCCEINCLUDE)/RCCE.h $(RCCEINCLUDE)/RCCE_lib.h $(iRCCEINCLUDE)/iRCCE.h $(iRCCEINCLUDE)/task.h
	$(CCOMPILE) -c $(CFLAGS) $(iRCCESRC)/RCCE_barrier_nb.c $(LIBTASK)

rwhash.o: $(iRCCESRC)/rwhash.c $(RCCEINCLUDE)/RCCE.h $(RCCEINCLUDE)/RCCE_lib.h $(iRCCEINCLUDE)/iRCCE.h $(iRCCEINCLUDE)/rwhash.h
	$(CCOMPILE) -c $(CFLAGS) $(iRCCESRC)/rwhash.c

log.o: $(iRCCESRC)/log.c $(RCCEINCLUDE)/RCCE.h $(RCCEINCLUDE)/RCCE_lib.h $(iRCCEINCLUDE)/iRCCE.h $(iRCCEINCLUDE)/log.h
	$(CCOMPILE) -c $(CFLAGS) $(iRCCESRC)/log.c

pubSubTM.o: $(iRCCESRC)/pubSubTM.c  $(RCCEINCLUDE)/RCCE.h $(RCCEINCLUDE)/RCCE_lib.h $(iRCCEINCLUDE)/iRCCE.h $(iRCCEINCLUDE)/iRCCE_lib.h $(iRCCEINCLUDE)/task.h \
	    $(iRCCEINCLUDE)/pubSubTM.h $(iRCCEINCLUDE)/common.h $(iRCCEINCLUDE)/tm.h $(iRCCEINCLUDE)/stm.h $(iRCCEINCLUDE)/hashtable.h $(iRCCEINCLUDE)/rwhash.h $(iRCCEINCLUDE)/log.h
	$(CCOMPILE) -c $(CFLAGS) $(iRCCESRC)/pubSubTM.c  $(LIBTASK)

clean:
	rm -f $(ARCHIVE) $(ARCHIVEOBJS)

