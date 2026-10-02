add_library(runtime_session_service STATIC
    RuntimeSessionService.cpp RuntimeSessionService.h RuntimeSessionTypes.h
    ParameterController.cpp ParameterController.h RunController.cpp RunController.h)
target_link_libraries(runtime_session_service PUBLIC Qt5::Core core_module ServoValveComm compiler_module)
target_include_directories(runtime_session_service PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> $<INSTALL_INTERFACE:include/runtime> ${LX_PUBLIC_INCLUDE_DIRS}
    PRIVATE ${LX_PRIVATE_INCLUDE_DIRS})
target_compile_features(runtime_session_service PUBLIC cxx_std_17)
