@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
title Сборка Депеши

echo.
echo   ДЕПЕША -- клиент Telegram для Symbian S60v3
echo   =========================================
echo.

set ROOT=%~dp0
set SDK=C:\QtSDK\Symbian\SDKs\Symbian1Qt473

REM --- 1. SDK -------------------------------------------------------------
REM Nokia Qt SDK проприетарный, вложить его в архив нельзя -- ставится отдельно.
if not exist "%SDK%\epoc32\tools\abld.pl" (
    echo   [!] Не найден Nokia Qt SDK 1.1.
    echo.
    echo   Ожидался здесь: %SDK%
    echo.
    echo   Его надо поставить отдельно: это инструментарий Nokia, и класть его
    echo   в архив нельзя. Файл установщика называется
    echo       Nokia_Qt_SDK_Win_offline_v1_1_2_en.exe
    echo   и ищется по этому имени в сети (например, на archive.org^).
    echo.
    echo   При установке: путь C:\QtSDK, отметить Symbian и Qt 4.7.3.
    echo   Подробнее -- в depesha\СБОРКА.md, шаг 1.
    echo.
    pause
    exit /b 1
)
echo   [+] Nokia Qt SDK найден.

REM --- 2. Ключи Telegram API ----------------------------------------------
set SECRETS=%ROOT%kutegram\quick\libkg\apisecrets.h

if exist "%SECRETS%" goto :secrets_ok

echo.
echo   Нужны свои ключи Telegram API. Где взять:
echo.
echo     1. Откройте my.telegram.org и войдите по номеру телефона.
echo     2. API development tools -- заполните форму (название любое^).
echo     3. Там будут App api_id (число^) и App api_hash (32 символа^).
echo.
echo   Чужие ключи брать нельзя: ключ привязан к тому, кто его выпустил, и
echo   отвечает за чужое использование тоже он.
echo.

:ask_id
set "API_ID="
set /p API_ID=  api_id (только цифры): 
if "!API_ID!"=="" goto :ask_id

:ask_hash
set "API_HASH="
set /p API_HASH=  api_hash (32 символа): 
if "!API_HASH!"=="" goto :ask_hash

> "%SECRETS%" echo #ifndef APISECRETS_H
>> "%SECRETS%" echo #define APISECRETS_H
>> "%SECRETS%" echo.
>> "%SECRETS%" echo #define KUTEGRAM_API_ID !API_ID!
>> "%SECRETS%" echo #define KUTEGRAM_API_HASH "!API_HASH!"
>> "%SECRETS%" echo.
>> "%SECRETS%" echo #endif // APISECRETS_H

echo.
echo   [+] Ключи записаны в kutegram\quick\libkg\apisecrets.h
echo       (файл остаётся только у вас, в архив и в репозиторий он не идёт^)
goto :media

:secrets_ok
echo   [+] Ключи Telegram API уже на месте.

REM --- 3. Медиа-сервер (по желанию) ---------------------------------------
:media
set DEFAULTS=%ROOT%depesha\src\testdefaults.h
if exist "%DEFAULTS%" goto :build

echo.
echo   Медиа-сервер нужен только для голосовых, кружков, видеостикеров и
echo   гифок: этот телефон их не декодирует, их переводит ваш сервер.
echo   Без него текст, фотографии и обычные стикеры работают полностью.
echo.
set "WANT="
set /p WANT=  Вписать адрес своего сервера в сборку? (y/N): 
if /i not "!WANT!"=="y" goto :build

set "MEDIA_URL="
set /p MEDIA_URL=  Адрес (например http://1.2.3.4:8089/kg-media): 
if "!MEDIA_URL!"=="" goto :build

set "MEDIA_TOKEN="
set /p MEDIA_TOKEN=  Токен: 

> "%DEFAULTS%" echo #ifndef TESTDEFAULTS_H
>> "%DEFAULTS%" echo #define TESTDEFAULTS_H
>> "%DEFAULTS%" echo.
>> "%DEFAULTS%" echo #define KG_TEST_MEDIA_URL "!MEDIA_URL!"
>> "%DEFAULTS%" echo #define KG_TEST_MEDIA_TOKEN "!MEDIA_TOKEN!"
>> "%DEFAULTS%" echo.
>> "%DEFAULTS%" echo #endif // TESTDEFAULTS_H

echo   [+] Адрес записан. Поменять его можно и на телефоне: Settings.

REM --- 4. Сборка ----------------------------------------------------------
:build
echo.
echo   Собираю. Первый раз это минут десять -- компилятору 2011 года
echo   тяжело даётся даже быстрый компьютер.
echo.

call "%ROOT%depesha\build-symbian.bat"
if errorlevel 1 goto :failed

set "PKG="
for %%f in ("%ROOT%depesha\Depesha-*.sis") do set PKG=%%~nxf
if "!PKG!"=="" goto :failed

copy /y "%ROOT%depesha\!PKG!" "%ROOT%!PKG!" >nul

echo.
echo   ГОТОВО: !PKG!
echo   Лежит рядом с этим файлом.
echo.
echo   Дальше -- на телефон: Средства ^> Дисп. прил. ^> Параметры ^> Настройки:
echo     Установка ПО: Все
echo     Проверка сертификатов в сети: Выкл.
echo   и открыть .sis в файловом менеджере. Подробнее -- depesha\СБОРКА.md.
echo.
pause
exit /b 0

:failed
echo.
echo   [!] Сборка не удалась. Полный вывод компилятора: C:\dpbuild\build.log
echo.
pause
exit /b 1
