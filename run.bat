@echo off

REM CHECK IF 'build' FOLDER EXISTS; CREATE IF IT DOES NOT
if not exist build (
    mkdir build
)

REM BUILDING DEBUG
pushd build
cmake ..
popd

pause