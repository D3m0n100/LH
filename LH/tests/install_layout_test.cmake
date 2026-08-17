if(NOT DEFINED LH_INSTALL_BINARY_DIR OR "${LH_INSTALL_BINARY_DIR}" STREQUAL "")
    message(FATAL_ERROR "安装布局测试必须提供非空的构建目录。")
endif()

if(NOT IS_DIRECTORY "${LH_INSTALL_BINARY_DIR}")
    message(FATAL_ERROR
        "安装布局测试的构建目录不存在或不是目录: ${LH_INSTALL_BINARY_DIR}"
    )
endif()

get_filename_component(_lh_build_dir "${LH_INSTALL_BINARY_DIR}" REALPATH)
file(TO_CMAKE_PATH "${_lh_build_dir}" _lh_build_dir)
if(_lh_build_dir STREQUAL "/" OR _lh_build_dir MATCHES "^[A-Za-z]:/$")
    message(FATAL_ERROR "安装布局测试拒绝将文件系统根目录作为构建目录: ${_lh_build_dir}")
endif()

if(NOT DEFINED LH_INSTALL_PREFIX OR "${LH_INSTALL_PREFIX}" STREQUAL "")
    message(FATAL_ERROR "安装布局测试必须提供非空的测试安装前缀。")
endif()
if(NOT DEFINED LH_INSTALL_EXECUTABLE_NAME OR "${LH_INSTALL_EXECUTABLE_NAME}" STREQUAL "")
    message(FATAL_ERROR "安装布局测试必须提供目标生成的安装可执行文件名。")
endif()
if(DEFINED LH_INSTALL_CONFIG)
    set(_lh_install_config "${LH_INSTALL_CONFIG}")
else()
    set(_lh_install_config "")
endif()
string(STRIP "${_lh_install_config}" _lh_install_config)
string(TOLOWER "${_lh_install_config}" _lh_install_config_lower)
if(_lh_install_config_lower STREQUAL "debug")
    set(_lh_qt_deploy_mode "debug")
    set(_lh_qt_plugin_suffix "d")
else()
    set(_lh_qt_deploy_mode "release")
    set(_lh_qt_plugin_suffix "")
endif()
if(_lh_install_config STREQUAL "")
    set(_lh_install_config_label "<empty/default-release>")
else()
    set(_lh_install_config_label "${_lh_install_config}")
endif()

get_filename_component(_lh_prefix_abs "${LH_INSTALL_PREFIX}" ABSOLUTE)
file(TO_CMAKE_PATH "${_lh_prefix_abs}" _lh_prefix_abs)
if(_lh_prefix_abs STREQUAL "/" OR _lh_prefix_abs MATCHES "^[A-Za-z]:/$")
    message(FATAL_ERROR "安装布局测试拒绝删除文件系统根目录: ${_lh_prefix_abs}")
endif()

set(_lh_build_compare "${_lh_build_dir}")
set(_lh_prefix_compare "${_lh_prefix_abs}")
if(WIN32)
    string(TOLOWER "${_lh_build_compare}" _lh_build_compare)
    string(TOLOWER "${_lh_prefix_compare}" _lh_prefix_compare)
endif()
if(_lh_prefix_compare STREQUAL _lh_build_compare)
    message(FATAL_ERROR
        "安装布局测试拒绝删除构建目录本身；必须使用专用测试前缀: ${_lh_prefix_abs}"
    )
endif()

get_filename_component(_lh_prefix_name "${_lh_prefix_abs}" NAME)
string(TOLOWER "${_lh_prefix_name}" _lh_prefix_name_lower)
if(NOT _lh_prefix_name_lower STREQUAL "install_layout_test_prefix")
    message(FATAL_ERROR
        "安装布局测试前缀必须是专用目录 install_layout_test_prefix: ${_lh_prefix_abs}"
    )
endif()

get_filename_component(_lh_prefix_parent "${_lh_prefix_abs}" DIRECTORY)
if(NOT IS_DIRECTORY "${_lh_prefix_parent}")
    message(FATAL_ERROR
        "安装布局测试前缀的父目录不存在或不是目录: ${_lh_prefix_parent}"
    )
endif()
get_filename_component(_lh_prefix_parent_real "${_lh_prefix_parent}" REALPATH)
file(TO_CMAKE_PATH "${_lh_prefix_parent_real}" _lh_prefix_parent_real)
set(_lh_parent_compare "${_lh_prefix_parent_real}")
if(WIN32)
    string(TOLOWER "${_lh_parent_compare}" _lh_parent_compare)
endif()
if(NOT _lh_parent_compare STREQUAL _lh_build_compare)
    message(FATAL_ERROR
        "安装布局测试前缀必须直接位于构建目录内，拒绝目录外或 ../ 逃逸路径: ${_lh_prefix_abs}"
    )
endif()

set(_lh_install_prefix "${_lh_build_dir}/install_layout_test_prefix")
if(IS_SYMLINK "${_lh_install_prefix}")
    message(FATAL_ERROR
        "安装布局测试拒绝删除符号链接测试前缀: ${_lh_install_prefix}"
    )
endif()
if(EXISTS "${_lh_install_prefix}" AND NOT IS_DIRECTORY "${_lh_install_prefix}")
    message(FATAL_ERROR
        "安装布局测试前缀已存在但不是目录: ${_lh_install_prefix}"
    )
endif()

file(REMOVE_RECURSE "${_lh_install_prefix}")

set(_lh_install_command
    "${CMAKE_COMMAND}"
    --install "${_lh_build_dir}"
    --prefix "${_lh_install_prefix}"
)
if(NOT "${_lh_install_config}" STREQUAL "")
    list(APPEND _lh_install_command --config "${_lh_install_config}")
endif()

