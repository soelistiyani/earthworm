@Echo Off

PushD .

@Rem Set environment variables used by Earthworm modules at run-time
@Rem ---------------------------------------------------------------
Set EW_INSTALLATION=INST_UNKNOWN
Set EW_HOME=C:\earthworm
Set EW_VERSION=earthworm_7.10

@Rem EW_BITS is for compile-time only, not run-time. It sets whether to 
@Rem build as a 64 bit application or a 32 bit application. 
@Rem A 32 bit Earthwom module won't work properly on a 64 bit Earthworm
@Rem startstop, and a 64 bit module won't work with a 32 bit startstop.
@Rem A 32 bit Earthworm _will_ work on a 64 bit version of the Windows 
@Rem operating system 
Set EW_BITS=64

Set EW_RUN_DIR=%EW_HOME%\run_working
Set EW_PARAMS=%EW_RUN_DIR%\params\
Set EW_LOG=%EW_RUN_DIR%\log\
Set EW_DATA_DIR=%EW_RUN_DIR%\data\
Set SYS_NAME=%COMPUTERNAME%
Set EW_INST_ID=%EW_INSTALLATION%

@Rem Enable the appropriate commands below to build from a Windows Command
@Rem Prompt window.  If you run a Microsoft Visual Studio or Intel Parallel
@Rem Studio command-line tool from the Windows Start menu to do the build,
@Rem you will not have to enable any commands here; the command-line tools
@Rem run the appropriate commands for you.  See the inline comments below.

@Rem The minimum required Microsoft Visual Studio is VS2015, Update 3.
@Rem Microsoft Visual Studio Community (formerly Express) Editions are available
@Rem for no cost.

@Rem A Fortran compiler is optional.  The minimum Intel Fortran compiler version
@Rem that supports Visual Studio 2015 is 15.0.4.221 (Composer/Parallel Studio XE
@Rem 2015, Update 4).  Intel Fortran requires a license.

@Rem To compile the Fortran modules, set FC to the Fortran compiler command

@Rem Note: If Intel Fortran is installed, the Fortran environment setup will
@Rem also set up the Visual Studio environment.

@Rem The default is not to compile any Fortran modules
Set FC=
@Rem The Intel Fortran compiler command is ifort
@Rem Set FC=ifort

If "%EW_BITS%" == "32" ( Goto SETUP_START )
If "%EW_BITS%" == "64" ( Goto SETUP_START )
@Echo Error: EW_BITS must be 32 or 64
@Exit /B 1

:SETUP_START

If "%FC%" == "" ( Goto SETUP_MSVC )

@Rem Set up the Fortran compilation environment
@Rem ------------------------------------------

:SETUP_FC

If NOT "%FC%" == "ifort" ( Goto END_IFORT )

@Rem Set up Intel Fortran compilation environment
@Rem --------------------------------------------

Set target=
If "%EW_BITS%" == "32" ( Set target=ia32    )
If "%EW_BITS%" == "64" ( Set target=intel64 )

@Rem Intel Parallel Studio (formerly Composer) XE 2015 Update 4 for Windows
@Rem From the Start menu, select IA-32 Visual Studio 2015 mode,
@Rem which will execute the following command; SDK 8.1 will be used
@Rem Call "C:\Program Files (x86)\Intel\Composer XE 2015\bin\ifortvars.bat" %target%
@Rem Intel Parallel Studio XE 2016 for Windows
@Rem Call "C:\Program Files (x86)\IntelSWTools\compilers_and_libraries\windows\bin\ifortvars.bat" %target%

@Rem Define the Intel Fortran compiler options
@Rem Extra run-time checks: /check:bounds,uninit
@Rem ifort version 15 introduced the /Qinit option; older compilers complain, but ignore it
@Rem ifort complains about comments past column 72; /warn:truncated_source is pretty useless
@Rem Set FFLAGS=/nologo /O3 /Qinit:snan /Qinit:arrays /extend-source /warn:truncated_source /fpe-all:0 /check:bounds,uninit /Qdiag-disable:8290 /traceback
Set FFLAGS=/nologo /O3 /Qinit:snan /Qinit:arrays /extend-source /warn:truncated_source /fpe-all:0 /Qdiag-disable:8290 /traceback

@Rem FC_MAIN_IS_C is not necessary for MSVC
@Rem Set FC_MAIN_IS_C=-nofor-main

:END_IFORT

