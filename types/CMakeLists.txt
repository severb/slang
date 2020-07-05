add_library(types dynarray.c list.c str.c table.c tag.c fun.c)
target_link_libraries(types
    PUBLIC mem
    PRIVATE safemath
)
target_include_directories(types INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
