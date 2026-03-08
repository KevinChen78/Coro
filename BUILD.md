# 构建指南

## 环境要求

- **操作系统**: Windows 10/11
- **编译器**: MSVC 2022 (C++20 支持)
- **已测试版本**: Visual Studio 2022 17.8+ (Insiders)

## 快速开始

### 1. 自动构建（推荐）

打开 PowerShell，在项目根目录运行：

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```

### 2. 手动构建

如果需要手动编译，设置以下环境变量：

```powershell
# MSVC 路径
$env:VCToolsInstallDir = "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Tools\MSVC\14.50.35702"
$env:WindowsSdkDir = "C:\Program Files (x86)\Windows Kits\10"
$env:WindowsSdkVersion = "10.0.22621.0"

# 包含路径
$env:INCLUDE = "$env:VCToolsInstallDir\include;$env:WindowsSdkDir\Include\$env:WindowsSdkVersion\ucrt;$env:WindowsSdkDir\Include\$env:WindowsSdkVersion\um;$env:WindowsSdkDir\Include\$env:WindowsSdkVersion\shared"

# 库路径
$env:LIB = "$env:VCToolsInstallDir\lib\x64;$env:WindowsSdkDir\Lib\$env:WindowsSdkVersion\ucrt\x64;$env:WindowsSdkDir\Lib\$env:WindowsSdkVersion\um\x64"

# 编译器路径
$env:PATH = "$env:VCToolsInstallDir\bin\Hostx64\x64;$env:PATH"
```

编译命令：

```powershell
# 创建构建目录
mkdir build
cd build

# 编译选项
$CXXFLAGS = @("/std:c++20", "/await:strict", "/EHsc", "/O2", "/W4", "/I..\include")

# 编译 scheduler.cpp
cl.exe @CXXFLAGS /c "..\src\scheduler.cpp" /Fo:scheduler.obj

# 编译示例
cl.exe @CXXFLAGS "..\examples\task_demo.cpp" scheduler.obj /Fe:task_demo.exe
cl.exe @CXXFLAGS "..\examples\generator_demo.cpp" /Fe:generator_demo.exe
cl.exe @CXXFLAGS "..\examples\echo_server.cpp" scheduler.obj /Fe:echo_server.exe
```

### 3. 运行示例

```powershell
.\task_demo.exe
.\generator_demo.exe
.\echo_server.exe
```

## 编译器配置说明

### C++20 协程支持

MSVC 需要 `/await:strict` 标志来启用标准 C++20 协程（而不是遗留的实验性协程）：

```
/std:c++20    - C++20 标准
/await:strict  - 启用标准 C++20 协程
/EHsc          - 启用 C++ 异常处理
```

### 常见问题

1. **找不到头文件**: 确保 `INCLUDE` 环境变量包含 MSVC 和 Windows SDK 路径
2. **协程类型未定义**: 必须使用 `/await:strict` 而不是 `/await`
3. **链接错误**: 确保 `LIB` 环境变量正确设置

## 故障排除

### 检查编译器版本

```powershell
cl.exe
```

预期输出：
```
Microsoft (R) C/C++ Optimizing Compiler Version 19.50.35702 for x64
```

### 检查环境变量

```powershell
echo $env:INCLUDE
echo $env:LIB
```

### 查找 MSVC 安装位置

```powershell
# Visual Studio 2022
ls "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\"

# Visual Studio 2019
ls "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\"
```

## 下一步

- 运行测试：`cd tests && ./run_tests.ps1` (待实现)
- 运行基准测试：`cd benchmarks && ./run_benchmarks.ps1` (待实现)
- 继续开发 Phase 2：线程池调度器
