@echo off
:: Build script for Coro library using MSVC
:: Requires Visual Studio 2022 or Build Tools

echo ==========================================
echo Coro Library Build Script (MSVC)
echo ==========================================
echo.

:: Set MSVC environment
set "VCToolsInstallDir=C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Tools\MSVC\14.50.35702"
set "WindowsSdkDir=C:\Program Files (x86)\Windows Kits\10"
set "WindowsSdkVersion=10.0.22621.0"

:: Set INCLUDE path
set "INCLUDE=%VCToolsInstallDir%\include;%WindowsSdkDir%\Include\%WindowsSdkVersion%\ucrt;%WindowsSdkDir%\Include\%WindowsSdkVersion%\um;%WindowsSdkDir%\Include\%WindowsSdkVersion%\shared"

:: Set LIB path
set "LIB=%VCToolsInstallDir%\lib\x64;%WindowsSdkDir%\Lib\%WindowsSdkVersion%\ucrt\x64;%WindowsSdkDir%\Lib\%WindowsSdkVersion%\um\x64"

:: Set PATH
set "PATH=%VCToolsInstallDir%\bin\Hostx64\x64;%PATH%"

:: Compiler settings
set "CXXFLAGS=/std:c++20 /await:strict /EHsc /O2 /W4 /Iinclude"
set "LDFLAGS="

:: Create build directory
if not exist build mkdir build
cd build

:: Compile source files
echo Compiling scheduler.cpp...
cl.exe %CXXFLAGS% /c "..\src\scheduler.cpp" /Fo:scheduler.obj
if %ERRORLEVEL% NEQ 0 goto :error

:: Compile examples
echo.
echo Compiling examples...
echo.

echo Building task_demo.exe...
cl.exe %CXXFLAGS% "..\examples\task_demo.cpp" scheduler.obj /Fe:task_demo.exe
if %ERRORLEVEL% NEQ 0 goto :error

echo Building generator_demo.exe...
cl.exe %CXXFLAGS% "..\examples\generator_demo.cpp" /Fe:generator_demo.exe
if %ERRORLEVEL% NEQ 0 goto :error

echo Building echo_server.exe...
cl.exe %CXXFLAGS% "..\examples\echo_server.cpp" scheduler.obj /Fe:echo_server.exe
if %ERRORLEVEL% NEQ 0 goto :error

echo.
echo ==========================================
echo Build successful!
echo ==========================================
echo.
echo Available executables:
echo   - task_demo.exe
echo   - generator_demo.exe
echo   - echo_server.exe
echo.
goto :end

:error
echo.
echo ==========================================
echo Build failed with error %ERRORLEVEL%
echo ==========================================
exit /b 1

:end
cd ..
