# 文件：src/monitor/CMakeLists.txt
# 监控模块 CMake 配置（性能优化版本）

set(MONITOR_SOURCES
    # 核心类型
    MonitorTypes.h
    RingBuffer.h
    
    # 数据处理器（性能优化核心）
    MonitorDataProcessor.h
    MonitorDataProcessor.cpp
    
    # 通道管理
    MonitorChannel.h
    MonitorChannel.cpp
    MonitorManager.h
    MonitorManager.cpp

    # 监控采样落库（新增）
    MonitorDataLogger.h
    MonitorDataLogger.cpp
    
    # 图表控件（增量渲染优化）
    ChartWidget.h
    ChartWidget.cpp
    MonitorChartView.h
    MonitorChartView.cpp
    
    # 主界面控件
    MonitorWidget.h
    MonitorWidget.cpp
    
    # 采样数据提供者
    SampleDataProvider.h
    SampleDataProvider.cpp
    
    # 导出助手（新增）
    MonitorExportHelper.h
    MonitorExportHelper.cpp
)

# 创建监控模块库
add_library(monitor STATIC ${MONITOR_SOURCES})

# 链接依赖
target_link_libraries(monitor
    PUBLIC
        core_module
        Qt5::Core
        Qt5::Widgets
        Qt5::Charts
        Qt5::Svg
)

# 包含路径
target_include_directories(monitor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# C++17 支持（用于 std::optional）
target_compile_features(monitor PUBLIC cxx_std_17)

# 编译选项
if(MSVC)
    target_compile_options(monitor PRIVATE /W4)
else()
    target_compile_options(monitor PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 性能优化编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        target_compile_options(monitor PRIVATE /O2)
    else()
        target_compile_options(monitor PRIVATE -O3 -march=native)
    endif()
endif()
