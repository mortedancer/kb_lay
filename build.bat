@echo off
setlocal
cd /d "%~dp0"

where gcc >nul 2>&1
if %errorlevel%==0 goto :gcc

where cl >nul 2>&1
if %errorlevel%==0 goto :msvc

echo No gcc or cl in PATH.
exit /b 1

:gcc
gcc -finput-charset=UTF-8 -DKB_LAY_TEST -O2 -o kb_lay_test.exe src\kb_lay.c
if errorlevel 1 exit /b 1
kb_lay_test.exe
if errorlevel 1 exit /b 1
gcc -finput-charset=UTF-8 -O2 -s -mwindows -static -o kb_lay.exe src\kb_lay.c -luser32 -lshell32 -ladvapi32
if errorlevel 1 exit /b 1
echo built kb_lay.exe
exit /b 0

:msvc
cl /nologo /utf-8 /O1 /W3 /DKB_LAY_TEST src\kb_lay.c /Fe:kb_lay_test.exe
if errorlevel 1 exit /b 1
kb_lay_test.exe
if errorlevel 1 exit /b 1
cl /nologo /utf-8 /O1 /W3 src\kb_lay.c /Fe:kb_lay.exe user32.lib shell32.lib advapi32.lib /link /SUBSYSTEM:WINDOWS
if errorlevel 1 exit /b 1
echo built kb_lay.exe
exit /b 0
