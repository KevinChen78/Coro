# PowerShell script to setup build environment

# MSVC paths
$env:VCToolsInstallDir = "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Tools\MSVC\14.50.35702"
$env:WindowsSdkDir = "C:\Program Files (x86)\Windows Kits\10"
$env:WindowsSdkVersion = "10.0.22621.0"

# Set INCLUDE
$env:INCLUDE = "$env:VCToolsInstallDir\include;$env:WindowsSdkDir\Include\$env:WindowsSdkVersion\ucrt;$env:WindowsSdkDir\Include\$env:WindowsSdkVersion\um;$env:WindowsSdkDir\Include\$env:WindowsSdkVersion\shared"

# Set LIB
$env:LIB = "$env:VCToolsInstallDir\lib\x64;$env:WindowsSdkDir\Lib\$env:WindowsSdkVersion\ucrt\x64;$env:WindowsSdkDir\Lib\$env:WindowsSdkVersion\um\x64"

# Set PATH
$env:PATH = "$env:VCToolsInstallDir\bin\Hostx64\x64;$env:PATH"

Write-Host "Environment configured for MSVC C++20!" -ForegroundColor Green
Write-Host "INCLUDE: $env:INCLUDE"
Write-Host "LIB: $env:LIB"

# Test compilation
Write-Host "`nTesting compilation..." -ForegroundColor Yellow

$compileArgs = @(
    "/std:c++20",
    "/await",
    "/O2",
    "/EHsc",
    "/Fe:test_compile.exe",
    "test_compile.cpp"
)

& cl.exe @compileArgs

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nCompilation successful! Running test..." -ForegroundColor Green
    & .\test_compile.exe
} else {
    Write-Host "`nCompilation failed!" -ForegroundColor Red
}
