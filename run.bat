@echo off
setlocal

REM OPTIONAL: Remove build_game to reset all cached options
if exist build_game (
    echo Removing build_game to reset CMake options...
    rmdir /S /Q build_game
)

REM OPTIONAL: Remove build to reset all cached options
if exist build (
    echo Removing build to reset CMake options...
    rmdir /S /Q build
)
mkdir build

pushd build
cmake -DSOFASPUDS_ENABLE_EDITOR=ON ..
popd

endlocal
pause
