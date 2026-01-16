@echo off
setlocal

echo === Cleaning build_game (editor OFF) ===
if exist build_game (
    rmdir /S /Q build_game
)
Nuke build to reset all cached options
if exist build (
    echo Removing build to reset CMake options...
    rmdir /S /Q build
)

mkdir build_game

echo === Running CMake with editor DISABLED ===
pushd build_game
cmake -DSOFASPUDS_ENABLE_EDITOR=OFF ..
popd

echo === Done. Open build_game/MyEngine.sln and build. ===
pause
endlocal
