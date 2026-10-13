@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c11 /W2 /Od /Zi /fsanitize=address tools\land-pursuer-host-test.c /Fe:.tmp\pursuer-test.exe /Fo:.tmp\pursuer-test.obj /Fd:.tmp\pursuer-test.pdb
if errorlevel 1 exit /b 1
.tmp\pursuer-test.exe
