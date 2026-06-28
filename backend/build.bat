@echo off
setlocal

set "UCRT64=C:\msys64\ucrt64"
set "GCC=%UCRT64%\bin\gcc.exe"
set "PATH=%UCRT64%\bin;%PATH%"

if not exist "%GCC%" (
    echo ERREUR: gcc introuvable dans %UCRT64%\bin
    echo Assure-toi que MSYS2 ucrt64 est bien installe.
    pause
    exit /b 1
)

echo Compilation en cours...

"%GCC%" -Wall -Wextra -std=c11 ^
    "-I%UCRT64%\include" -I.\src ^
    src\main.c src\server.c src\db.c src\ai_client.c src\models.c ^
    -o moodmatch_backend.exe ^
    "-L%UCRT64%\lib" -lmicrohttpd -lsqlite3 -lcurl -lcjson

if errorlevel 1 (
    echo.
    echo ERREUR: la compilation a echoue.
    pause
    exit /b 1
)

echo.
echo OK - moodmatch_backend.exe compile avec succes.
pause
