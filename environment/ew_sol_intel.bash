# Create an Earthworm environment on Intel Solaris
# This file should be sourced by a Bourne shell wanting
# to run or build an EARTHWORM system under Intel Solaris.

# Set environment variables describing your Earthworm directory/version

# Use value from elsewhere if defined (eg from .bashrc)
# otherwise use the value after the :-
export EW_HOME="${EW_INSTALL_HOME:-/opt/earthworm}"
export EW_VERSION="${EW_INSTALL_VERSION:-earthworm_7.10}"
EW_RUN_DIR="${EW_RUN_DIR:-${EW_HOME}/run_working}"
# Or set your own values directly
#export EW_HOME=/opt/earthworm
#export EW_VERSION=earthworm_7.10
#EW_RUN_DIR=$EW_HOME/run_working

# This next env var is required if you run statmgr:
export SYS_NAME=`hostname`

# Set environment variables used by earthworm modules at run-time
# Path names must end with the slash "/"
export EW_INSTALLATION="${EW_INSTALL_INSTALLATION:-INST_UNKNOWN}"
export EW_PARAMS="${EW_RUN_DIR}/params/"
export EW_LOG="${EW_RUN_DIR}/log/"
export EW_DATA_DIR="${EW_RUN_DIR}/data/"

# Tack the earthworm bin directory in front of the current path
export PATH="${EW_HOME}/${EW_VERSION}/bin:${PATH}"

# Set environment variables for compiling Earthworm modules

# Be explicit about which compilers to use
export CC=cc
export CCC=CC

export EW_BITS="${EW_INSTALL_BITS:-32}"
TARGET="-m${EW_BITS}"

# Warning flags for compiler:
WARNFLAGS=""

# -D_FILE_OFFSET_BITS=64 is for Large Filesystem Support (64-bit offsets)
# on a 32-bit compile; it is benign on a 64-bit compile

# cc -mt compiles (-D_REENTRANT) and links with Solaris threads (-lthread) support

# Set initial defaults for the SunOS make implicit .c.o and .cpp.o target rules

# C compiler flags (also used for ld flags)
export CFLAGS="${TARGET} -g -mt ${WARNFLAGS}"
# C++ compiler flags
export CCFLAGS=$"{CFLAGS}"
# C preprocessor defs and includes
export CPPFLAGS="-D_SOLARIS -D_INTEL -D_FILE_OFFSET_BITS=64 -I${EW_HOME}/${EW_VERSION}/include"

# Earthworm makefiles override CFLAGS=$(GLOBALFLAGS) (for now), and not all
# makefiles have been converted yet to use the implicit make target rules
export GLOBALFLAGS="${CFLAGS} ${CPPFLAGS}"

# Use the Solaris makefile.unix options
export PLATFORM="SOLARIS"

# Eventually replace with gfortran when we move away from Forte compilers
# Auto-detect fortran compiler and flags
if which f90 1> /dev/null 2>&1
then
   export FC=f90
   export FFLAGS="${TARGET} -g -O"
fi

# Alternatively, you can hard-code values here:
#export FC='...'
#export FFLAGS='...'
