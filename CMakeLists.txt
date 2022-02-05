cmake_minimum_required(VERSION 3.11)
project(snake_cpp)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -g -O0 -Wl,--stack=64000000")

if (ONLINE_JUDGE)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif ()

add_executable(main main.cpp)

if (WIN32)
    target_link_libraries(main PUBLIC ws2_32)
endif ()