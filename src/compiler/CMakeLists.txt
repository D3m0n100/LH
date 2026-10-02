# compiler 模块：封装 Python DSL 编译器

add_library(compiler_module STATIC
    DSLCompilerInterface.cpp
    DSLCompilerInput.cpp
    DSLCompilerArtifacts.cpp
    DSLCompilerAsync.cpp
)

target_link_libraries(compiler_module
    Qt5::Core
    download_profile_contract
)

target_include_directories(compiler_module PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> $<INSTALL_INTERFACE:include/compiler>
    ${LX_PUBLIC_INCLUDE_DIRS}
)

target_include_directories(compiler_module PRIVATE ${LX_PRIVATE_INCLUDE_DIRS})
