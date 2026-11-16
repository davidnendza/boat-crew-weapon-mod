@echo off
rem Builds and runs tools\out\crew_report_test.exe (the prose after-action report renderer, src\crew_report.h).
rem Run from a plain command prompt or PowerShell:   .\tools\build_report_test.bat
setlocal
set "VCVARS="
for %%p in (
  "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
) do if not defined VCVARS if exist %%p set "VCVARS=%%~p"
if not defined VCVARS goto novc
call "%VCVARS%" >nul 2>nul
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /EHsc /W3 /std:c++17 crew_report_test.cpp /Fo:out\ /Fe:out\crew_report_test.exe
if errorlevel 1 exit /b 1
out\crew_report_test.exe
exit /b %errorlevel%

:novc
echo No Visual Studio C++ build tools found. Install "Build Tools for Visual Studio" with the
echo "Desktop development with C++" workload, or edit the paths at the top of this file.
exit /b 1
