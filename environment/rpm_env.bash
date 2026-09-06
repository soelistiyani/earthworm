#export EW_HOME="${EW_INSTALL_HOME:-/opt/earthworm}"
CURR_DIR=$(pwd)
export EW_HOME="${EW_INSTALL_HOME:-"$(dirname "$CURR_DIR")"}"
export EW_VERSION="${EW_INSTALL_VERSION:-earthworm_7.10}"
export EW_BITS=64

export PATH="$EW_HOME/$EW_VERSION/bin:$PATH"

export WARNFLAGS="-Wall -Wextra -Wno-sign-compare -Wno-unknown-pragmas -Wno-pragmas -Wformat"
export GLOBALFLAGS="-m${EW_BITS} -g -D_LINUX -Dlinux -D_INTEL -D_USE_SCHED -D_USE_PTHREADS -D_USE_TERMIOS -D_FILE_OFFSET_BITS=64 -I${EW_HOME}/${EW_VERSION}/include ${WARNFLAGS}"
export PLATFORM="LINUX"

export LINK_LIBS="-lm -lpthread"

export CFLAGS=${GLOBALFLAGS}
export CXXFLAGS=${GLOBALFLAGS}

FC=gfortran
export FC
export FFLAGS="${TARGET}-O -g -Wuninitialized -Werror=line-truncation -ffpe-trap=invalid,zero,overflow -fbacktrace"
