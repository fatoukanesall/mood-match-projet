@echo off
setlocal enabledelayedexpansion

rem DLLs MSYS2 necessaires pour le serveur
set "PATH=C:\msys64\ucrt64\bin;%PATH%"

rem Charge la cle API depuis config.bat si elle existe
if exist config.bat call config.bat

rem Si la cle n'est toujours pas definie, la demander UNE SEULE FOIS et la sauvegarder
if "%GROQ_API_KEY%"=="" (
    echo.
    echo ============================================================
    echo  Cle API Groq non trouvee.
    echo  Recupere-la sur : https://console.groq.com/keys
    echo ============================================================
    echo.
    set /p GROQ_API_KEY="Colle ta cle API Groq ici : "
    echo set "GROQ_API_KEY=!GROQ_API_KEY!" > config.bat
    echo.
    echo Cle sauvegardee dans config.bat - elle sera chargee automatiquement au prochain lancement.
    echo.
)

if not exist moodmatch_backend.exe (
    echo ERREUR: moodmatch_backend.exe introuvable.
    echo Lance d'abord build.bat pour compiler le projet.
    pause
    exit /b 1
)

echo.
echo Serveur MoodMatch demarre sur http://localhost:8080
echo Ouvre frontend\index.html dans ton navigateur.
echo Appuie sur CTRL+C pour arreter.
echo.

moodmatch_backend.exe
