# Tests CMakeLists.txt

enable_testing()

# Test executable
add_executable(coro_tests
    test_task.cpp
    test_generator.cpp
    test_scheduler.cpp
    main.cpp
)

target_link_libraries(coro_tests
    PRIVATE
    coro::coro
    GTest::gtest
    GTest::gtest_main
)

# Add tests to CTest
add_test(NAME coro_unit_tests COMMAND coro_tests)

# Compiler-specific options for tests
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(coro_tests PRIVATE -g -O0 --coverage)
    target_link_options(coro_tests PRIVATE --coverage)
endif()
