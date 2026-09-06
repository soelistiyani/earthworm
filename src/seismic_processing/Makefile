#
#       Makefile for the Earthworm seismic_processing modules
#
#
# This makefile contains all the directives necessary to compile the
# Earthworm seismic_processing modules on Solaris, Unix (Linux and OS X),
# and NT, as well as clean up the Earthworm source and binary file tree.
#
# 1. To build (compile) the Earthworm modules on Solaris:
#        make solaris
#
# 2. To build (compile) the Earthworm modules on Unix (Llinux and OS X):
#        make unix
#
# 3. To build (compile) the Earthworm modules on Windows:
#        nmake nt
#
# 4. To clean up the source tree, i.e. remove all *.o, *.obj, *~, *%,
#    and core files from all modules, on Solaris:
#        make clean_solaris
#
# 5. To clean up the source tree, i.e. remove all *.o, *.obj, *~, *%,
#    and core files from all modules, on Unix (Linux and OS X):
#        make clean_unix
#
# 6. To clean up the source tree, i.e. remove all *.o, *.obj, *~, *%,
#    and core files from all modules, on Windows:
#        nmake clean_nt
#
# 7. To clean up the bin directory, i.e. remove all Earthworm programs,
#    on Solaris:
#        make clean_bin_solaris
#
# 8. To clean up the bin directory, i.e. remove all Earthworm programs,
#    on Unix (Linux and OS X):
#        make clean_bin_solaris
#
# 9. To clean up the bin directory, i.e. remove all Earthworm programs,
#    on Windows:
#        nmake clean_bin_nt

# Whenever a new module is added to the Earthworm distribution, it should be
# added to the list of ALL_MODULES, and to the appropriate lists of
# SOLARIS_MODULES, UNIX_MODULES, and NT_MODULES.  The reverse procedure should
# be followed when a module is removed from the Earthworm distribution.

# Note: Neither Sun make nor Microsoft nmake support the GNU gmake .PHONY
# pseudo-target.  Makefiles should use a phony target (the name does not matter)
# with no dependencies and no rules, then add the phony dependency to force a
# recipe to run.  This means there can never be a file with the phony target
# name (case insensitive).  Earthworm makefiles use PHONY for this purpose.

# Note: Windows nmake does not pass $(MAKEFLAGS) as part of $(MAKE) for
# recursive makes; use $(MAKE) /$(MAKEFLAGS).

#
# List all modules (for cleaning purposes)
#

ALL_MODULES = \
	$(C_MODULES) \
	eqfilterII \
	eqmaxel \
	eqverify_assemble \
	geqproc \
	globalproc \
	Mm \
	Ms \
	nll_mgr \
	nq2pgm \
	Theta \
	$(FORTRAN_MODULES)

C_MODULES = \
	binder_ew \
	carlstatrig \
	carlsubtrig \
	coda_aav \
	coda_dur \
	compress_UA \
	debias \
	decimate \
	decompress_UA \
	eqassemble \
	eqbuf \
	eqcoda \
	eqfilter \
	eqprelim \
	eqproc \
	eqverify \
	evansassoc \
	evanstrig \
	ew_rsamalarm \
	ew2rsam \
	ew2ssam \
	ewaccel \
	ewdrift \
	ewintegrate \
	ewnotify \
	ewshear \
	ewspectra \
	ewthresh \
	fir \
	gmew \
	hypAssoc \
	loc_wcatwc \
	localmag \
	pick_ew \
	pick_FP \
	pick_wcatwc \
	pkfilter \
	raypicker \
	statrigfilter \
	wftimefilter \
	wftimeshifter

FORTRAN_MODULES = \
	hyp71_mgr \
	hyp2000 \
	hyp2000_mgr \
	Hypo71PC \
	rayloc_ew

#
# List all modules to be compiled into the Solaris distribution
#

SOLARIS_MODULES = \
	$(C_MODULES) \
	nll_mgr \
	$(FORTRAN_MODULES)

#
# List all modules to be compiled into the Unix/Linux/Mac OS X distribution
#

UNIX_MODULES = \
	$(C_MODULES) \
	nll_mgr \
	$(FORTRAN_MODULES)

#
# List all modules to be compiled into the Windows distribution
#

# The Windows modules are determined in nt_modules target recipe

#####################
# Top level targets #
#####################

# There is no default target; print usage info
usage: PHONY
	@echo "Usage: make unix or make solaris or nmake nt"

solaris: solaris_modules
unix:    unix_modules
nt:      nt_modules

###################
# Solaris targets #
###################

solaris_modules: PHONY
	@for x in $(SOLARIS_MODULES) ; \
	do ( \
		cd $$x && \
			echo ---------- ; \
			echo Making $@ in: `pwd` ; \
			$(MAKE) -f makefile.unix \
	) ; done

clean_solaris:     clean_unix
clean_bin_solaris: clean_bin_unix

###############################
# Unix/Linux/Mac OS X targets #
###############################

unix_modules: PHONY
	@for x in $(UNIX_MODULES) ; \
	do ( \
		cd $$x && \
			echo ---------- ; \
			echo Making $@ in: `pwd` ; \
			$(MAKE) -f makefile.unix \
	) ; done

clean_unix: PHONY
	-@for x in $(ALL_MODULES) ; \
	do ( \
		cd $$x && \
			echo Cleaning in: `pwd` ; \
			$(MAKE) -f makefile.unix clean \
	) ; done

clean_bin_unix: PHONY
	-@for x in $(ALL_MODULES) ; \
	do ( \
		cd $$x && \
			echo Cleaning binaries in: `pwd` ; \
			$(MAKE) -f makefile.unix clean_bin \
	) ; done

###################
# Windows targets #
###################

nt_modules: PHONY
	@for %x in ( $(C_MODULES) ) \
	do @( \
		pushd %x & \
			echo ---------- & \
			for /f "delims=" %d in ( 'cd' ) do @echo Making $@ in: %d & \
			$(MAKE) /$(MAKEFLAGS) /f makefile.nt & \
		popd \
	)
	-@if NOT "$(FC)" == "" ( \
		for %x in ( $(FORTRAN_MODULES) ) \
		do @( \
			pushd %x & \
				echo ---------- & \
				for /f "delims=" %d in ( 'cd' ) do @echo Making $@ in: %d & \
				$(MAKE) /$(MAKEFLAGS) /f makefile.nt & \
			popd \
		) \
	)

clean_nt: PHONY
	-@for %x in ( $(ALL_MODULES) ) \
	do @( \
		pushd %x & \
			for /f "delims=" %d in ( 'cd' ) do @echo Cleaning in: %d & \
			$(MAKE) /$(MAKEFLAGS) /f makefile.nt clean & \
		popd \
	)

clean_bin_nt: PHONY
	-@for %x in ( $(ALL_MODULES) ) \
	do @( \
		pushd %x & \
			for /f "delims=" %d in ( 'cd' ) do @echo Cleaning binaries in: %d & \
			$(MAKE) /$(MAKEFLAGS) /f makefile.nt clean_bin & \
		popd \
	)

PHONY:
