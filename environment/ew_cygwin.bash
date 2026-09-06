# Create an Earthworm environment for Cygwin on Windows
# This file should be sourced by a Bourne shell wanting 
# to run or build an EARTHWORM system under Cygwin.

echo
echo "NOTE: Anything done with earthworm under Cygwin is VERY experimental"
echo

# Set environment variables describing your Earthworm directory/version
export EW_HOME=/cygdrive/c/earthworm
export EW_VERSION=earthworm_7.10
export SYS_NAME=`hostname`

# Set environment variables used by earthworm modules at run-time
# Path names must end with the slash "/"
export EW_INSTALLATION=INST_UNKNOWN
export EW_RUN_DIR=${EW_HOME}/run_working
export EW_PARAMS=${EW_RUN_DIR}/params/
export EW_LOG=${EW_RUN_DIR}/log/
export EW_DATA_DIR=${EW_RUN_DIR}/data/

export PATH=${EW_HOME}/${EW_VERSION}/bin\:$PATH

# Set environment variables for compiling earthworm modules
# we're keeping the LINUX and related defines here, because having them helps more than
# hurts cygwin.  We do add a -D_CYGWIN for cygwin specific nuances
# gcc -pthread compiles and links with POSIX Threads support

# Set initial defaults

# C compiler flags (also used for ld flags)
export CFLAGS="-g -pthread"
# C++ compiler flags
export CXXFLAGS="${CFLAGS}"
# C preprocessor defs and includes
export CPPFLAGS="-D_CYGWIN -D_LINUX -Dlinux -D_INTEL -D_USE_SCHED -D_USE_PTHREADS -D_USE_TERMIOS -D_FILE_OFFSET_BITS=64 -I${EW_HOME}/${EW_VERSION}/include"

# Earthworm makefiles override CFLAGS=$(GLOBALFLAGS) (for now), and not all
# makefiles have been converted yet to use the implicit gmake target rules
export GLOBALFLAGS="${CFLAGS} ${CPPFLAGS}"
