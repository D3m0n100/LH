cmake_minimum_required(VERSION 3.16)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network SerialPort SerialBus)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network SerialPort SerialBus)

set(COMM_SOURCES
    CANInterface.cpp
    CANOpenInterface.cpp
    J1939Interface.cpp
    SerialInterface.cpp
    ModbusInterface.cpp
    CommFactory.cpp
    ProtocolDetector.cpp
    DownloadManager.cpp
    EthernetInterface.cpp
    ControllerBridge.cpp
    DownloadProfile.cpp
    VirtualDeviceBackend.cpp
    ClassicOpcServer.cpp
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
    DownloadProfile.h
    IDownloadTransport.h
    IDeviceBackend.h
    VirtualDeviceBackend.h
    IOpcServer.h
    ClassicOpcServer.h
    OpcServerFactory.h
)

add_library(ServoValveComm STATIC
    ${COMM_SOURCES}
    ${COMM_HEADERS}
)

target_include_directories(ServoValveComm
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ServoValveComm
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::SerialPort
        Qt${QT_VERSION_MAJOR}::SerialBus
)

target_compile_features(ServoValveComm PUBLIC cxx_std_17)

set_target_properties(ServoValveComm PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

install(TARGETS ServoValveComm
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ${COMM_HEADERS}
    DESTINATION include/communication
)

option(BUILD_SHARED_LIBS "Build shared library" OFF)
if(BUILD_SHARED_LIBS)
    add_library(ServoValveCommShared SHARED
        ${COMM_SOURCES}
        ${COMM_HEADERS}
    )

    target_include_directories(ServoValveCommShared
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(ServoValveCommShared
        PUBLIC
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Network
            Qt${QT_VERSION_MAJOR}::SerialPort
            Qt${QT_VERSION_MAJOR}::SerialBus
    )

    set_target_properties(ServoValveCommShared PROPERTIES
        AUTOMOC ON
        OUTPUT_NAME ServoValveComm
    )
endif()
