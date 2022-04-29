@echo off

rem script to build StartWaitTree on Windows 2000
rem using Visual C++ 2005 and the Windows 2000 Platform SDK

rem set environment variables
if "%VcVarsSet%" == "1" goto envdone
set PATH=%PATH%;%ProgramFiles%\Microsoft Visual Studio 8\Common7\IDE;%ProgramFiles%\Microsoft Visual Studio 8\VC\bin
set INCLUDE=%ProgramFiles%\Microsoft Visual Studio 8\VC\include;%ProgramFiles%\Microsoft Platform SDK\Include
set LIB=%ProgramFiles%\Microsoft Visual Studio 8\VC\lib;%ProgramFiles%\Microsoft Platform SDK\Lib
set VcVarsSet=1
:envdone

rem generate .rc file for manifest
echo #define RT_MANIFEST 24 >.\StartWaitTree.rc
echo #define CREATEPROCESS_MANIFEST_RESOURCE_ID 1 >>.\StartWaitTree.rc
echo CREATEPROCESS_MANIFEST_RESOURCE_ID RT_MANIFEST "StartWaitTree.manifest" >>.\StartWaitTree.rc

rem compile .rc file to .res
rc.exe StartWaitTree.rc

rem compile
cl.exe /nologo /c /Fo.\StartWaitTree.obj /DNOSTDBOOL /DWINVER=0x0500 /D_WIN32_WINNT=0x0500 .\StartWaitTree.c

rem link
link.exe /nologo /nodefaultlib /subsystem:console /entry:noCrtMain /out:.\StartWaitTree.exe .\StartWaitTree.obj .\StartWaitTree.res kernel32.lib
