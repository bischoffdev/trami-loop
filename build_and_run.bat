@echo off
setlocal
cd /d "%~dp0"

set "PATH=%LOCALAPPDATA%\Microsoft\WinGet\Packages\Kitware.CMake_Microsoft.Winget.Source_8wekyb3d8bbwe\cmake-4.4.4-windows-x86_64\bin;%PATH%"

echo ========================================================
echo  trami-loop: CMake Configure
echo ========================================================
cmake -B build -A x64 -DCMAKE_BUILD_TYPE=Release -DTRAMI_BUILD_TESTS=ON
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [FEHLER] CMake-Konfiguration fehlgeschlagen.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================================
echo  trami-loop: Build (Release)
echo ========================================================
cmake --build build --config Release --parallel
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [FEHLER] Kompilierung fehlgeschlagen.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================================
echo  trami-loop: Tests ausfuehren
echo ========================================================
ctest --test-dir build -C Release --output-on-failure

echo.
echo ========================================================
echo  trami-loop: Starte Anwendung
echo ========================================================
if exist "build\src\TramiLoop_artefacts\Release\trami-loop.exe" (
    start "" "build\src\TramiLoop_artefacts\Release\trami-loop.exe"
) else if exist "build\src\Release\trami-loop.exe" (
    start "" "build\src\Release\trami-loop.exe"
) else (
    echo Anwendung wurde kompiliert, aber Pfad zur .exe konnte nicht automatisch gefunden werden.
    pause
)
