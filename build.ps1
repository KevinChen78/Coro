# PowerShell build script for Coro library

# Stop on error
$ErrorActionPreference = "Stop"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Coro Library Build Script (MSVC)" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

# Set MSVC environment
$env:VCToolsInstallDir = "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Tools\MSVC\14.50.35702"
$env:WindowsSdkDir = "C:\Program Files (x86)\Windows Kits\10"
$env:WindowsSdkVersion = "10.0.22621.0"

# Set INCLUDE path
$env:INCLUDE = "$env:VCToolsInstallDir\include;$env:WindowsSdkDir\Include\$env:WindowsSdkVersion\ucrt;$env:WindowsSdkDir\Include\$env:WindowsSdkVersion\um;$env:WindowsSdkDir\Include\$env:WindowsSdkVersion\shared"

# Set LIB path
$env:LIB = "$env:VCToolsInstallDir\lib\x64;$env:WindowsSdkDir\Lib\$env:WindowsSdkVersion\ucrt\x64;$env:WindowsSdkDir\Lib\$env:WindowsSdkVersion\um\x64"

# Set PATH
$env:PATH = "$env:VCToolsInstallDir\bin\Hostx64\x64;$env:PATH"

# Get absolute paths
$projectRoot = Resolve-Path "..\coro"

# Compiler settings
$CXXFLAGS = @("/std:c++20", "/await:strict", "/EHsc", "/O2", "/W4", "/I$projectRoot\include")

# Create build directory
if (!(Test-Path "build")) {
    New-Item -ItemType Directory -Name "build" | Out-Null
}
Set-Location "build"

# Compile source files
Write-Host "Compiling source files..." -ForegroundColor Yellow

& cl.exe @CXXFLAGS /c "..\src\scheduler.cpp" /Fo:scheduler.obj
if ($LASTEXITCODE -ne 0) { throw "scheduler.cpp compilation failed" }

& cl.exe @CXXFLAGS /c "..\src\thread_pool.cpp" /Fo:thread_pool.obj
if ($LASTEXITCODE -ne 0) { throw "thread_pool.cpp compilation failed" }

# Compile examples
Write-Host ""
Write-Host "Compiling examples..." -ForegroundColor Yellow
Write-Host ""

Write-Host "Building task_demo.exe..." -ForegroundColor Green
& cl.exe @CXXFLAGS "..\examples\task_demo.cpp" scheduler.obj /Fe:task_demo.exe
if ($LASTEXITCODE -ne 0) { throw "task_demo.cpp compilation failed" }

Write-Host "Building generator_demo.exe..." -ForegroundColor Green
& cl.exe @CXXFLAGS "..\examples\generator_demo.cpp" /Fe:generator_demo.exe
if ($LASTEXITCODE -ne 0) { throw "generator_demo.cpp compilation failed" }

Write-Host "Building echo_server.exe..." -ForegroundColor Green
& cl.exe @CXXFLAGS "..\examples\echo_server.cpp" scheduler.obj /Fe:echo_server.exe
if ($LASTEXITCODE -ne 0) { throw "echo_server.cpp compilation failed" }

Write-Host "Building thread_pool_demo.exe..." -ForegroundColor Green
& cl.exe @CXXFLAGS "..\examples\thread_pool_demo.cpp" scheduler.obj thread_pool.obj /Fe:thread_pool_demo.exe
if ($LASTEXITCODE -ne 0) { throw "thread_pool_demo.cpp compilation failed" }

Write-Host "Building sync_primitives_demo.exe..." -ForegroundColor Green
& cl.exe @CXXFLAGS "..\examples\sync_primitives_demo.cpp" scheduler.obj thread_pool.obj /Fe:sync_primitives_demo.exe
if ($LASTEXITCODE -ne 0) { throw "sync_primitives_demo.cpp compilation failed" }

Write-Host "Building http_server_demo.exe..." -ForegroundColor Green
& cl.exe @CXXFLAGS "..\examples\http_server_demo.cpp" scheduler.obj thread_pool.obj /Fe:http_server_demo.exe
if ($LASTEXITCODE -ne 0) { throw "http_server_demo.cpp compilation failed" }

Write-Host "Building memory_pool_demo.exe..." -ForegroundColor Green
& cl.exe @CXXFLAGS "..\examples\memory_pool_demo.cpp" scheduler.obj thread_pool.obj /Fe:memory_pool_demo.exe
if ($LASTEXITCODE -ne 0) { throw "memory_pool_demo.cpp compilation failed" }

Write-Host "Building memory_bench.exe..." -ForegroundColor Green
& cl.exe @CXXFLAGS "..\benchmarks\memory_bench.cpp" scheduler.obj thread_pool.obj /Fe:memory_bench.exe
if ($LASTEXITCODE -ne 0) { throw "memory_bench.cpp compilation failed" }

Write-Host ""
Write-Host "==========================================" -ForegroundColor Green
Write-Host "Build successful!" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Available executables:" -ForegroundColor White
Write-Host "  - task_demo.exe"
Write-Host "  - generator_demo.exe"
Write-Host "  - echo_server.exe"
Write-Host "  - thread_pool_demo.exe"
Write-Host "  - sync_primitives_demo.exe"
Write-Host "  - http_server_demo.exe"
Write-Host "  - memory_pool_demo.exe"
Write-Host "  - memory_bench.exe"
Write-Host ""

Set-Location ".."