@Rem Make sure the Fortran compiler is usable
If NOT "%FC%" == "" (
   where %FC% >NUL
   If %ERRORLEVEL% NEQ 0 (
      @Set FC=
   )
)

@Rem The Intel Fortran compiler setup scripts also set up the MSVC environment
If "%FC%" == "ifort" Goto :SETUP_TARGET

@Rem Set up Visual C++ compilation environment 
@Rem -----------------------------------------

@Rem * Earthworm requires the C99 inttypes.h header and round() in math.h
@Rem   The minimum MSVC++ version is 18.0 (Visual Studio 2013/12.0)
@Rem * Earthworm requires the C99 size_t "z" format modifier
@Rem   The minimum MSVC++ version is 19.0 (Visual Studio 2015/14.0)

:SETUP_MSVC

Set target=
If "%EW_BITS%" == "32" ( Set target=x86   )
If "%EW_BITS%" == "64" ( Set target=amd64 )

@Rem From the Start menu, select VS2015 x64 Native Tools Command Prompt,
@Rem which will execute the following command; SDK 8.1 will be used
@Rem Call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" %target%
@Rem Call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" %target%

:SETUP_TARGET

@Rem Set the target Windows OS version to Windows 7
@Rem and the target SDK version to 8.1 for Win32.Mak
@Rem -----------------------------------------------
Set APPVER=6.1

@Rem End of compiler(s) setup
@Rem ------------------------

@Rem Set the path
@Rem ------------
Set PATH=%EW_HOME%\%EW_VERSION%\bin;%PATH%

@Rem Set the INCLUDE path
@Rem --------------------
@Rem Include "include_win" directory for "NtWin32.Mak" file
Set INCLUDE=%EW_HOME%\%EW_VERSION%\include;%EW_HOME%\%EW_VERSION%\include_win;%INCLUDE%
@Rem Set the include MySQL path
@Rem Set INCLUDE=%EW_HOME%\%EW_VERSION%\src\archiving\mole\mysql-connector-c-6.0.2\include;%INCLUDE%
@Rem If you want to compile ew2moledb with another MySQL library, then
@Rem Set the INCLUDE variable with your own mysql include dir and
@Rem copy the library file mysqlclient.lib to the directory
@Rem %EW_HOME%\%EW_VERSION%\src\archiving\mole\mysql-connector-c-build\lib

@Rem Set the LIB (library) path
@Rem --------------------------
Set WIN_FLUSH_OBJ=commode.obj
Set LIB=%EW_HOME%\%EW_VERSION%\lib;%LIB%

@Rem Set C compiler flags here
@Rem -------------------------

Set GLOBALFLAGS=/D_WINNT /D_INTEL
@Rem The /Od flag configures build for no optimization
@Rem Remove /Od for default optimization; add /Ox for maximum optimization
Set GLOBALFLAGS=%GLOBALFLAGS% /Od
@Rem Define WIN32_LEAN_AND_MEAN (required) to disable #include <winsock.h> in windows.h
@Rem otherwise winsock2.h fails (https://blogs.msdn.microsoft.com/oldnewthing/20091130-00/?p=15863)
Set GLOBALFLAGS=%GLOBALFLAGS% /DWIN32_LEAN_AND_MEAN
@Rem Define _CRT_SECURE_NO_DEPRECATE and _CRT_SECURE_NO_WARNINGS to suppress deprecation warnings
Set GLOBALFLAGS=%GLOBALFLAGS% /D_CRT_SECURE_NO_DEPRECATE /D_CRT_SECURE_NO_WARNINGS
@Rem Define _WINSOCK_DEPRECATED_NO_WARNINGS to suppress deprecation warnings
Set GLOBALFLAGS=%GLOBALFLAGS% /D_WINSOCK_DEPRECATED_NO_WARNINGS
@Rem Define _USE_32BIT_TIME_T for 32-bit-sized 'time_t' type (not supported on 64-bit builds)
If "%EW_BITS%" == "32" ( Set GLOBALFLAGS=%GLOBALFLAGS% /D_USE_32BIT_TIME_T )
Set CFLAGS=%GLOBALFLAGS%

@Rem Set environment variables for Glass compilation
@Rem -----------------------------------------------
Set GLASS_DIR=%EW_HOME%\%EW_VERSION%\src\seismic_processing\glass

@Rem Earthworm uses UTC
@Rem ------------------
Set TZ=GMT

PopD
