cmake_minimum_required(VERSION 3.17)
project(slang VERSION 0.1)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()
if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DSLANG_DEBUG)
endif()

if (MSVC)
    # warning level 4 and all warnings as errors
    add_compile_options(/W4 /WX)
else()
    # lots of warnings and all warnings as errors
    add_compile_options(
        -Wall -Wextra -pedantic -Wno-missing-braces
        $<$<CONFIG:DEBUG>:-g>
        $<$<CONFIG:RELEASE>:-O3>
    )
endif()

add_subdirectory(types)
add_subdirectory(builtins)
add_subdirectory(frontend)
add_subdirectory(vm)
add_subdirectory(bin)
add_subdirectory(test)

add_library(safemath safemath.c)
target_include_directories(safemath INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
option(FAST_OVERFLOW "compile safemath with fast overflow extension checks" ON)
if(FAST_OVERFLOW)
    target_compile_definitions(safemath PUBLIC SLANG_FAST_OVERFLOW)
endif(FAST_OVERFLOW)

add_library(mem mem.c)
target_include_directories(mem INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mem PUBLIC safemath)
option(COREDUMP "don't abort execution on memory errors to cause a coredump" OFF)
if(COREDUMP)
    target_compile_definitions(mem PRIVATE SLANG_COREDUMP)
endif(COREDUMP)

option(WASM "set extra flags for Emscripten output" OFF)
if(WASM)
    # --pre-js is relative to where the output file is
    set_target_properties(slang PROPERTIES COMPILE_FLAGS
        "-s EXPORTED_FUNCTIONS=\"['_run', '_disassamble']\" -s EXTRA_EXPORTED_RUNTIME_METHODS=\"['ccall']\" --pre-js ../../web/src/pre.js"
    )
    set_target_properties(slang PROPERTIES LINK_FLAGS
        "-s EXPORTED_FUNCTIONS=\"['_run', '_disassamble']\" -s EXTRA_EXPORTED_RUNTIME_METHODS=\"['ccall']\" --pre-js ../../web/src/pre.js"
    )
endif(WASM)
