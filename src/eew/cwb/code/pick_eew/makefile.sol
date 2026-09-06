LINUX_FLAGS    = -lm -lpthread
SOLARIS_FLAGS  = -lm -lrt -lpthread
SPECIFIC_FLAGS = $($(PLATFORM)_FLAGS)

CFLAGS = $(GLOBALFLAGS)

B = $(EW_HOME)/$(EW_VERSION)/bin
L = $(EW_HOME)/$(EW_VERSION)/lib

APP = pick_ew

OBJS = \
	compare.o \
	config.o \
	index.o \
	initvar.o \
	pick_ew.o \
	pick_ra.o \
	report.o \
	restart.o \
	sample.o \
	scan.o \
	sign.o \
	sniff_eew.o \
	stalist.o

EW_LIBS = \
	$L/chron3.o \
	$L/getutil.o \
	$L/kom.o \
	$L/logit_mt.o \
	$L/sleep_ew.o \
	$L/swap.o \
	$L/time_ew.o \
	$L/transport.o \
	$L/trheadconv.o \
	-L$L -lew_mt

$B/$(APP): $(OBJS)
	$(CC) -o $@ $(CFLAGS) $(OBJS) $(EW_LIBS) $(SPECIFIC_FLAGS)


# Clean-up rules
clean: PHONY
	-$(RM) a.out core *.o *.obj *% *~

clean_bin: PHONY
	-$(RM) $B/$(APP) $B/$(APP).exe

PHONY:
