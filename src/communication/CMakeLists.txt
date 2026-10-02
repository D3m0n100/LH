cmake_minimum_required(VERSION 3.16)

set(COMM_SOURCES
    CANInterface.cpp
    CANOpenInterface.cpp
    J1939Interface.cpp
    SerialInterface.cpp
    ModbusInterface.cpp
    DownloadManager.cpp
    EthernetInterface.cpp
    ControllerBridge.cpp
    ControllerDeviceBackend.cpp
    ControllerDeviceBackendDebug.cpp
    ControllerDeviceBackendDiagnostics.cpp
    ControllerDeviceBackendDownload.cpp
    ControllerDeviceBackendPoints.cpp
    ControllerDebugClient.cpp
    ControllerDebugProtocol.cpp
    RuntimePointRegisterCodec.cpp
    VirtualDeviceBackend.cpp
    ClassicOpcServer.cpp
    ClassicOpcPollWorker.cpp
    MatrikonOpcServer.cpp
    AsyncOpcServer.cpp
    OpcServerFactory.cpp
)

set(COMM_HEADERS
    Common.h
    CommTypes.h
    ICommInterface.h
    CANCommon.h
    CANInterface.h
    CANOpenInterface.h
    J1939Interface.h
    SerialInterface.h
    ModbusInterface.h
    EthernetInterface.h
    Communication.h
    ControllerBridge.h
    ControllerDeviceBackend.h
    ControllerDebugClient.h
    ControllerDebugProtocol.h
    RuntimePointRegisterCodec.h
    DownloadProfile.h
    IDownloadTransport.h
    IDeviceBackend.h
    VirtualDeviceBackend.h
    IOpcServer.h
    ClassicOpcServer.h
    MatrikonOpcServer.h
    AsyncOpcServer.h
    OpcWriteResultState.h
    OpcServerFactory.h
    RuntimePointQualityMapper.h
    ModbusLimits.h
)

add_library(download_profile_contract STATIC DownloadProfile.cpp DownloadProfile.h ModbusLimits.h)
target_include_directories(download_profile_contract PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> $<INSTALL_INTERFACE:include/communication>)
target_link_libraries(download_profile_contract PUBLIC Qt5::Core)
target_compile_features(download_profile_contract PUBLIC cxx_std_17)

add_library(ServoValveComm STATIC
    ${COMM_SOURCES}
    ${COMM_HEADERS}
)

target_include_directories(ServoValveComm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> $<INSTALL_INTERFACE:include/communication>
        ${LX_PUBLIC_INCLUDE_DIRS}
)

target_link_libraries(ServoValveComm
    PUBLIC
        download_profile_contract
        Qt5::Core
        Qt5::Network
        Qt5::SerialPort
        Qt5::SerialBus
)

if(WIN32)
    target_link_libraries(ServoValveComm PUBLIC ole32 oleaut32)
endif()

target_compile_features(ServoValveComm PUBLIC cxx_std_17)

set_target_properties(ServoValveComm PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

install(TARGETS ServoValveComm download_profile_contract
    EXPORT LHTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ${COMM_HEADERS}
    DESTINATION include/communication
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/common/
    DESTINATION include/common FILES_MATCHING PATTERN "*.h" PATTERN "ProjectMutationGuard.h" EXCLUDE)
install(FILES ${CMAKE_SOURCE_DIR}/include/Common.h DESTINATION include)

target_include_directories(ServoValveComm PRIVATE ${LX_PRIVATE_INCLUDE_DIRS})
