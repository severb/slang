add_library(vm OBJECT vm.c)

target_link_libraries(vm
    PRIVATE types
    PUBLIC frontend
)

target_include_directories(vm INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})