execute_process(
    COMMAND ${_lh_install_command}
    RESULT_VARIABLE _lh_install_result
    OUTPUT_VARIABLE _lh_install_stdout
    ERROR_VARIABLE _lh_install_stderr
)
if(NOT _lh_install_result EQUAL 0)
    message(FATAL_ERROR
        "安装布局测试执行 cmake --install 失败（退出码 ${_lh_install_result}）。\n"
        "stdout:\n${_lh_install_stdout}\n"
        "stderr:\n${_lh_install_stderr}"
    )
endif()

set(_lh_installed_executable "${_lh_install_prefix}/bin/${LH_INSTALL_EXECUTABLE_NAME}")
if(NOT EXISTS "${_lh_installed_executable}" OR IS_DIRECTORY "${_lh_installed_executable}")
    message(FATAL_ERROR
        "安装布局缺少已安装主程序文件: ${_lh_installed_executable}"
    )
endif()

set(_lh_runtime_root "${_lh_install_prefix}/third_party/custom_dsp_language/compile")
set(_lh_required_files
    "${_lh_runtime_root}/lmc.py"
    "${_lh_runtime_root}/requirements.txt"
    "${_lh_runtime_root}/grammar/LHLexer.py"
    "${_lh_runtime_root}/grammar/LHParser.py"
    "${_lh_runtime_root}/grammar/LHListener.py"
    "${_lh_runtime_root}/grammar/LHVisitor.py"
    "${_lh_runtime_root}/src/__init__.py"
    "${_lh_runtime_root}/src/lh_compiler/__init__.py"
    "${_lh_runtime_root}/src/lh_compiler/compiler.py"
    "${_lh_runtime_root}/src/lh_compiler/frontend/__init__.py"
    "${_lh_runtime_root}/src/lh_compiler/frontend/ast_builder.py"
    "${_lh_runtime_root}/src/lh_compiler/frontend/ast_nodes.py"
    "${_lh_runtime_root}/src/lh_compiler/backend/__init__.py"
    "${_lh_runtime_root}/src/lh_compiler/backend/codegen.py"
    "${_lh_runtime_root}/src/lh_compiler/backend/emitter.py"
    "${_lh_runtime_root}/src/lh_compiler/backend/memory.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/__init__.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/registry.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/__init__.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/_application.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/_comm.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/_compare.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/_constant.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/_control.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/_counter.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/_data.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/_display.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/_exca.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/_filter.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/_io.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/_logic.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/_math.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/_param.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/_pid.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/_safety.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/_system.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/_task.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/_timer.py"
    "${_lh_runtime_root}/src/lh_compiler/function_blocks/definitions/_tso.py"
)
foreach(_lh_required_file IN LISTS _lh_required_files)
    if(NOT EXISTS "${_lh_required_file}")
        message(FATAL_ERROR "安装布局缺少必要运行时文件: ${_lh_required_file}")
    endif()
endforeach()

file(GLOB_RECURSE _lh_runtime_entries LIST_DIRECTORIES true "${_lh_runtime_root}/*")
set(_lh_forbidden_entries)
foreach(_lh_entry IN LISTS _lh_runtime_entries)
    file(RELATIVE_PATH _lh_relative_entry "${_lh_runtime_root}" "${_lh_entry}")
    string(REPLACE "\\" "/" _lh_relative_entry "${_lh_relative_entry}")
    string(TOLOWER "${_lh_relative_entry}" _lh_lower_relative_entry)
    if(_lh_lower_relative_entry MATCHES "(^|/)(venv|__pycache__|tests|test_programs|examples|docs|scripts|compiled_output|output)(/|$)"
       OR _lh_lower_relative_entry MATCHES "\\.(pyc|md|docx|g4|interp|tokens|lh|bat|sh|toml|code|list|typ|rep)$")
        list(APPEND _lh_forbidden_entries "${_lh_entry}")
    endif()
endforeach()
if(_lh_forbidden_entries)
    list(JOIN _lh_forbidden_entries "\n" _lh_forbidden_report)
    message(FATAL_ERROR
        "安装布局包含禁止的开发/缓存/输出内容:\n${_lh_forbidden_report}"
    )
endif()

if(WIN32 AND LH_ENABLE_QT_DEPLOYMENT)
    foreach(_lh_plugin IN ITEMS
        "${_lh_install_prefix}/bin/platforms/qwindows${_lh_qt_plugin_suffix}.dll"
        "${_lh_install_prefix}/bin/sqldrivers/qsqlite${_lh_qt_plugin_suffix}.dll"
    )
        if(NOT EXISTS "${_lh_plugin}")
            message(FATAL_ERROR
                "Windows Qt 部署配置 '${_lh_install_config_label}'（模式 ${_lh_qt_deploy_mode}）"
                "缺少期望插件文件: ${_lh_plugin}"
            )
        endif()
    endforeach()
elseif(WIN32)
    message(STATUS "LH_ENABLE_QT_DEPLOYMENT=OFF，跳过 Windows Qt 插件断言；需由外部打包流程提供 Qt。")
else()
    message(STATUS "非 Windows 平台不执行 windeployqt 插件断言；Qt 动态库由平台打包流程提供。")
endif()

message(STATUS "安装布局测试通过: ${_lh_install_prefix}")
if(IS_SYMLINK "${_lh_install_prefix}")
    message(FATAL_ERROR
        "安装布局测试拒绝清理被替换为符号链接的测试前缀: ${_lh_install_prefix}"
    )
endif()
if(EXISTS "${_lh_install_prefix}" AND NOT IS_DIRECTORY "${_lh_install_prefix}")
    message(FATAL_ERROR
        "安装布局测试拒绝清理非目录测试前缀: ${_lh_install_prefix}"
    )
endif()
file(REMOVE_RECURSE "${_lh_install_prefix}")
