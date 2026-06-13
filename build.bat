@echo off
REM cloc-c build script for MSVC
REM Usage: build.bat          (release build)
REM        build.bat debug    (debug build)
REM        build.bat clean    (clean build artifacts)

setlocal

set SRC=src\main.c src\scanner.c src\counter.c src\language.c src\output.c src\thread.c src\utils.c src\os_win32.c
set OUT=clocc.exe
set INCLUDES=/Iinclude
set LIBS=kernel32.lib

if "%1"=="clean" (
    if exist clocc.exe del clocc.exe
    if exist *.obj del *.obj
    echo Cleaned.
    goto :eof
)

if "%1"=="debug" (
    set CFLAGS=/std:c11 /Zi /Od /W4 /DDEBUG %INCLUDES%
) else (
    set CFLAGS=/std:c11 /O2 /W4 /DNDEBUG %INCLUDES%
)

echo Building cloc-c...
cl.exe %CFLAGS% /Fe%OUT% %SRC% /link %LIBS%

if %ERRORLEVEL%==0 (
    echo Build successful: %OUT%
) else (
    echo Build failed.
)

endlocal
