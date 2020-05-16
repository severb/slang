add_library(vm vm.c)

target_link_libraries(vm
    PRIVATE types
    PUBLIC frontend
)

target_include_directories(vm INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})