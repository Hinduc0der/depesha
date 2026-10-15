@echo off
REM ===============================================================
REM Depesha: сборка под Windows/mingw на том же Qt 4.7.3, что и телефон.
REM Не для выпуска: это единственный быстрый способ увидеть ошибки
REM компиляции, не дожидаясь abld.
REM
REM Проект ссылается на общий код Kutegram (../kutegram/quick), поэтому
REM зеркалятся оба каталога с сохранением их взаимного расположения.
REM ===============================================================
setlocal

set QTDIR=C:\QtSDK\Simulator\Qt\mingw
set MINGW=C:\QtSDK\mingw
set PATH=%QTDIR%\bin;%MINGW%\bin;C:\Windows\System32;%PATH%
set NoDefaultCurrentDirectoryInExePath=

if "%DP_ROOT%"==""  set DP_ROOT=%~dp0..
if "%DP_WORK%"==""  set DP_WORK=C:\dpwin

echo === mirror ===
robocopy "%DP_ROOT%\kutegram\quick" "%DP_WORK%\kutegram\quick" /MIR /NFL /NDL /NJH /NJS /NP /XD .git >nul
if errorlevel 8 exit /b 1
robocopy "%DP_ROOT%\depesha" "%DP_WORK%\depesha" /MIR /NFL /NDL /NJH /NJS /NP /XD .git >nul
if errorlevel 8 exit /b 1

REM Версия -- макрос, а make про макросы не знает: без этого окно
REM называется старой версией, хотя собрано новое.
copy /b "%DP_WORK%\depesha\src\main.cpp"+,, "%DP_WORK%\depesha\src\main.cpp" >nul

cd /d "%DP_WORK%\depesha"
if errorlevel 1 exit /b 1

echo === qmake ===
call qmake -spec win32-g++ CONFIG+=debug CONFIG+=console
if errorlevel 1 exit /b 1

echo === mingw32-make ===
call mingw32-make -j4
if errorlevel 1 exit /b 1

echo Done: %DP_WORK%\depesha\debug\Depesha.exe
endlocal
