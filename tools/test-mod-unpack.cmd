@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c11 /W3 /Od /Zi /fsanitize=address tools\mod-unpack-test.c /Fe:.tmp\mod-unpack-test.exe /Fo:.tmp\mod-unpack-test.obj /Fd:.tmp\mod-unpack-test.pdb
if errorlevel 1 exit /b 1
.tmp\mod-unpack-test.exe
