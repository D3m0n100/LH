# compiler 模块：封装 Python DSL 编译器

add_library(compiler_module STATIC
    DSLCompilerInterface.cpp
)

target_link_libraries(compiler_module
    Qt5::Core
)

target_include_directories(compiler_module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
