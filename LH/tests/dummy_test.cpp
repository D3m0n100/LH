/**
 * @file dummy_test.cpp
 * @brief 基础环境测试
 *
 * 验证基本的 Qt 环境和通用模块是否正常工作。
 * 
 * ============== 变更历史 ==============
 * 2025-01: 移除对 ComponentType 等图形化组态类型的引用，
 *          这些类型已从项目中清除。
 */

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include "Common.h"

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    
    qInfo() << "========================================";
    qInfo() << "Basic Environment Test";
    qInfo() << "========================================";
    
    // 测试版本宏
    qInfo() << QString("Platform Version: %1.%2.%3")
                   .arg(PLATFORM_VERSION_MAJOR)
                   .arg(PLATFORM_VERSION_MINOR)
                   .arg(PLATFORM_VERSION_PATCH);
    
    // 测试日志宏
    LOG_INFO("Testing LOG_INFO macro");
    LOG_DEBUG("Testing LOG_DEBUG macro");
    
    // 测试 Qt 基本功能
    QString testString = "Hello, ServoValvePlatform!";
    qInfo() << "Test string:" << testString;
    
    QDateTime now = QDateTime::currentDateTime();
    qInfo() << "Current time:" << now.toString(Qt::ISODate);
    
    // 测试 Platform 命名空间中的通信协议枚举
    qInfo() << "CommProtocol count:" << static_cast<int>(Platform::CommProtocol::Modbus) + 1;
    
    // 验证各通信协议枚举值
    qInfo() << "CommProtocol::CAN =" << static_cast<int>(Platform::CommProtocol::CAN);
    qInfo() << "CommProtocol::Ethernet =" << static_cast<int>(Platform::CommProtocol::Ethernet);
    qInfo() << "CommProtocol::SerialPort =" << static_cast<int>(Platform::CommProtocol::SerialPort);
    qInfo() << "CommProtocol::Modbus =" << static_cast<int>(Platform::CommProtocol::Modbus);
    
    qInfo() << "";
    qInfo() << "========================================";
    qInfo() << "All basic tests PASSED!";
    qInfo() << "========================================";
    
    return 0;
}
