# Examples CMakeLists.txt

add_executable(echo_server echo_server.cpp)
target_link_libraries(echo_server PRIVATE coro::coro)

add_executable(generator_demo generator_demo.cpp)
target_link_libraries(generator_demo PRIVATE coro::coro)

add_executable(task_demo task_demo.cpp)
target_link_libraries(task_demo PRIVATE coro::coro)
