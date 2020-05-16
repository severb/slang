add_library(frontend bytecode.c compiler.c lex.c)
target_link_libraries(frontend PUBLIC types PRIVATE mem)
target_include_directories(frontend INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
