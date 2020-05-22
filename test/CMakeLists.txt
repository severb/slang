add_executable(tag tag.c)
target_link_libraries(tag PUBLIC types)

add_executable(dynarray dynarray.c)
target_link_libraries(dynarray PUBLIC types mem)

add_executable(table_bench table_bench.c)
target_link_libraries(table_bench PUBLIC types mem)

add_executable(table_fuzz table_fuzz.c)
target_link_libraries(table_fuzz PUBLIC types mem)