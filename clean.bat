@echo off
setlocal

REM Remove build directory in the root
if exist build (
    echo Removing CMakeCache file...
    del /F /Q "build\CMakeCache.txt"
    rmdir /S /Q build
)

REM Remove build directory in game
pushd game >nul
if exist build (
    echo Removing build directory...
    rmdir /S /Q build
)
popd >nul

endlocal