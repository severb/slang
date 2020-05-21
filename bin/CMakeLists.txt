add_executable(slang slang.c)
target_link_libraries(slang
    PUBLIC frontend
    PUBLIC vm
    PUBLIC mem
)
