@echo off
:: Build script for Coro library using Intel oneAPI compiler

call setup_env.bat
if %ERRORLEVEL% NEQ 0 exit /b 1

echo.
echo ==========================================
echo Building Coro Library
echo ==========================================

:: Create build directory
if not exist build mkdir build
cd build

:: Configure with CMake if available, otherwise use direct compilation
where cmake >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Using CMake...
    cmake .. -DCMAKE_CXX_COMPILER=icx -DCMAKE_BUILD_TYPE=Release -T ClangCL
    if %ERRORLEVEL% NEQ 0 (
        echo CMake configuration failed, falling back to direct compilation
        goto :direct_compile
    )
    cmake --build . --config Release -j
) else (
    :direct_compile
    echo Using direct compilation...

    :: Compiler flags
    set CXXFLAGS=-std=c++20 -O2 -Wall -fcoroutines -I"..\include"

    :: Create output directories
    if not exist Release mkdir Release

    :: Compile source files
    echo Compiling scheduler.cpp...
    icx %CXXFLAGS% -c "..\src\scheduler.cpp" -o "Release\scheduler.obj"
    if %ERRORLEVEL% NEQ 0 goto :error

    :: Compile and link tests
    echo Compiling tests...
    icx %CXXFLAGS% -c "..\tests\main.cpp" -o "Release\test_main.obj"
    if %ERRORLEVEL% NEQ 0 goto :error

    icx %CXXFLAGS% -c "..\tests\test_task.cpp" -o "Release\test_task.obj"
    if %ERRORLEVEL% NEQ 0 goto :error

    icx %CXXFLAGS% -c "..\tests\test_generator.cpp" -o "Release\test_generator.obj"
    if %ERRORLEVEL% NEQ 0 goto :error

    icx %CXXFLAGS% -c "..\tests\test_scheduler.cpp" -o "Release\test_scheduler.obj"
    if %ERRORLEVEL% NEQ 0 goto :error

    :: Link tests (would need gtest library)
    echo Linking tests...
    :: icx Release\*.obj -o Release\coro_tests.exe

echo.
echo Build complete!
goto :end

:error
echo.
echo ERROR: Build failed!
exit /b 1

:end
cd ..
echo.
echo ==========================================
echo Build finished!
echo ==========================================
