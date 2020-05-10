cmake_minimum_required(VERSION 3.17)
project(slang VERSION 0.1)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(types)
add_subdirectory(frontend)

add_library(mem OBJECT mem.c)
target_include_directories(mem INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(slang bin/slang.c)
target_link_libraries(slang PUBLIC types)
target_link_libraries(slang PUBLIC frontend)
target_link_libraries(slang PUBLIC mem)
