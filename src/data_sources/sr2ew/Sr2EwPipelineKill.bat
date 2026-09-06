@echo off

rem FILE: KillSr2EwPipeline.bat
rem COPYRIGHT: (c), Symmetric Research, 2015-2018
rem
rem
rem Terminate all utilities in the "SR to EW Pipeline" program chain.
rem
rem

echo Killing the "SR to EW Pipeline" utilities ( Blast, Pak2Bin, Interp, sr2ew ) ...

rem             sr2ew killed by Earthworm startstop
taskkill /f /im Interp.exe
taskkill /f /im Pak2Bin.exe
taskkill /f /im Blast.exe

echo.
echo Run tasklist to verify all SR utilities are gone.
echo.
pause
echo.


