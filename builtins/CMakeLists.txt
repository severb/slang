add_library(builtins builtins.c)
target_link_libraries(builtins
    PRIVATE mem
    PRIVATE vm
    PUBLIC types
)
target_include_directories(builtins INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
