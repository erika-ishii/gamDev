@echo off
setlocal

echo ================================
echo   Cleaning CMake build folders
echo ================================
echo.

REM ------------------------------------------
REM Remove build and build_game in the ROOT
REM ------------------------------------------

if exist build (
    echo Removing root\build ...
    del /F /Q "build\CMakeCache.txt" 2>nul
    rmdir /S /Q build
)

if exist build_game (
    echo Removing root\build_game ...
    rmdir /S /Q build_game
)

REM ------------------------------------------
REM Remove build and build_game inside /game
REM ------------------------------------------

pushd game >nul

if exist build (
    echo Removing game\build ...
    rmdir /S /Q build
)

if exist build_game (
    echo Removing game\build_game ...
    rmdir /S /Q build_game
)

popd >nul

echo.
echo Done!
echo.

endlocal
pause
