@echo off
REM ===============================================================
REM Depesha: сборка под Symbian (GCCE, abld), Qt 4.7.3.
REM
REM Инструментарий 2011 года не переваривает кириллицу в путях, поэтому
REM исходники зеркалятся в %DP_WORK%. Проект ссылается на общий код
REM Kutegram, значит зеркалятся оба каталога рядом.
REM
REM Usage:  build-symbian.bat          - зеркалирование, сборка, .sis
REM         build-symbian.bat clean    - очистить рабочее дерево
REM ===============================================================
setlocal

set SDKPREFIX=C:\QtSDK\Symbian\SDKs\Symbian1Qt473
set SYMBIANDIR=C:\QtSDK\Symbian
set EPOCROOT=\QtSDK\Symbian\SDKs\Symbian1Qt473\
set QTDIR=%SDKPREFIX%
set GCCPATH=%SYMBIANDIR%\tools\gcce4
set NoDefaultCurrentDirectoryInExePath=
set PATH=%SDKPREFIX%\epoc32\gcc\bin;%SDKPREFIX%\epoc32\tools;%GCCPATH%\bin;%SYMBIANDIR%\tools\perl\bin;%SYMBIANDIR%\tools\sbs\bin;%SDKPREFIX%\bin;%GCCPATH%\arm-none-symbianelf\bin;%SDKPREFIX%\mkspecs\default;%QTDIR%;C:\Windows\System32;%PATH%;.

if "%DP_ROOT%"==""  set DP_ROOT=%~dp0..
if "%DP_WORK%"==""  set DP_WORK=C:\dpbuild

if "%1"=="clean" (
    md "%TEMP%\dpempty" 2>nul
    if exist "%DP_WORK%" robocopy "%TEMP%\dpempty" "%DP_WORK%" /MIR /NJH /NJS >nul
    echo Work tree emptied: %DP_WORK%
    goto :end
)

echo === mirror ===
robocopy "%DP_ROOT%\kutegram\quick" "%DP_WORK%\kutegram\quick" /MIR /NFL /NDL /NJH /NJS /NP /XD .git >nul
if errorlevel 8 exit /b 1
robocopy "%DP_ROOT%\depesha" "%DP_WORK%\depesha" /MIR /NFL /NDL /NJH /NJS /NP /XD .git >nul
if errorlevel 8 exit /b 1

cd /d "%DP_WORK%\depesha"
if errorlevel 1 exit /b 1

echo === qmake ===
call qmake -spec symbian-abld CONFIG+=release
if errorlevel 1 exit /b 1

echo === make release-gcce ===
REM abld -- perl-скрипт и возвращает 0 даже когда компилятор упал, поэтому
REM успех проверяется по журналу, а не по коду возврата.
call make release-gcce > "%DP_WORK%\build.log" 2>&1
type "%DP_WORK%\build.log"
if errorlevel 1 exit /b 1
findstr /i /c:"error:" /c:"Error 1" /c:"Error 2" "%DP_WORK%\build.log" >nul && (
    echo.
    echo BUILD FAILED - see %DP_WORK%\build.log
    exit /b 1
)

echo === make sis ===
del /q "%DP_WORK%\depesha\Depesha.sis" 2>nul
call make sis QT_SIS_OPTIONS=-d

echo === collect ===
set DP_VER=
for /f "tokens=3" %%v in ('findstr /b /c:"VERSION = " "%DP_ROOT%\depesha\depesha.pro"') do set DP_VER=%%v
if "%DP_VER%"=="" set DP_VER=unknown
if not exist "%DP_WORK%\depesha\Depesha.sis" (
    echo PACKAGING FAILED - no Depesha.sis was produced
    exit /b 1
)
copy /y "%DP_WORK%\depesha\Depesha.sis" "%~dp0Depesha-%DP_VER%.sis" >nul 2>nul
echo Done. Artifact copied as Depesha-%DP_VER%.sis
:end
endlocal
