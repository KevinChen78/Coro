@echo off
:: Quick compiler test for C++20 coroutines

echo ==========================================
echo Testing C++20 Coroutine Support
echo ==========================================

:: Setup paths
set "ONEAPI_ROOT=D:\Oneapi\compiler\latest"
set "PATH=%ONEAPI_ROOT%\windows\bin;%ONEAPI_ROOT%\windows\bin-llvm;%PATH%"

:: Set Windows SDK
set "WINDOWS_SDK_ROOT=C:\Program Files (x86)\Windows Kits\10"
set "WINDOWS_SDK_VERSION=10.0.22621.0"
set "INCLUDE=%WINDOWS_SDK_ROOT%\Include\%WINDOWS_SDK_VERSION%\ucrt;%WINDOWS_SDK_ROOT%\Include\%WINDOWS_SDK_VERSION%\um;%WINDOWS_SDK_ROOT%\Include\%WINDOWS_SDK_VERSION%\shared"
set "LIB=%WINDOWS_SDK_ROOT%\Lib\%WINDOWS_SDK_VERSION%\ucrt\x64;%WINDOWS_SDK_ROOT%\Lib\%WINDOWS_SDK_VERSION%\um\x64"

echo.
echo Compiler: icx --version
echo ----------------------------------------
icx --version
echo.

:: Compile test
echo Compiling test_compile.cpp...
echo ----------------------------------------
icx -std=c++20 -O2 -o test_compile.exe test_compile.cpp -fcoroutines-ts

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Trying with -fcoroutines...
    icx -std=c++20 -O2 -o test_compile.exe test_compile.cpp -fcoroutines
)

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Trying with /std:c++20 (MSVC style)...
    icx /std:c++20 /O2 /EHsc /Fe:test_compile.exe test_compile.cpp /await
)

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Compilation failed!
    echo Please check compiler installation.
    exit /b 1
)

echo.
echo Compilation successful!
echo.
echo Running test...
echo ----------------------------------------
.	est_compile.exe

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ==========================================
    echo SUCCESS! Your compiler supports C++20 coroutines!
    echo ==========================================
) else (
    echo.
    echo Test execution failed!
    exit /b 1
)
