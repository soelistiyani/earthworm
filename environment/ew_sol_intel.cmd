# Create an Earthworm environment on Intel Solaris
# This file should be sourced by a C shell wanting
# to run or build an EARTHWORM system under Intel Solaris.

# Set environment variables describing your Earthworm directory/version

setenv EW_HOME          /opt/earthworm
setenv EW_VERSION       earthworm_7.10
set EW_RUN_DIR = ${EW_HOME}/run_working

# This next env var is required if you run statmgr:
setenv SYS_NAME         `hostname`

# Set environment variables used by earthworm modules at run-time
# Path names must end with the slash "/"
setenv EW_INSTALLATION  INST_UNKNOWN
setenv EW_PARAMS        ${EW_RUN_DIR}/params/
setenv EW_LOG           ${EW_RUN_DIR}/log/
setenv EW_DATA_DIR      ${EW_RUN_DIR}/data/

# Tack the earthworm bin directory in front of the current path
set path=( ${EW_HOME}/${EW_VERSION}/bin ${path} )

# Set up library path for dynamically loaded libraries:
setenv OPENWINHOME      /usr/openwin
setenv COMPILER_DIR     /opt/SUNWspro
if ("${?LD_LIBRARY_PATH}" == 0) then
    setenv LD_LIBRARY_PATH "${OPENWINHOME}/lib:${COMPILER_DIR}/lib:/usr/lib"
else
    setenv LD_LIBRARY_PATH "${OPENWINHOME}/lib:${COMPILER_DIR}/lib:/usr/lib:${LD_LIBRARY_PATH}"
endif

# Set environment variables for compiling Earthworm modules

# Be explicit about which compilers to use
setenv CC               cc
setenv CCC              CC
setenv FC               f90

#setenv EW_BITS          32
setenv EW_BITS          64
set TARGET = -m${EW_BITS}

# Warning flags for compiler:
set WARNFLAGS = ""

# -D_FILE_OFFSET_BITS=64 is for Large Filesystem Support (64-bit offsets)
# on a 32-bit compile; it is benign on a 64-bit compile

# cc -mt compiles (-D_REENTRANT) and links with Solaris threads (-lthread) support

# Set initial defaults for the SunOS make implicit .c.o and .cpp.o target rules

# C compiler flags (also used for ld flags)
setenv CFLAGS           "${TARGET} -g -mt ${WARNFLAGS}"
# C++ compiler flags
setenv CCFLAGS          "${CFLAGS}"
# C preprocessor defs and includes
setenv CPPFLAGS         "-D_SOLARIS -D_INTEL -D_FILE_OFFSET_BITS=64 -I${EW_HOME}/${EW_VERSION}/include"
setenv FFLAGS           "${TARGET} -g -O"

# Earthworm makefiles override CFLAGS=$(GLOBALFLAGS) (for now), and not all
# makefiles have been converted yet to use the implicit make target rules
setenv GLOBALFLAGS      "${CFLAGS} ${CPPFLAGS}"

# Use the Solaris makefile.unix options
setenv PLATFORM         "SOLARIS"

# Number of available file descriptors
limit descriptors 256
