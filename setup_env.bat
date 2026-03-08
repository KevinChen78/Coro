@echo off
:: Coro Project - Compiler Environment Setup
:: Intel oneAPI + Windows SDK

echo ==========================================
echo Setting up C++20 Coroutine Build Environment
echo ==========================================

:: Set Intel oneAPI compiler path
set "ONEAPI_ROOT=D:\Oneapi\compiler\latest"
set "PATH=%ONEAPI_ROOT%\windows\bin;%ONEAPI_ROOT%\windows\bin-llvm;%PATH%"

:: Set Windows SDK
set "WINDOWS_SDK_VERSION=10.0.22621.0"
set "WINDOWS_SDK_ROOT=C:\Program Files (x86)\Windows Kits\10"

:: Set MSVC environment (using Intel's bundled MSVC headers if available)
:: Or use standalone MSVC Build Tools
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo Warning: MSVC vcvars not found. Using Intel compiler with Windows SDK only.

    :: Set Windows SDK paths
    set "INCLUDE=%WINDOWS_SDK_ROOT%\Include\%WINDOWS_SDK_VERSION%\ucrt;%WINDOWS_SDK_ROOT%\Include\%WINDOWS_SDK_VERSION%\um;%WINDOWS_SDK_ROOT%\Include\%WINDOWS_SDK_VERSION%\shared;%INCLUDE%"
    set "LIB=%WINDOWS_SDK_ROOT%\Lib\%WINDOWS_SDK_VERSION%\ucrt\x64;%WINDOWS_SDK_ROOT%\Lib\%WINDOWS_SDK_VERSION%\um\x64;%LIB%"
)

:: Verify compiler
echo.
echo Checking compiler...
icx --version

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Intel compiler not found!
    exit /b 1
)

echo.
echo ==========================================
echo Environment ready for C++20 Coroutines!
echo ==========================================
echo.
echo Available commands:
echo   icx --version    : Check compiler version
echo   build.bat        : Build the project
echo   test.bat         : Run tests
echo.
