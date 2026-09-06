LINUX_FLAGS    = -lm -lpthread
SOLARIS_FLAGS  = -lm -lrt -lpthread
SPECIFIC_FLAGS = $($(PLATFORM)_FLAGS)

CFLAGS = $(GLOBALFLAGS)

B = $(EW_HOME)/$(EW_VERSION)/bin
L = $(EW_HOME)/$(EW_VERSION)/lib

APP = dcsn

OBJS = $(APP).o

EW_LIBS = \
	$L/getutil.o \
	$L/kom.o \
	$L/lockfile.o \
	$L/lockfile_ew.o \
	$L/logit_mt.o \
	$L/sleep_ew.o \
	$L/time_ew.o \
	$L/transport.o \
	-L$L -lew_mt

$B/$(APP): $(OBJS)
	$(CC) -o $@ $(CFLAGS) $(OBJS) $(EW_LIBS) $(SPECIFIC_FLAGS)


# Clean-up rules
clean: PHONY
	-$(RM) a.out core *.o *.obj *% *~

clean_bin: PHONY
	-$(RM) $B/$(APP) $B/$(APP).exe

PHONY:
