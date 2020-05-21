add_executable(test_tag tag.c)
target_link_libraries(test_tag PUBLIC types)

add_executable(test_dynarray dynarray.c)
target_link_libraries(test_dynarray PUBLIC types mem)