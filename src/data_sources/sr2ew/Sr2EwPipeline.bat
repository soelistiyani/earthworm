@echo off

rem FILE: Sr2EwPipeline.bat ( Example Seismic App Earthworm )
rem COPYRIGHT: (c), Symmetric Research, 2013-2018
rem
rem This batch file shows how to pipe USBxCH data all the way from
rem Blast to an Earthworm ring in TRACEBUF2 format.  Along the way the pipe
rem utilities interpolate the data to 100Hz and carry out other tasks.
rem 
rem


rem Save path to the current example directory.

set EXAMPLEDIR=%CD%


rem Keep all title and path changes local ( ie restore on exit )

setlocal

title SR SR2EWPIPELINE.BAT - ( Blast to Earthworm )
path .;C:\SR\USBXCH\Exe;%EXAMPLEDIR%;%PATH%



rem Announce the program start:

echo SR2EWPIPELINE.BAT: Starting ...
echo.




rem Check for USBxCH driver and board presence.

echo.
echo SR2EWPIPELINE.BAT: Checking for USBxCH presence ( SrUsbXch0 ):
echo.

Presence.exe SrUsbXch0

if ERRORLEVEL 1  goto :ErrorReturn




rem Set an environment variable with the INI file name.
rem
rem Take the name from the batch command line or use the default.  This
rem one file is shared among all of the pipeline utilities comprising 
rem Sr2EwPipeline.bat and has [sections] for each utility.
rem
rem Currently the default INI file is taken from the Earthworm run\params
rem directory.  This assumes the Earthworm environment has already been
rem set by running the ew_nt.cmd batch file first.  An alternative would
rem be to have the default INI in this example directory.  To do that, change
rem
rem     set INIFILE=%EW_PARAMS%\Sr2Ew.ini
rem to
rem     set INIFILE=%EXAMPLEDIR%\Sr2Ew.ini
rem
rem NOTE: Referring to a batch argument with percent tilda like %~1 
rem       instead of %1 strips off any quotes.  The quotes can be added 
rem       later where they are needed.
rem       
rem       

set INIFILE=%~1
if "%INIFILE%" == "" (
        set INIFILE=%EW_PARAMS%\Sr2Ew.ini
)



rem Check the INI syntax:
rem
rem Before actually executing it is best to check the INI syntax.  If there is
rem a syntax error, this batch file terminates.  Fix the error and try again.
rem 
rem Note:
rem
rem These syntax checks are done without /to clause on the utilities.  Because
rem of that the "nextprog" field on the INI reports will be reported as NULL.  See
rem below for the /to clause on each when the utilities are actually run.
rem 
rem 

echo.
echo.
echo.
echo SR2EWPIPELINE.BAT: Checking INI syntax:
echo.

Blast /syntax "%INIFILE%"
if ERRORLEVEL 1 goto :SyntaxError
echo.

Pak2Bin /syntax "%INIFILE%"
if ERRORLEVEL 1 goto :SyntaxError
echo.

Interp /syntax "%INIFILE%"
if ERRORLEVEL 1 goto :SyntaxError
echo.

rem FIX
Bin2Asc /syntax "%INIFILE%"
if ERRORLEVEL 1 goto :SyntaxError
echo.

rem sr2ew syntax is handled with Earthworm .d processing

goto :SyntaxGood


:SyntaxError
echo. 
echo SR2EWPIPELINE.BAT: ERROR, failed INI syntax check, quitting ...
echo. 
goto :ErrorReturn

:SyntaxGood
echo.
echo SR2EWPIPELINE.BAT: SUCCESS, INI syntax check passed ...
echo.




rem Start the pipeline chain in the background.
rem
rem NOTES:
rem
rem * Sampling rates, etc ... are all set in the single INI file
rem * "Start /b" is the Windows way of starting a process in the background
rem * Remember these programs are running in the YMDHMS subdirectory.
rem
rem

echo.
echo.
echo SR2EWPIPELINE.BAT: Starting the pipeline in the background ...

start /b Blast    "%INIFILE%"  /to Pak2Bin
start /b Pak2Bin  "%INIFILE%"  /to Interp 
start /b Interp   "%INIFILE%"  /to sr2ew
rem      sr2ew    this is started by Earthworm startstop

echo SR2EWPIPELINE.BAT: background pipeline started ... see *.rpt files for info
echo.



rem Success return, returns immediately.
rem
rem

:Return

exit 0



rem Error return, includes a pause. 
rem
rem

echo.
echo.
echo SR2EWPIPELINE.BAT: An Error Occurred
echo.
echo This window will go away after "press any key to continue" ...
echo.
pause
echo.

exit 1
