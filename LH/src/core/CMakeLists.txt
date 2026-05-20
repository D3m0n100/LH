# Core 模块构建配置

add_library(core_module STATIC
    TaskScheduler.h
    TaskScheduler.cpp
    DataManager.h
    DataManager.cpp
)

target_link_libraries(core_module
    Qt5::Core
    Qt5::Sql
)

target_include_directories(core_module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(core_module PUBLIC cxx_std_17)

# 预编译头（可选）
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
    target_precompile_headers(core_module PRIVATE
        <QObject>
        <QString>
        <QDateTime>
        <QMutex>
        <QTimer>
        <QSqlDatabase>
        <QSqlQuery>
        <functional>
        <memory>
    )
endif()
