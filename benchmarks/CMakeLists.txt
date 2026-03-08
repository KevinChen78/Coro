# Benchmarks CMakeLists.txt

add_executable(coro_benchmark
    benchmark_main.cpp
    task_bench.cpp
    generator_bench.cpp
)

target_link_libraries(coro_benchmark
    PRIVATE
    coro::coro
    benchmark::benchmark
    benchmark::benchmark_main
)

# Compiler optimizations for benchmarks
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(coro_benchmark PRIVATE -O3 -march=native)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(coro_benchmark PRIVATE /O2 /arch:AVX2)
endif()
