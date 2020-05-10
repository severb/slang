add_library(types OBJECT dynarray.c list.c str.c table.c tag.c)
target_link_libraries(types PRIVATE mem)
target_include_directories(types INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
