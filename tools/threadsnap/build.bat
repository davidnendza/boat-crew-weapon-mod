@echo off
rem Builds threadsnap.exe with the Visual Studio C++ build tools (same search as the mod's build.bat).
setlocal
set "VCVARS="
for %%p in (
  "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) do if not defined VCVARS if exist %%p set "VCVARS=%%~p"
if not defined VCVARS (echo No Visual Studio C++ build tools found & exit /b 1)
call "%VCVARS%" >nul 2>nul
cd /d "%~dp0"
cl /nologo /O2 /MT /W3 /EHsc threadsnap.cpp /Fe:threadsnap.exe /link psapi.lib
